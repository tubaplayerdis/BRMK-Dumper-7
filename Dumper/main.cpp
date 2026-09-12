#include <Windows.h>
#include <iostream>
#include <chrono>
#include <fstream>

#include "Generators/CppGenerator.h"
#include "Generators/MappingGenerator.h"
#include "Generators/IDAMappingGenerator.h"
#include "Generators/DumpspaceGenerator.h"

#include "Generators/Generator.h"

class TeeBuffer : public std::streambuf {
public:
	TeeBuffer(std::streambuf* sb1, std::streambuf* sb2) : sb1(sb1), sb2(sb2) {}

protected:
	virtual int overflow(int c) override {
		if (c == EOF) {
			return !EOF;
		} else {
			int const r1 = sb1->sputc(c);
			int const r2 = sb2->sputc(c);
			return (r1 == EOF || r2 == EOF) ? EOF : c;
		}
	}

	virtual int sync() override {
		int const r1 = sb1->pubsync();
		int const r2 = sb2->pubsync();
		return (r1 == 0 && r2 == 0) ? 0 : -1;
	}

private:
	std::streambuf* sb1;
	std::streambuf* sb2;
};


enum class EFortToastType : uint8
{
    Default                                  = 0,
    Subdued                                  = 1,
    Impactful                                = 2,
    Lock                                     = 3,
    EFortToastType_MAX                       = 4,
};

DWORD MainThread(HMODULE Module)
{
	AllocConsole();
	FILE* Dummy;
	freopen_s(&Dummy, "CONIN$", "r", stdin);
	freopen_s(&Dummy, "CONOUT$", "w", stdout);
	freopen_s(&Dummy, "CONOUT$", "w", stderr);
	std::cerr.clear(); // clear internal error flags on cerr after redirect
	std::cerr << std::boolalpha << std::hex;

	// 1. Open your log file
	std::ofstream fileOut("output.txt");

	// 2. Create the tee buffer, passing it the console buffer and the file buffer
	TeeBuffer teeBuffer(std::cerr.rdbuf(), fileOut.rdbuf());

	// 3. Save the original cout buffer so we can restore it later
	std::streambuf* oldCoutBuffer = std::cerr.rdbuf();

	// 4. Redirect std::cout to use our custom tee buffer
	std::cerr.rdbuf(&teeBuffer);

	std::cerr << "Initializing [Dumper-7]\n";

	Settings::Config::Load();
	Settings::Config::DelayDumperStart();

	std::cerr << "Started Generation [Dumper-7]!\n";
	auto DumpStartTime = std::chrono::high_resolution_clock::now();

	Generator::InitEngineCore();
	Generator::InitInternal();

	if (Settings::Generator::GameName.empty() && Settings::Generator::GameVersion.empty())
	{
		// Only Possible in Main()
		FString Name;
		FString Version;
		UEClass Kismet = ObjectArray::FindClassFast("KismetSystemLibrary");
		UEFunction GetGameName = Kismet.GetFunction("KismetSystemLibrary", "GetGameName");
		UEFunction GetEngineVersion = Kismet.GetFunction("KismetSystemLibrary", "GetEngineVersion");

		Kismet.ProcessEvent(GetGameName, &Name);
		Kismet.ProcessEvent(GetEngineVersion, &Version);

		Settings::Generator::GameName = Name.ToString();
		Settings::Generator::GameVersion = Version.ToString();
	}

	std::cerr << "GameName: " << Settings::Generator::GameName << "\n";
	std::cerr << "GameVersion: " << Settings::Generator::GameVersion << "\n\n";

	std::cerr << "FolderName: " << (Settings::Generator::GameVersion + '-' + Settings::Generator::GameName) << "\n\n";

	Generator::Generate<CppGenerator>();
	Generator::Generate<MappingGenerator>();
	Generator::Generate<IDAMappingGenerator>();
	Generator::Generate<DumpspaceGenerator>();

	auto DumpFinishTime = std::chrono::high_resolution_clock::now();

	std::chrono::duration<double, std::milli> DumpTime = DumpFinishTime - DumpStartTime;

	std::cerr << "\n\nGenerating SDK took (" << DumpTime.count() << "ms)\n\n\n";

	if (Settings::Debug::bExecuteSDKTestScript)
	{
		/* Executes a python script to test if the SDK compiles correctly. */
		CppGenerator::ExecuteSDKCompilationTestScript();
	}

	std::cerr << "\n\nPress F6 to unload\n\n\n";

	while (true)
	{
		if (GetAsyncKeyState(VK_F6) & 1)
		{
			fclose(stderr);
			if (Dummy) 
			{
				fclose(Dummy);
			}
			FreeConsole();

			FreeLibraryAndExitThread(Module, 0);
		}

		Sleep(100);
	}

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
		break;
	}

	return TRUE;
}
