// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/win/pe_image.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DelayloadsTest : public testing::Test {
 protected:
  static bool ImportsCallback(const base::win::PEImage& image,
                              LPCSTR module,
                              PIMAGE_THUNK_DATA name_table,
                              PIMAGE_THUNK_DATA iat,
                              PVOID cookie) {
    std::vector<std::string>* import_list =
        reinterpret_cast<std::vector<std::string>*>(cookie);
    import_list->push_back(module);
    return true;
  }

  static void GetImports(const base::FilePath& module_path,
                         std::vector<std::string>* imports) {
    ASSERT_TRUE(imports != NULL);

    base::MemoryMappedFile module_mmap;

    ASSERT_TRUE(module_mmap.Initialize(module_path));
    base::win::PEImageAsData pe_image_data(
        reinterpret_cast<HMODULE>(const_cast<uint8_t*>(module_mmap.data())));
    pe_image_data.EnumImportChunks(DelayloadsTest::ImportsCallback, imports,
                                   nullptr);
  }
};

// Run this test only in Release builds.
//
// These tests make sure that chrome.dll, chrome_child.dll, and chrome.exe
// have only certain types of imports.
// In particular, we explicitly want to ensure user32.dll and its many related
// dlls are delayloaded and not automatically brought in via some other
// dependent dll. The primary reason for this is that the sandbox for the
// renderer process prevents user32 from working at all and therefore we have
// no reason to load the dll.
// However, they directly and indirectly depend on base, which has lots more
// imports than are allowed here.
//
// In release builds, the offending imports are all stripped since this
// depends on a relatively small portion of base.
//
// If you break these tests, you may have changed base or the Windows sandbox
// such that more system imports are required to link.
//
// Also note that the dlls are listed with specific case-sensitive names. If
// you fail a test double-check that casing of the name.
#if defined(NDEBUG) && !defined(COMPONENT_BUILD)

TEST_F(DelayloadsTest, ChromeDllDelayloadsCheck) {
  base::FilePath dll;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &dll));
  dll = dll.Append(L"chrome.dll");
  std::vector<std::string> dll_imports;
  GetImports(dll, &dll_imports);

  // Check that the dll has imports.
  ASSERT_LT(0u, dll_imports.size())
      << "Ensure the delayloads_unittests "
         "target was built, instead of delayloads_unittests.exe";

  static const char* const kValidFilePatterns[] = {
    "KERNEL32.dll",
    "chrome_elf.dll",
#if !defined(CHROME_MULTIPLE_DLL_BROWSER)
    "DWrite.dll",
    "ADVAPI32.dll",
    "CRYPT32.dll",
    "dbghelp.dll",
    "dhcpcsvc.DLL",
    "IPHLPAPI.DLL",
    "ntdll.dll",
    "OLEAUT32.dll",
    "Secur32.dll",
    "UIAutomationCore.DLL",
    "USERENV.dll",
    "WINHTTP.dll",
    "WINMM.dll",
    "WINSPOOL.DRV",
    "WINTRUST.dll",
    "WS2_32.dll",
#endif  //  CHROME_MULTIPLE_DLL_BROWSER
    // On 64 bit the Version API's like VerQueryValue come from VERSION.dll.
    // It depends on kernel32, advapi32 and api-ms-win-crt*.dll. This should
    // be ok.
    "VERSION.dll",
  };

  // Make sure all of chrome.dll's imports are in the valid imports list.
  for (const std::string& dll_import : dll_imports) {
    bool match = false;
    for (const char* kValidFilePattern : kValidFilePatterns) {
      if (base::MatchPattern(dll_import, kValidFilePattern)) {
        match = true;
        break;
      }
    }
    EXPECT_TRUE(match) << "Illegal import in chrome.dll: " << dll_import;
  }
}

TEST_F(DelayloadsTest, ChromeDllLoadSanityTest) {
  // As a precaution to avoid affecting other tests, we need to ensure this is
  // executed in its own test process. This "test" will re-launch with custom
  // parameters to accomplish that.
  base::CommandLine new_test =
      base::CommandLine(base::CommandLine::ForCurrentProcess()->GetProgram());
  new_test.AppendSwitchASCII(
      base::kGTestFilterFlag,
      "DelayloadsTest.DISABLED_ChromeDllLoadSanityTestImpl");
  new_test.AppendSwitch("gtest_also_run_disabled_tests");
  new_test.AppendSwitch("single-process-tests");

  std::string output;
  ASSERT_TRUE(base::GetAppOutput(new_test, &output));
  std::string crash_string =
      "OK ] DelayloadsTest.DISABLED_ChromeDllLoadSanityTestImpl";

  if (output.find(crash_string) == std::string::npos) {
    GTEST_FAIL() << "Couldn't find\n"
                 << crash_string << "\n in output\n " << output;
  }
}

// Note: This test is not actually disabled, it's just tagged disabled so that
// the real run (above, in ChromeDllLoadSanityTest) can run it with an argument
// added to the command line.
TEST_F(DelayloadsTest, DISABLED_ChromeDllLoadSanityTestImpl) {
  base::FilePath dll;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &dll));
  dll = dll.Append(L"chrome.dll");

  // We don't expect user32 to be loaded in delayloads_unittests. If this
  // test case fails, then it means that a dependency on user32 has crept into
  // the delayloads_unittests executable, which needs to be removed.
  // NOTE: it may be a secondary dependency of another system DLL.  If so,
  // try adding a "/DELAYLOAD:<blah>.dll" to the build.gn file.
  ASSERT_EQ(nullptr, ::GetModuleHandle(L"user32.dll"));

  HMODULE chrome_module_handle = ::LoadLibrary(dll.value().c_str());
  ASSERT_TRUE(chrome_module_handle != nullptr);

#if defined(CHROME_MULTIPLE_DLL_BROWSER)
  // Loading chrome.dll should not load user32.dll.
  EXPECT_EQ(nullptr, ::GetModuleHandle(L"user32.dll"));
#else
  // Loading chrome.dll should not load user32.dll on Win10.
  // On Win7, chains of system dlls and lack of apisets result in it loading.
  if (base::win::GetVersion() >= base::win::Version::WIN10) {
    EXPECT_EQ(nullptr, ::GetModuleHandle(L"user32.dll"));
  } else {
    EXPECT_NE(nullptr, ::GetModuleHandle(L"user32.dll"));
  }
#endif  // CHROME_MULTIPLE_DLL_BROWSER
}

#if defined(CHROME_MULTIPLE_DLL_BROWSER)

TEST_F(DelayloadsTest, ChromeChildDllDelayloadsCheck) {
  base::FilePath dll;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &dll));
  dll = dll.Append(L"chrome_child.dll");
  std::vector<std::string> dll_imports;
  GetImports(dll, &dll_imports);

  // Check that the dll has imports.
  ASSERT_LT(0u, dll_imports.size())
      << "Ensure the delayloads_unittests "
         "target was built, instead of delayloads_unittests.exe";

  static const char* const kValidFilePatterns[] = {
      "KERNEL32.dll",
      "chrome_elf.dll",
      "DWrite.dll",
      "ADVAPI32.dll",
      "CRYPT32.dll",
      "dbghelp.dll",
      "dhcpcsvc.DLL",
      "IPHLPAPI.DLL",
      "ntdll.dll",
      "OLEAUT32.dll",
      "Secur32.dll",
      "UIAutomationCore.DLL",
      "USERENV.dll",
      "WINHTTP.dll",
      "WINMM.dll",
      "WINSPOOL.DRV",
      "WINTRUST.dll",
      "WS2_32.dll",
      // On 64 bit the Version API's like VerQueryValue come from VERSION.dll.
      // It depends on kernel32, advapi32 and api-ms-win-crt*.dll. This should
      // be ok.
      "VERSION.dll",
  };

  // Make sure all of chrome_child.dll's imports are in the valid imports list.
  for (const std::string& dll_import : dll_imports) {
    bool match = false;
    for (const char* kValidFilePattern : kValidFilePatterns) {
      if (base::MatchPattern(dll_import, kValidFilePattern)) {
        match = true;
        break;
      }
    }
    EXPECT_TRUE(match) << "Illegal import in chrome_child.dll: " << dll_import;
  }
}

TEST_F(DelayloadsTest, ChromeChildDllLoadSanityTest) {
  // On Win7 we expect this test to result in user32.dll getting loaded. As a
  // result, we need to ensure it is executed in its own test process. This
  // "test" will re-launch with custom parameters to accomplish that.
  base::CommandLine new_test =
      base::CommandLine(base::CommandLine::ForCurrentProcess()->GetProgram());
  new_test.AppendSwitchASCII(
      base::kGTestFilterFlag,
      "DelayloadsTest.DISABLED_ChromeChildDllLoadSanityTestImpl");
  new_test.AppendSwitch("gtest_also_run_disabled_tests");
  new_test.AppendSwitch("single-process-tests");

  std::string output;
  ASSERT_TRUE(base::GetAppOutput(new_test, &output));
  std::string crash_string =
      "OK ] DelayloadsTest.DISABLED_ChromeChildDllLoadSanityTestImpl";

  if (output.find(crash_string) == std::string::npos) {
    GTEST_FAIL() << "Couldn't find\n"
                 << crash_string << "\n in output\n " << output;
  }
}

// Note: This test is not actually disabled, it's just tagged disabled so that
// the real run (above, in ChromeChildDllLoadSanityTest) can run it with an
// argument added to the command line.
TEST_F(DelayloadsTest, DISABLED_ChromeChildDllLoadSanityTestImpl) {
  base::FilePath dll;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &dll));
  dll = dll.Append(L"chrome_child.dll");

  // We don't expect user32 to be loaded in delayloads_unittests. If this
  // test case fails, then it means that a dependency on user32 has crept into
  // the delayloads_unittests executable, which needs to be removed.
  // NOTE: it may be a secondary dependency of another system DLL.  If so,
  // try adding a "/DELAYLOAD:<blah>.dll" to the build.gn file.
  ASSERT_EQ(nullptr, ::GetModuleHandle(L"user32.dll"));

  HMODULE chrome_child_module_handle = ::LoadLibrary(dll.value().c_str());
  ASSERT_TRUE(chrome_child_module_handle != nullptr);
  // Loading chrome_child.dll should not load user32.dll on Win10.
  // On Win7, chains of system dlls and lack of apisets result in it loading.
  if (base::win::GetVersion() >= base::win::Version::WIN10) {
    EXPECT_EQ(nullptr, ::GetModuleHandle(L"user32.dll"));
  } else {
    EXPECT_NE(nullptr, ::GetModuleHandle(L"user32.dll"));
  }
}

#endif  // CHROME_MULTIPLE_DLL_BROWSER

TEST_F(DelayloadsTest, ChromeElfDllDelayloadsCheck) {
  base::FilePath dll;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &dll));
  dll = dll.Append(L"chrome_elf.dll");
  std::vector<std::string> dll_imports;
  GetImports(dll, &dll_imports);

  // Check that the dll has imports.
  ASSERT_LT(0u, dll_imports.size())
      << "Ensure the delayloads_unittests "
         "target was built, instead of delayloads_unittests.exe";

  static const char* const kValidFilePatterns[] = {
    "KERNEL32.dll",
#if defined(ADDRESS_SANITIZER) && defined(COMPONENT_BUILD)
    "clang_rt.asan_dynamic-i386.dll",
#endif
    // On 64 bit the Version API's like VerQueryValue come from VERSION.dll.
    // It depends on kernel32, advapi32 and api-ms-win-crt*.dll. This should
    // be ok.
    "VERSION.dll",
  };

  // Make sure all of ELF's imports are in the valid imports list.
  for (const std::string& dll_import : dll_imports) {
    bool match = false;
    for (const char* kValidFilePattern : kValidFilePatterns) {
      if (base::MatchPattern(dll_import, kValidFilePattern)) {
        match = true;
        break;
      }
    }
    ASSERT_TRUE(match) << "Illegal import in chrome_elf.dll: " << dll_import;
  }
}

TEST_F(DelayloadsTest, ChromeElfDllLoadSanityTest) {
  // chrome_elf will try to launch crashpad_handler by reinvoking the current
  // binary with --type=crashpad-handler if not already running that way. To
  // avoid that, we relaunch and run the real test body manually, adding that
  // command line argument, as we're only trying to confirm that user32.dll
  // doesn't get loaded by import table when chrome_elf.dll does.
  base::CommandLine new_test =
      base::CommandLine(base::CommandLine::ForCurrentProcess()->GetProgram());
  new_test.AppendSwitchASCII(
      base::kGTestFilterFlag,
      "DelayloadsTest.DISABLED_ChromeElfDllLoadSanityTestImpl");
  new_test.AppendSwitchASCII("type", "crashpad-handler");
  new_test.AppendSwitch("gtest_also_run_disabled_tests");
  new_test.AppendSwitch("single-process-tests");

  std::string output;
  ASSERT_TRUE(base::GetAppOutput(new_test, &output));
  std::string crash_string =
      "OK ] DelayloadsTest.DISABLED_ChromeElfDllLoadSanityTestImpl";

  if (output.find(crash_string) == std::string::npos) {
    GTEST_FAIL() << "Couldn't find\n"
                 << crash_string << "\n in output\n " << output;
  }
}

// Note: This test is not actually disabled, it's just tagged disabled so that
// the real run (above, in ChromeElfDllLoadSanityTest) can run it with an
// argument added to the command line.
TEST_F(DelayloadsTest, DISABLED_ChromeElfDllLoadSanityTestImpl) {
  base::FilePath dll;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &dll));
  dll = dll.Append(L"chrome_elf.dll");

  // We don't expect user32 to be loaded in delayloads_unittests. If this
  // test case fails, then it means that a dependency on user32 has crept into
  // the delayloads_unittests executable, which needs to be removed.
  // NOTE: it may be a secondary dependency of another system DLL.  If so,
  // try adding a "/DELAYLOAD:<blah>.dll" to the build.gn file.
  ASSERT_EQ(nullptr, ::GetModuleHandle(L"user32.dll"));

  HMODULE chrome_elf_module_handle = ::LoadLibrary(dll.value().c_str());
  ASSERT_TRUE(chrome_elf_module_handle != nullptr);
  // Loading chrome_elf.dll should not load user32.dll
  EXPECT_EQ(nullptr, ::GetModuleHandle(L"user32.dll"));
  // Note: Do not unload the chrome_elf DLL in any test where the elf hook has
  // been applied (browser process type only).  This results in the shim code
  // disappearing, but ntdll hook remaining, followed in tests by fireworks.
  EXPECT_TRUE(!!::FreeLibrary(chrome_elf_module_handle));
}

TEST_F(DelayloadsTest, ChromeExeDelayloadsCheck) {
  std::vector<std::string> exe_imports;
  base::FilePath exe;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe));
  exe = exe.Append(L"chrome.exe");
  GetImports(exe, &exe_imports);

  // Check that chrome.exe has imports.
  ASSERT_LT(0u, exe_imports.size())
      << "Ensure the delayloads_unittests "
         "target was built, instead of delayloads_unittests.exe";

  static const char* const kValidFilePatterns[] = {
      "KERNEL32.dll",
      "chrome_elf.dll",
      // On 64 bit the Version API's like VerQueryValue come from VERSION.dll.
      // It depends on kernel32, advapi32 and api-ms-win-crt*.dll. This should
      // be ok.
      "VERSION.dll",
  };

  // Make sure all of chrome.exe's imports are in the valid imports list.
  for (const std::string& exe_import : exe_imports) {
    bool match = false;
    for (const char* kValidFilePattern : kValidFilePatterns) {
      if (base::MatchPattern(exe_import, kValidFilePattern)) {
        match = true;
        break;
      }
    }
    EXPECT_TRUE(match) << "Illegal import in chrome.exe: " << exe_import;
  }
}

#endif  // NDEBUG && !COMPONENT_BUILD

TEST_F(DelayloadsTest, ChromeExeLoadSanityCheck) {
  std::vector<std::string> exe_imports;

  base::FilePath exe;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe));
  exe = exe.Append(L"chrome.exe");
  GetImports(exe, &exe_imports);

  // Check that chrome.exe has imports.
  ASSERT_LT(0u, exe_imports.size())
      << "Ensure the delayloads_unittests "
         "target was built, instead of delayloads_unittests.exe";

  // Chrome.exe's first import must be ELF.
  EXPECT_EQ("chrome_elf.dll", exe_imports[0])
      << "Illegal import order in chrome.exe (ensure the "
         "delayloads_unittests "
         "target was built, instead of just delayloads_unittests.exe)";
}

}  // namespace

int main(int argc, char** argv) {
  // Ensure that the CommandLine instance honors the command line passed in
  // instead of the default behavior on Windows which is to use the shell32
  // CommandLineToArgvW API. The delayloads_unittests test suite should
  // not depend on user32 directly or indirectly (For the curious shell32
  // depends on user32)
  base::CommandLine::InitUsingArgvForTesting(argc, argv);

  install_static::ScopedInstallDetails scoped_install_details;

  base::TestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
