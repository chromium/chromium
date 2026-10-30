// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/installer.h"

#include <shlobj.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/pe_image.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/win/installer/exit_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char install_switch[] = "--install=";
constexpr char enable_logging_switch[] = "--enable-logging";
constexpr char logging_module_switch[] = "--vmodule=";

using DetailedImports = std::map<std::string, std::set<std::string>>;
using SupportedApiSets = std::map<std::string, size_t>;

bool SupportedApiSet(std::string_view import,
                     const SupportedApiSets& supported_api_sets) {
  if (!base::StartsWith(import, "api-") || !base::EndsWith(import, ".dll")) {
    return false;
  }
  // Need at least api-{components}-l1-1-0.dll so > four dashes.
  if (std::ranges::count(import, '-') < 5) {
    return false;
  }

  // Want (requested_api: api-{components}-l1-1) and (requested_version: 0).
  const size_t last_dash = import.rfind('-');
  const size_t dot = import.rfind('.');
  size_t requested_version = 0;
  if (!base::StringToSizeT(import.substr(last_dash + 1, dot - last_dash - 1),
                           &requested_version)) {
    return false;
  }

  const auto api_to_max_ver =
      supported_api_sets.find(std::string(import.substr(0, last_dash)));
  return api_to_max_ver != supported_api_sets.end() &&
         requested_version <= api_to_max_ver->second;
}

void ExpectExactlyOneOccurrence(const std::wstring& search_string,
                                const std::wstring& substring) {
  auto pos = search_string.find(substring);
  EXPECT_NE(pos, std::wstring::npos);
  pos = search_string.find(substring, pos + substring.size());
  EXPECT_EQ(pos, std::wstring::npos);
}

void ExpectSwitchValue(const std::wstring& cmd_line,
                       const std::wstring& switch_str,
                       const std::wstring& expected_value) {
  auto pos = cmd_line.find(switch_str);
  EXPECT_NE(pos, std::wstring::npos);
  pos += switch_str.size();
  EXPECT_LE(pos + expected_value.size(), cmd_line.size());
  EXPECT_EQ(cmd_line.substr(pos, expected_value.size()), expected_value);
}
}  // namespace

// Tests that `HandleRunElevated` returns `UNEXPECTED_ELEVATION_LOOP` when
// not elevated and called with `kCmdLineExpectElevated` argument.
TEST(InstallerTest, HandleRunElevated) {
  if (::IsUserAnAdmin()) {
    return;
  }

  base::CommandLine command_line(
      base::FilePath(FILE_PATH_LITERAL("UpdaterSetup.exe")));
  command_line.AppendSwitch(updater::kInstallSwitch);
  command_line.AppendSwitch(updater::kSystemSwitch);
  command_line.AppendSwitch(updater::kCmdLineExpectElevated);

  updater::ProcessExitResult exit_result =
      updater::HandleRunElevated(command_line);
  EXPECT_EQ(exit_result.exit_code, updater::UNEXPECTED_ELEVATION_LOOP);
  EXPECT_EQ(exit_result.windows_error, 0U);
}

TEST(InstallerTest, HandleRunElevatedSilentBlocked) {
  if (::IsUserAnAdmin()) {
    return;
  }

  base::CommandLine command_line(
      base::FilePath(FILE_PATH_LITERAL("UpdaterSetup.exe")));
  command_line.AppendSwitch(updater::kInstallSwitch);
  command_line.AppendSwitch(updater::kSystemSwitch);
  command_line.AppendSwitch(updater::kSilentSwitch);

  updater::ProcessExitResult exit_result =
      updater::HandleRunElevated(command_line);
  EXPECT_EQ(exit_result.exit_code, updater::UNEXPECTED_ELEVATION_LOOP_SILENT);
  EXPECT_EQ(exit_result.windows_error, 0U);
}

TEST(InstallerTest, FindOfflineDir) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath unpack_path = temp_dir.GetPath();
  base::FilePath metainstall_dir = unpack_path.Append(L"bin");
  ASSERT_TRUE(base::CreateDirectory(metainstall_dir));

  EXPECT_FALSE(updater::FindOfflineDir(unpack_path).has_value());

  base::FilePath offline_install_dir =
      metainstall_dir.Append(L"Offline")
          .Append(L"{8D5D0563-F2A0-40E3-932D-AFEAE261A9D1}");
  ASSERT_TRUE(base::CreateDirectory(offline_install_dir));

  std::optional<base::FilePath> offline_dir =
      updater::FindOfflineDir(unpack_path);
  EXPECT_TRUE(offline_dir.has_value());
  EXPECT_EQ(offline_dir->BaseName(),
            base::FilePath(L"{8D5D0563-F2A0-40E3-932D-AFEAE261A9D1}"));
}

// Verifies that updater and installer executables only statically import
// functions and ApiSets supported on Windows 10 10.0.10240 (RTM), preventing
// startup failures in the Windows PE loader on 10.0.10240.
TEST(InstallerTest, MinimumWindowsSupport) {
  const DetailedImports kAvailableImports = {
#include "chrome/updater/supported_imports.inc"
  };
  const SupportedApiSets kSupportedApiSets = {
#include "chrome/test/delayload/supported_apisets_10.0.10240.inc"
  };

  ASSERT_TRUE(kAvailableImports.at("user32.dll").contains("GetSystemMetrics"));
  ASSERT_FALSE(kAvailableImports.at("user32.dll").contains("GetDpiForWindow"));
  ASSERT_FALSE(
      kAvailableImports.at("user32.dll").contains("GetSystemMetricsForDpi"));
  ASSERT_TRUE(
      SupportedApiSet("api-ms-win-core-synch-l1-2-0.dll", kSupportedApiSets));
  ASSERT_FALSE(
      SupportedApiSet("api-ms-win-core-synch-l1-2-2.dll", kSupportedApiSets));

  base::FilePath out_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &out_dir));

  static constexpr const wchar_t* kExecutables[] = {
      L"UpdaterSetup.exe",
      L"UpdaterSetup_test.exe",
      L"updater.exe",
      L"updater_test.exe",
  };

  for (const wchar_t* executable : kExecutables) {
    const base::FilePath exe_path = out_dir.Append(executable);
    SCOPED_TRACE(exe_path.value());
    ASSERT_TRUE(base::PathExists(exe_path));

    base::MemoryMappedFile mapped_exe;
    ASSERT_TRUE(mapped_exe.Initialize(exe_path));

    // Only static imports are checked; delay-loaded imports are intentionally
    // not checked because they are not resolved at process load time.
    DetailedImports imports;
    base::win::PEImageAsData pe_image(
        reinterpret_cast<HMODULE>(mapped_exe.mutable_bytes().data()));
    ASSERT_TRUE(pe_image.VerifyMagic());
    pe_image.EnumAllImports(
        [](const base::win::PEImage&, LPCSTR module, DWORD, LPCSTR import_name,
           DWORD, PIMAGE_THUNK_DATA, PVOID cookie) -> bool {
          if (!module) {
            ADD_FAILURE();
            return false;
          }
          if (import_name) {
            (*static_cast<DetailedImports*>(cookie))[base::ToLowerASCII(module)]
                .insert(import_name);
          }
          return true;
        },
        &imports, nullptr);
    ASSERT_TRUE(imports.contains("kernel32.dll"));
    ASSERT_TRUE(imports.contains("user32.dll"));

    for (const auto& [module, functions] : imports) {
      if (base::StartsWith(module, "api-")) {
        EXPECT_TRUE(SupportedApiSet(module, kSupportedApiSets))
            << "Unsupported ApiSet " << module << " in "
            << exe_path.BaseName().value();
        continue;
      }
      // `supported_imports_10.0.10240.txt` only covers the subset of system
      // DLLs dumped for `chrome.exe`, and non-official builds also import
      // component/debug/sanitizer runtime DLLs.
      const auto available_functions = kAvailableImports.find(module);
      if (available_functions == kAvailableImports.end()) {
        continue;
      }
      for (const std::string& function : functions) {
        EXPECT_TRUE(available_functions->second.contains(function))
            << "Unsupported import " << function << " from " << module << " in "
            << exe_path.BaseName().value();
      }
    }
  }
}

TEST(BuildInstallerCommandLineArgumentsTest, EnableLoggingSwitch) {
  // Test that --enable-logging switch is added if none is provided.
  updater::CommandString cmd_line_args;
  std::wstring command_line_str(L"UpdaterSetup.exe");
  // Add a tag switch to bypass attempting to parse a tag.
  command_line_str = base::SysUTF8ToWide(
      base::StrCat({"UpdaterSetup.exe ", install_switch, "fake_tag"}));
  updater::ProcessExitResult exit_result =
      updater::BuildInstallerCommandLineArguments(command_line_str.c_str(),
                                                  cmd_line_args.get(),
                                                  cmd_line_args.capacity());
  EXPECT_EQ(exit_result.exit_code, updater::SUCCESS_EXIT_CODE);
  ExpectExactlyOneOccurrence(std::wstring(cmd_line_args.get()),
                             base::SysUTF8ToWide(enable_logging_switch));

  // Test that no --enable-logging switch is added if one is provided.
  cmd_line_args.clear();
  // Add a tag switch to bypass attempting to parse a tag.
  command_line_str =
      base::SysUTF8ToWide(base::StrCat({"UpdaterSetup.exe ", install_switch,
                                        "fake_tag ", enable_logging_switch}));
  exit_result = updater::BuildInstallerCommandLineArguments(
      command_line_str.c_str(), cmd_line_args.get(), cmd_line_args.capacity());
  EXPECT_EQ(exit_result.exit_code, updater::SUCCESS_EXIT_CODE);
  ExpectExactlyOneOccurrence(std::wstring(cmd_line_args.get()),
                             base::SysUTF8ToWide(enable_logging_switch));
}

TEST(BuildInstallerCommandLineArgumentsTest, LoggingModuleSwitch) {
  // Test that --vmodule switch is added if none is provided.
  updater::CommandString cmd_line_args;
  std::wstring command_line_str(L"UpdaterSetup.exe");
  // Add a tag switch to bypass attempting to parse a tag.
  command_line_str = base::SysUTF8ToWide(
      base::StrCat({"UpdaterSetup.exe ", install_switch, "fake_tag"}));
  updater::ProcessExitResult exit_result =
      updater::BuildInstallerCommandLineArguments(command_line_str.c_str(),
                                                  cmd_line_args.get(),
                                                  cmd_line_args.capacity());
  EXPECT_EQ(exit_result.exit_code, updater::SUCCESS_EXIT_CODE);
  ExpectExactlyOneOccurrence(std::wstring(cmd_line_args.get()),
                             base::SysUTF8ToWide(logging_module_switch));

  // Test that no --vmodule switch is added if one is provided.
  cmd_line_args.clear();
  // Add a tag switch to bypass attempting to parse a tag.
  command_line_str = base::SysUTF8ToWide(
      base::StrCat({"UpdaterSetup.exe ", install_switch, "fake_tag ",
                    logging_module_switch, "fake_module"}));
  exit_result = updater::BuildInstallerCommandLineArguments(
      command_line_str.c_str(), cmd_line_args.get(), cmd_line_args.capacity());
  EXPECT_EQ(exit_result.exit_code, updater::SUCCESS_EXIT_CODE);
  ExpectExactlyOneOccurrence(std::wstring(cmd_line_args.get()),
                             base::SysUTF8ToWide(logging_module_switch));
  ExpectSwitchValue(command_line_str,
                    base::SysUTF8ToWide(logging_module_switch),
                    std::wstring(L"fake_module"));
}

TEST(BuildInstallerCommandLineArgumentsTest, CommandStringOverflow) {
  updater::CommandString cmd_line_args;
  std::wstring command_line_str(L"UpdaterSetup.exe");
  std::string long_tag(updater::kInstallerMaxCommandString + 1, 'A');
  command_line_str = base::SysUTF8ToWide(
      base::StrCat({"UpdaterSetup.exe ", install_switch, long_tag}));
  updater::ProcessExitResult exit_result =
      updater::BuildInstallerCommandLineArguments(command_line_str.c_str(),
                                                  cmd_line_args.get(),
                                                  cmd_line_args.capacity());
  EXPECT_EQ(exit_result.exit_code, updater::COMMAND_STRING_OVERFLOW);
}

TEST(BuildInstallerCommandLineArgumentsTest, NoArguments) {
  // Passing in no arguments on the command line will attempt to
  // extract the embedded tag, but since the test executable is not
  // tagged this should not add any --tag switches.
  updater::CommandString cmd_line_args;
  std::wstring command_line_str(L"UpdaterSetup.exe");
  updater::ProcessExitResult exit_result =
      updater::BuildInstallerCommandLineArguments(command_line_str.c_str(),
                                                  cmd_line_args.get(),
                                                  cmd_line_args.capacity());
  EXPECT_EQ(exit_result.exit_code, updater::INVALID_OPTION);
}

TEST(BuildInstallerCommandLineArgumentsTest, LegacyCommandLine) {
  std::optional<base::CommandLine> cmd_line =
      updater::CommandLineForLegacyFormat(
          L"UpdaterSetup.exe /install "
          L"\"appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&appname=Google%"
          L"20Chrome&needsadmin=Prefers&lang=en\"");
  ASSERT_TRUE(cmd_line.has_value());
  updater::CommandString cmd_line_args;
  updater::ProcessExitResult exit_result =
      updater::BuildInstallerCommandLineArguments(
          cmd_line->GetCommandLineString().c_str(), cmd_line_args.get(),
          cmd_line_args.capacity());
  EXPECT_EQ(exit_result.exit_code, updater::SUCCESS_EXIT_CODE);
  const base::CommandLine command_line = base::CommandLine::FromString(
      base::StrCat({L"exe.exe ", cmd_line_args.get()}));
  EXPECT_EQ(command_line.GetSwitchValueUTF8(updater::kInstallSwitch),
            "appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&appname=Google%"
            "20Chrome&needsadmin=Prefers&lang=en");
}
