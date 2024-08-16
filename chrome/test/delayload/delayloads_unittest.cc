// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/win/pe_image.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "mojo/buildflags.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using DetailedImports = std::map<std::string, std::set<std::string>>;
using SupportedApiSets = std::map<std::string, size_t>;

// Generated static data - see `generate_allowed_imports.py` - module must be
// lowercase as we force imports to lowercase when we read the module.
// e.g. const DetailedImports kAvailableImports = {
//     {"kernel32.dll", {"Function1", "Function2"}}};
const DetailedImports kAvailableImports = {
#include "chrome/test/delayload/supported_imports.inc"
};

// Highest min version of each ApiSet that is allowed - this file is checked in
// but can be regenerated using `generate_allowed_apisets.py`.
// e.g. { "api-ms-win-core-synch-l1-2", 2}
// allows "api-ms-win-core-synch-l1-2-1.dll"
const SupportedApiSets kSupportedApiSets = {
#include "chrome/test/delayload/supported_apisets_10.0.10240.inc"
};

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

  static std::vector<std::string> GetImports(
      const base::FilePath& module_path) {
    std::vector<std::string> imports;

    base::MemoryMappedFile module_mmap;
    CHECK(module_mmap.Initialize(module_path));

    base::win::PEImageAsData pe_image_data(
        reinterpret_cast<HMODULE>(const_cast<uint8_t*>(module_mmap.data())));
    pe_image_data.EnumImportChunks(DelayloadsTest::ImportsCallback, &imports,
                                   nullptr);
    return imports;
  }
};

// Tests using this fixture validate that imports are available on the earliest
// supported Windows version.
class MinimumWindowsSupportTest : public DelayloadsTest {
 protected:
  // Internal modules that are expected to be imported by modules under test.
  void SetExtraAllowedImports(std::set<std::string> internal_modules) {
    modules_ = internal_modules;
  }

  void Validate(const std::wstring& module) {
    base::FilePath module_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &module_path));
    ValidateImportsForEarliestWindowsVersion(module_path.Append(module),
                                             modules_);
  }

  // Validate ApiSet is supported.
  //  `import` - a normal import like 'chrome_elf.dll' or an ApiSet like
  //     'api-ms-win-core-synch-l1-2-0.dll'.
  // ApiSets are supported if the (name, major, minor) match, and the requested
  // subversion is <= the supported subversion.
  static bool SupportedApiSet(const std::string& import) {
    if (!base::StartsWith(import, "api-") || !base::EndsWith(import, ".dll")) {
      return false;
    }
    // Need at least api-{components}-l1-1-0.dll so > four dashes.
    if (base::ranges::count(import, '-') < 5) {
      return false;
    }

    // Want (requested_api: api-{components}-l1-1) and (requested_version: 0).
    const size_t last_dash = import.rfind("-");
    const size_t dot = import.rfind(".");
    size_t requested_version = 0;
    const auto requested_version_str =
        import.substr(last_dash + 1, dot - last_dash - 1);
    if (!base::StringToSizeT(requested_version_str, &requested_version)) {
      return false;
    }

    const auto requested_api = import.substr(0, last_dash);
    const auto api_to_max_ver = kSupportedApiSets.find(requested_api);
    if (api_to_max_ver == kSupportedApiSets.end()) {
      // ApiSet not supported.
      return false;
    }

    return requested_version <= api_to_max_ver->second;
  }

  static bool DetailedImportsCallback(const base::win::PEImage& image,
                                      const char* module,
                                      DWORD ordinal,
                                      const char* import_name,
                                      DWORD hint,
                                      IMAGE_THUNK_DATA* iat,
                                      void* cookie) {
    if (!module) {
      return false;
    }
    if (!import_name) {
      return true;
    }
    // Force module name to lowercase here.
    const std::string mod_str = base::ToLowerASCII(module);
    DetailedImports* imports = reinterpret_cast<DetailedImports*>(cookie);
    if (auto fn_names = imports->find(mod_str); fn_names != imports->end()) {
      fn_names->second.emplace(import_name);
    } else {
      std::set<std::string> empty_fn_names;
      empty_fn_names.emplace(import_name);
      imports->emplace(std::move(mod_str), std::move(empty_fn_names));
    }
    return true;
  }

  static DetailedImports GetDetailedImports(const base::FilePath& module_path) {
    base::MemoryMappedFile module_mmap;
    DetailedImports imports;

    CHECK(module_mmap.Initialize(module_path));
    base::win::PEImageAsData pe_image_data(
        reinterpret_cast<HMODULE>(const_cast<uint8_t*>(module_mmap.data())));
    pe_image_data.EnumAllImports(
        MinimumWindowsSupportTest::DetailedImportsCallback, &imports, nullptr);
    return imports;
  }

  // Helper so that we can validate the checker in a little test.
  static void AreImportsOk(DetailedImports& imports,
                           const std::set<std::string>& extra_imports) {
    for (const auto& imports_entry : imports) {
      const std::string& module = imports_entry.first;
      auto available_functions = kAvailableImports.find(module);
      if (available_functions == kAvailableImports.end()) {
        // Unlisted modules must be provided by the Chrome build or be a
        // supported apiset.
        if (!SupportedApiSet(module)) {
          EXPECT_THAT(extra_imports, testing::Contains(module));
        }
      } else {
        // Imported functions must be available in earliest Windows version.
        for (const auto& function : imports_entry.second) {
          EXPECT_THAT(available_functions->second, testing::Contains(function));
        }
      }
    }
  }

  // Validate that any static (non-delayloaded) imported functions are available
  // in the earliest version of Windows that Chrome supports. If an unsupported
  // function is added to Chrome's imports Chrome and its crash reporting client
  // may fail to start.
  // `mod_path` - exe or dll (e.g. chrome.exe) to check.
  // `extra_imports` - modules from the build (e.g. chrome.exe can import
  // chrome_elf.dll or a supported apiset dll).
  static void ValidateImportsForEarliestWindowsVersion(
      const base::FilePath& mod_path,
      const std::set<std::string>& extra_imports) {
    DetailedImports imports = GetDetailedImports(mod_path);
    AreImportsOk(imports, extra_imports);
  }

  std::set<std::string> modules_;
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
  std::vector<std::string> dll_imports = GetImports(dll);

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

// Flaking on ASAN: https://crbug.com/1047723
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ChromeDllLoadSanityTest DISABLED_ChromeDllLoadSanityTest
#else
#define MAYBE_ChromeDllLoadSanityTest ChromeDllLoadSanityTest
#endif
TEST_F(DelayloadsTest, MAYBE_ChromeDllLoadSanityTest) {
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

  // Loading chrome.dll should not load user32.dll on Windows.
  EXPECT_EQ(nullptr, ::GetModuleHandle(L"user32.dll"));
}

TEST_F(DelayloadsTest, ChromeElfDllDelayloadsCheck) {
  base::FilePath dll;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &dll));
  dll = dll.Append(L"chrome_elf.dll");
  std::vector<std::string> dll_imports = GetImports(dll);

  // Check that the dll has imports.
  ASSERT_LT(0u, dll_imports.size())
      << "Ensure the delayloads_unittests "
         "target was built, instead of delayloads_unittests.exe";

  // Allowlist of modules that do not delayload.
  static const char* const kValidFilePatterns[] = {
    "KERNEL32.dll",
    "ntdll.dll",
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
  EXPECT_TRUE(::FreeLibrary(chrome_elf_module_handle));
}

TEST_F(DelayloadsTest, ChromeExeDelayloadsCheck) {
  base::FilePath exe;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe));
  exe = exe.Append(L"chrome.exe");
  std::vector<std::string> exe_imports = GetImports(exe);

  // Check that chrome.exe has imports.
  ASSERT_LT(0u, exe_imports.size())
      << "Ensure the delayloads_unittests "
         "target was built, instead of delayloads_unittests.exe";

  // Allowlist of modules that do not delayload.
  static const char* const kValidFilePatterns[] = {
    "KERNEL32.dll",
    "ntdll.dll",
    "chrome_elf.dll",
    // On 64 bit the Version API's like VerQueryValue come from VERSION.dll.
    // It depends on kernel32, advapi32 and api-ms-win-crt*.dll. This should
    // be ok.
    "VERSION.dll",
#if defined(ADDRESS_SANITIZER)
    // The ASan runtime uses the synchapi (see crbug.com/1236586).
    "api-ms-win-core-synch-l1-2-0.dll",
#endif
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

TEST_F(MinimumWindowsSupportTest, ChromeElf) {
  Validate(L"chrome_elf.dll");
}

TEST_F(MinimumWindowsSupportTest, ChromeWer) {
  Validate(L"chrome_wer.dll");
}

TEST_F(MinimumWindowsSupportTest, ChromeExe) {
  SetExtraAllowedImports({"chrome_elf.dll"});
  Validate(L"chrome.exe");
}

TEST_F(MinimumWindowsSupportTest, ChromeDll) {
  SetExtraAllowedImports({"chrome_elf.dll"});
  Validate(L"chrome.dll");
}

TEST_F(MinimumWindowsSupportTest, ChromeExtraDlls) {
  std::vector<std::wstring> extra_dlls = {
      L"d3dcompiler_47.dll",
#if !defined(ARCH_CPU_ARM64)
      // These are not yet supported for Arm64.
      L"dxcompiler.dll", L"dxil.dll",
#endif  // !defined(ARCH_CPU_ARM64
      L"libEGL.dll", L"libGLESv2.dll", L"vk_swiftshader.dll", L"vulkan-1.dll"};
  for (const auto& dll : extra_dlls) {
    Validate(dll);
  }
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(MinimumWindowsSupportTest, ChromeOptimizationGuide) {
  Validate(L"optimization_guide_internal.dll");
}
#endif

#endif  // NDEBUG && !COMPONENT_BUILD

TEST_F(DelayloadsTest, ChromeExeLoadSanityCheck) {
  base::FilePath exe;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe));
  exe = exe.Append(L"chrome.exe");
  std::vector<std::string> exe_imports = GetImports(exe);

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

TEST_F(MinimumWindowsSupportTest, ValidateImportChecker) {
  // These may need to be updated if the checked-in apiset file is updated.
  DetailedImports expected_ok = {{"ntdll.dll", {"DbgPrint"}}};
  AreImportsOk(expected_ok, {});
  // Tests exist to catch a repeat of crbug.com/1482250.
  DetailedImports expected_missing = {
      {"kernel32.dll", {"IsEnclaveTypeSupported"}}};
  EXPECT_NONFATAL_FAILURE(AreImportsOk(expected_missing, {}), "");
}

TEST_F(MinimumWindowsSupportTest, ValidateApisetChecker) {
  // These may need to be updated if the checked-in apiset file is updated.
  ASSERT_TRUE(SupportedApiSet("api-ms-win-core-synch-l1-2-0.dll"));
  ASSERT_FALSE(SupportedApiSet("api-ms-win-core-synch-l1-2-2.dll"));
  ASSERT_FALSE(SupportedApiSet("api-ms-win-core-synch-l1-3-0.dll"));
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
