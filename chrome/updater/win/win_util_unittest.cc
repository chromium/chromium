// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/win_util.h"

#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/unittest_util_win.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/win_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

// Converts an unsigned long integral to an HRESULT to avoid the
// warning about a narrowing conversion.
HRESULT MakeHRESULT(unsigned long x) {
  return static_cast<HRESULT>(x);
}

std::wstring GetCommandLine(int key, const std::wstring& exe_name) {
  base::FilePath programfiles_path;
  EXPECT_TRUE(base::PathService::Get(key, &programfiles_path));
  return base::CommandLine(programfiles_path.Append(exe_name))
      .GetCommandLineString();
}

}  // namespace

TEST(WinUtil, HRESULTFromUpdaterError) {
  EXPECT_EQ(HRESULTFromUpdaterError(0), MakeHRESULT(0xa0430000L));
  EXPECT_EQ(HRESULTFromUpdaterError(ERROR_ACCESS_DENIED),
            MakeHRESULT(0xa0430005));
  EXPECT_EQ(HRESULTFromUpdaterError(-1), -1);
  EXPECT_EQ(HRESULTFromUpdaterError(-10), -10);
}

TEST(WinUtil, GetDownloadProgress) {
  EXPECT_EQ(GetDownloadProgress(0, 50), 0);
  EXPECT_EQ(GetDownloadProgress(12, 50), 24);
  EXPECT_EQ(GetDownloadProgress(25, 50), 50);
  EXPECT_EQ(GetDownloadProgress(50, 50), 100);
  EXPECT_EQ(GetDownloadProgress(50, 50), 100);
  EXPECT_EQ(GetDownloadProgress(0, -1), -1);
  EXPECT_EQ(GetDownloadProgress(-1, -1), -1);
  EXPECT_EQ(GetDownloadProgress(50, 0), -1);
}

TEST(WinUtil, GetServiceDisplayName) {
  for (const bool is_internal_service : {true, false}) {
    EXPECT_EQ(base::StrCat({base::ASCIIToWide(PRODUCT_FULLNAME_STRING), L" ",
                            is_internal_service ? kWindowsInternalServiceName
                                                : kWindowsServiceName,
                            L" ", kUpdaterVersionUtf16}),
              GetServiceDisplayName(is_internal_service));
  }
}

TEST(WinUtil, GetServiceName) {
  for (const bool is_internal_service : {true, false}) {
    EXPECT_EQ(base::StrCat({base::ASCIIToWide(PRODUCT_FULLNAME_STRING),
                            is_internal_service ? kWindowsInternalServiceName
                                                : kWindowsServiceName,
                            kUpdaterVersionUtf16}),
              GetServiceName(is_internal_service));
  }
}

TEST(WinUtil, BuildMsiCommandLine) {
  EXPECT_STREQ(L"", BuildMsiCommandLine(std::wstring(L"arg1 arg2 arg3"), {},
                                        base::FilePath(L"NotMsi.exe"))
                        .c_str());
  EXPECT_STREQ(
      L"msiexec arg1 arg2 arg3 REBOOT=ReallySuppress /qn /i \"c:\\my "
      L"path\\YesMsi.msi\" /log \"c:\\my path\\YesMsi.msi.log\"",
      BuildMsiCommandLine(std::wstring(L"arg1 arg2 arg3"), {},
                          base::FilePath(L"c:\\my path\\YesMsi.msi"))
          .c_str());
  EXPECT_STREQ(
      L"msiexec arg1 arg2 arg3 INSTALLERDATA=\"c:\\my path\\installer data "
      L"file.dat\" REBOOT=ReallySuppress /qn /i \"c:\\my "
      L"path\\YesMsi.msi\" /log \"c:\\my path\\YesMsi.msi.log\"",
      BuildMsiCommandLine(
          std::wstring(L"arg1 arg2 arg3"),
          base::FilePath(L"c:\\my path\\installer data file.dat"),
          base::FilePath(L"c:\\my path\\YesMsi.msi"))
          .c_str());
}

TEST(WinUtil, BuildExeCommandLine) {
  EXPECT_STREQ(L"", BuildExeCommandLine(std::wstring(L"arg1 arg2 arg3"), {},
                                        base::FilePath(L"NotExe.msi"))
                        .c_str());
  EXPECT_STREQ(L"\"c:\\my path\\YesExe.exe\" arg1 arg2 arg3",
               BuildExeCommandLine(std::wstring(L"arg1 arg2 arg3"), {},
                                   base::FilePath(L"c:\\my path\\YesExe.exe"))
                   .c_str());
  EXPECT_STREQ(
      L"\"c:\\my path\\YesExe.exe\" arg1 arg2 arg3 --installerdata=\"c:\\my "
      L"path\\installer data file.dat\"",
      BuildExeCommandLine(
          std::wstring(L"arg1 arg2 arg3"),
          base::FilePath(L"c:\\my path\\installer data file.dat"),
          base::FilePath(L"c:\\my path\\YesExe.exe"))
          .c_str());
}

TEST(WinUtil, ShellExecuteAndWait) {
  DWORD exit_code = 0;

  EXPECT_EQ(ShellExecuteAndWait(base::FilePath(L"NonExistent.Exe"), {}, {},
                                &exit_code),
            HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

  EXPECT_HRESULT_SUCCEEDED(ShellExecuteAndWait(
      GetTestProcessCommandLine(GetTestScope()).GetProgram(), {}, {},
      &exit_code));
  EXPECT_EQ(exit_code, 0UL);
}

TEST(WinUtil, RunElevated) {
  // TODO(crbug.com/1314521): Click on UAC prompts in Updater tests that require
  // elevation
  if (!::IsUserAnAdmin())
    return;

  DWORD exit_code = 0;
  const base::CommandLine test_process_cmd_line =
      GetTestProcessCommandLine(GetTestScope());
  EXPECT_HRESULT_SUCCEEDED(
      RunElevated(test_process_cmd_line.GetProgram(),
                  test_process_cmd_line.GetArgumentsString(), &exit_code));
  EXPECT_EQ(exit_code, 0UL);
}

TEST(WinUtil, GetOSVersion) {
  absl::optional<OSVERSIONINFOEX> rtl_os_version = GetOSVersion();
  ASSERT_NE(rtl_os_version, absl::nullopt);

  // Compare to the version from `::GetVersionEx`.
  OSVERSIONINFOEX os = {};
  os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  EXPECT_TRUE(::GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&os)));

  EXPECT_EQ(rtl_os_version->dwOSVersionInfoSize, os.dwOSVersionInfoSize);
  EXPECT_EQ(rtl_os_version->dwMajorVersion, os.dwMajorVersion);
  EXPECT_EQ(rtl_os_version->dwMinorVersion, os.dwMinorVersion);
  EXPECT_EQ(rtl_os_version->dwBuildNumber, os.dwBuildNumber);
  EXPECT_EQ(rtl_os_version->dwPlatformId, os.dwPlatformId);
  EXPECT_STREQ(rtl_os_version->szCSDVersion, os.szCSDVersion);
  EXPECT_EQ(rtl_os_version->wServicePackMajor, os.wServicePackMajor);
  EXPECT_EQ(rtl_os_version->wServicePackMinor, os.wServicePackMinor);
  EXPECT_EQ(rtl_os_version->wSuiteMask, os.wSuiteMask);
  EXPECT_EQ(rtl_os_version->wProductType, os.wProductType);
}

TEST(WinUtil, CompareOSVersions_SameAsCurrent) {
  absl::optional<OSVERSIONINFOEX> this_os = GetOSVersion();
  ASSERT_NE(this_os, absl::nullopt);

  EXPECT_TRUE(CompareOSVersions(this_os.value(), VER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(this_os.value(), VER_GREATER_EQUAL));
  EXPECT_FALSE(CompareOSVersions(this_os.value(), VER_GREATER));
  EXPECT_FALSE(CompareOSVersions(this_os.value(), VER_LESS));
  EXPECT_TRUE(CompareOSVersions(this_os.value(), VER_LESS_EQUAL));
}

TEST(WinUtil, CompareOSVersions_NewBuildNumber) {
  absl::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, absl::nullopt);
  ASSERT_GT(prior_os->dwBuildNumber, 0UL);
  --prior_os->dwBuildNumber;

  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
}

TEST(WinUtil, CompareOSVersions_NewMajor) {
  absl::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, absl::nullopt);
  ASSERT_GT(prior_os->dwMajorVersion, 0UL);
  --prior_os->dwMajorVersion;

  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
}

TEST(WinUtil, CompareOSVersions_NewMinor) {
  absl::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, absl::nullopt);

  // This test only runs if the current OS has a minor version.
  if (prior_os->dwMinorVersion >= 1) {
    --prior_os->dwMinorVersion;

    EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
    EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
    EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER));
    EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS));
    EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
  }
}

TEST(WinUtil, CompareOSVersions_NewMajorWithLowerMinor) {
  absl::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, absl::nullopt);
  ASSERT_GT(prior_os->dwMajorVersion, 0UL);
  --prior_os->dwMajorVersion;
  ++prior_os->dwMinorVersion;

  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_GREATER));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
}

TEST(WinUtil, CompareOSVersions_OldMajor) {
  absl::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, absl::nullopt);
  ++prior_os->dwMajorVersion;

  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
  EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_GREATER));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_LESS));
  EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
}

TEST(WinUtil, CompareOSVersions_OldMajorWithHigherMinor) {
  absl::optional<OSVERSIONINFOEX> prior_os = GetOSVersion();
  ASSERT_NE(prior_os, absl::nullopt);

  // This test only runs if the current OS has a minor version.
  if (prior_os->dwMinorVersion >= 1) {
    ++prior_os->dwMajorVersion;
    --prior_os->dwMinorVersion;

    EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_EQUAL));
    EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_GREATER_EQUAL));
    EXPECT_FALSE(CompareOSVersions(prior_os.value(), VER_GREATER));
    EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_LESS));
    EXPECT_TRUE(CompareOSVersions(prior_os.value(), VER_LESS_EQUAL));
  }
}

TEST(WinUtil, GetAppCommandFormatComponents_InvalidPaths) {
  const struct {
    const UpdaterScope scope;
    const wchar_t* input;
  } test_cases[] = {
      // Relative paths are invalid.
      {UpdaterScope::kUser, L"process.exe"},

      // Paths not under %ProgramFiles% or %ProgramFilesX86% are invalid for
      // system.
      {UpdaterScope::kSystem, L"\"C:\\foobar\\process.exe\""},
      {UpdaterScope::kSystem, L"C:\\ProgramFiles\\process.exe"},
      {UpdaterScope::kSystem, L"C:\\windows\\system32\\cmd.exe"},
  };

  base::FilePath executable;
  std::vector<std::wstring> parameters;

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(GetAppCommandFormatComponents(test_case.scope, test_case.input,
                                            executable, parameters),
              E_INVALIDARG);
  }
}

TEST(WinUtil, GetAppCommandFormatComponents_ProgramFilesPaths) {
  base::FilePath executable;
  std::vector<std::wstring> parameters;

  for (const int key : {base::DIR_PROGRAM_FILES, base::DIR_PROGRAM_FILESX86,
                        base::DIR_PROGRAM_FILES6432}) {
    const std::wstring process_command_line =
        GetCommandLine(key, L"process.exe");
    ASSERT_EQ(GetAppCommandFormatComponents(
                  GetTestScope(), process_command_line, executable, parameters),
              S_OK);
    EXPECT_EQ(executable,
              base::CommandLine::FromString(process_command_line).GetProgram());
    EXPECT_TRUE(parameters.empty());
  }
}

TEST(WinUtil, GetAppCommandFormatComponents_And_FormatAppCommandLine) {
  const std::vector<std::wstring> nosubstitutions = {};
  const std::vector<std::wstring> p1p2p3 = {L"p1", L"p2", L"p3"};

  const struct {
    std::vector<std::wstring> input;
    const wchar_t* output;
    const std::vector<std::wstring>& substitutions;
  } test_cases[] = {
      // Unformatted parameters.
      {{L"abc=1"}, L"abc=1", nosubstitutions},
      {{L"abc=1", L"xyz=2"}, L"abc=1 xyz=2", nosubstitutions},
      {{L"abc=1", L"xyz=2", L"q"}, L"abc=1 xyz=2 q", nosubstitutions},
      {{L" abc=1  ", L"  xyz=2", L"q "}, L"abc=1 xyz=2 q", nosubstitutions},
      {{L"\"abc = 1\""}, L"\"abc = 1\"", nosubstitutions},
      {{L"abc\" = \"1", L"xyz=2"}, L"\"abc = 1\" xyz=2", nosubstitutions},
      {{L"\"abc = 1\""}, L"\"abc = 1\"", nosubstitutions},
      {{L"abc\" = \"1"}, L"\"abc = 1\"", nosubstitutions},

      // Simple parameters.
      {{L"abc=%1"}, L"abc=p1", p1p2p3},
      {{L"abc=%1 ", L" %3", L" %2=x  "}, L"abc=p1 p3 p2=x", p1p2p3},

      // Escaping valid `%` signs.
      {{L"%1"}, L"p1", p1p2p3},
      {{L"%%1"}, L"%1", p1p2p3},
      {{L"%%%1"}, L"%p1", p1p2p3},
      {{L"abc%%def%%"}, L"abc%def%", p1p2p3},
      {{L"%12"}, L"p12", p1p2p3},
      {{L"%1%2"}, L"p1p2", p1p2p3},

      // Invalid `%` signs.
      {{L"unescaped", L"percent", L"%"}, nullptr, p1p2p3},
      {{L"unescaped", L"%%%", L"percents"}, nullptr, p1p2p3},
      {{L"always", L"escape", L"percent", L"otherwise", L"%foobar"},
       nullptr,
       p1p2p3},
      {{L"%", L"percents", L"need", L"to", L"be", L"escaped%"},
       nullptr,
       p1p2p3},

      // Parameter index invalid or greater than substitutions.
      {{L"placeholder", L"needs", L"to", L"be", L"between", L"1", L"and", L"9,",
        L"not", L"%A"},
       nullptr,
       p1p2p3},
      {{L"placeholder", L"%4 ", L"is", L">", L"size", L"of", L"input",
        L"substitutions"},
       nullptr,
       p1p2p3},
      {{L"%1", L"is", L"ok,", L"but", L"%8", L"or", L"%9", L"is", L"not",
        L"ok"},
       nullptr,
       p1p2p3},
      {{L"%4"}, nullptr, p1p2p3},
      {{L"abc=%1"}, nullptr, nosubstitutions},

      // Special characters in the substitution.
      // embedded \ and \\.
      {{L"%1"}, L"a\\b\\\\c", {L"a\\b\\\\c"}},
      // trailing \.
      {{L"%1"}, L"a\\", {L"a\\"}},
      // trailing \\.
      {{L"%1"}, L"a\\\\", {L"a\\\\"}},
      // only \\.
      {{L"%1"}, L"\\\\", {L"\\\\"}},
      // empty.
      {{L"%1"}, L"\"\"", {L""}},
      // embedded quote.
      {{L"%1"}, L"a\\\"b", {L"a\"b"}},
      // trailing quote.
      {{L"%1"}, L"abc\\\"", {L"abc\""}},
      // embedded \\".
      {{L"%1"}, L"a\\\\\\\\\\\"b", {L"a\\\\\"b"}},
      // trailing \\".
      {{L"%1"}, L"abc\\\\\\\\\\\"", {L"abc\\\\\""}},
      // embedded space.
      {{L"%1"}, L"\"abc def\"", {L"abc def"}},
      // trailing space.
      {{L"%1"}, L"\"abcdef \"", {L"abcdef "}},
      // leading space.
      {{L"%1"}, L"\" abcdef\"", {L" abcdef"}},
  };

  const std::wstring process_command_line =
      GetCommandLine(base::DIR_PROGRAM_FILES, L"process.exe");
  base::FilePath executable;
  std::vector<std::wstring> parameters;

  for (const auto& test_case : test_cases) {
    ASSERT_EQ(GetAppCommandFormatComponents(
                  GetTestScope(),
                  base::StrCat({process_command_line, L" ",
                                base::JoinString(test_case.input, L" ")}),
                  executable, parameters),
              S_OK);
    EXPECT_EQ(executable,
              base::CommandLine::FromString(process_command_line).GetProgram());
    EXPECT_EQ(parameters.size(), test_case.input.size());

    absl::optional<std::wstring> command_line =
        FormatAppCommandLine(parameters, test_case.substitutions);
    if (!test_case.output) {
      EXPECT_EQ(command_line, absl::nullopt);
      continue;
    }

    EXPECT_EQ(command_line.value(), test_case.output);

    if (test_case.input[0] != L"%1" || test_case.substitutions.size() != 1)
      continue;

    // The formatted output is now sent through ::CommandLineToArgvW to
    // verify that it produces the original substitution.
    std::wstring cmd = base::StrCat({L"process.exe ", command_line.value()});
    int num_args = 0;
    ScopedLocalAlloc argv_handle(::CommandLineToArgvW(&cmd[0], &num_args));
    ASSERT_TRUE(argv_handle.is_valid());
    EXPECT_EQ(num_args, 2) << "substitution '" << test_case.substitutions[0]
                           << "' gave command line '" << cmd
                           << "' which unexpectedly did not parse to a single "
                           << "argument.";

    EXPECT_EQ(reinterpret_cast<const wchar_t**>(argv_handle.get())[1],
              test_case.substitutions[0])
        << "substitution '" << test_case.substitutions[0]
        << "' gave command line '" << cmd
        << "' which did not parse back to the "
        << "original substitution";
  }
}

TEST(WinUtil, ExecuteAppCommand) {
  base::CommandLine cmd_exe_command_line(base::CommandLine::NO_PROGRAM);
  base::ScopedTempDir temp_programfiles_dir;
  SetupCmdExe(GetTestScope(), cmd_exe_command_line, temp_programfiles_dir);

  const struct {
    const std::vector<std::wstring> input;
    const std::vector<std::wstring> substitutions;
    const int expected_exit_code;
  } test_cases[] = {
      {{L"/c", L"exit 7"}, {}, 7},
      {{L"/c", L"exit %1"}, {L"5420"}, 5420},
  };

  for (const auto& test_case : test_cases) {
    base::Process process;
    ASSERT_HRESULT_SUCCEEDED(
        ExecuteAppCommand(cmd_exe_command_line.GetProgram(), test_case.input,
                          test_case.substitutions, process));

    int exit_code = 0;
    EXPECT_TRUE(process.WaitForExitWithTimeout(
        TestTimeouts::action_max_timeout(), &exit_code));
    EXPECT_EQ(exit_code, test_case.expected_exit_code);
  }
}

}  // namespace updater
