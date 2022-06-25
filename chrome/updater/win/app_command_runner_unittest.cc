// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/app_command_runner.h"

#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>

#include <algorithm>
#include <array>
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

constexpr wchar_t kAppId1[] = L"{3B1A3CCA-0525-4418-93E6-A0DB3398EC9B}";

constexpr wchar_t kCmdLineValid[] =
    L"\"C:\\Program Files\\Windows Media Player\\wmpnscfg.exe\" /Close";

constexpr wchar_t kCmdId1[] = L"command 1";
constexpr wchar_t kCmdId2[] = L"command 2";

std::wstring GetCommandLine(int key, const std::wstring& exe_name) {
  base::FilePath programfiles_path;
  EXPECT_TRUE(base::PathService::Get(key, &programfiles_path));
  return base::CommandLine(programfiles_path.Append(exe_name))
      .GetCommandLineString();
}

}  // namespace

class AppCommandRunnerTest : public testing::Test {
 protected:
  AppCommandRunnerTest() = default;
  ~AppCommandRunnerTest() override = default;

  void SetUp() override {
    SetupCmdExe(GetTestScope(), cmd_exe_command_line_, temp_programfiles_dir_);
  }

  void TearDown() override { DeleteAppClientKey(GetTestScope(), kAppId1); }

  HRESULT CreateAppCommandRunner(const std::wstring& app_id,
                                 const std::wstring& command_id,
                                 const std::wstring& command_line_format,
                                 AppCommandRunner& app_command_runner) {
    CreateAppCommandRegistry(GetTestScope(), app_id, command_id,
                             command_line_format);

    return AppCommandRunner::LoadAppCommand(GetTestScope(), app_id, command_id,
                                            app_command_runner);
  }

  base::CommandLine cmd_exe_command_line_{base::CommandLine::NO_PROGRAM};
  base::ScopedTempDir temp_programfiles_dir_;
};

TEST_F(AppCommandRunnerTest, GetAppCommandFormatComponents_InvalidPaths) {
  const struct {
    const UpdaterScope scope;
    const wchar_t* command_format;
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
    EXPECT_EQ(
        AppCommandRunner::GetAppCommandFormatComponents(
            test_case.scope, test_case.command_format, executable, parameters),
        E_INVALIDARG);
  }
}

TEST_F(AppCommandRunnerTest, GetAppCommandFormatComponents_ProgramFilesPaths) {
  base::FilePath executable;
  std::vector<std::wstring> parameters;

  for (const int key : {base::DIR_PROGRAM_FILES, base::DIR_PROGRAM_FILESX86,
                        base::DIR_PROGRAM_FILES6432}) {
    const std::wstring process_command_line =
        GetCommandLine(key, L"process.exe");
    ASSERT_EQ(AppCommandRunner::GetAppCommandFormatComponents(
                  GetTestScope(), process_command_line, executable, parameters),
              S_OK);
    EXPECT_EQ(executable,
              base::CommandLine::FromString(process_command_line).GetProgram());
    EXPECT_TRUE(parameters.empty());
  }
}

TEST_F(AppCommandRunnerTest,
       GetAppCommandFormatComponents_And_FormatAppCommandLine) {
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
    ASSERT_EQ(AppCommandRunner::GetAppCommandFormatComponents(
                  GetTestScope(),
                  base::StrCat({process_command_line, L" ",
                                base::JoinString(test_case.input, L" ")}),
                  executable, parameters),
              S_OK);
    EXPECT_EQ(executable,
              base::CommandLine::FromString(process_command_line).GetProgram());
    EXPECT_EQ(parameters.size(), test_case.input.size());

    absl::optional<std::wstring> command_line =
        AppCommandRunner::FormatAppCommandLine(parameters,
                                               test_case.substitutions);
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

TEST_F(AppCommandRunnerTest, ExecuteAppCommand) {
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
    ASSERT_HRESULT_SUCCEEDED(AppCommandRunner::ExecuteAppCommand(
        cmd_exe_command_line_.GetProgram(), test_case.input,
        test_case.substitutions, process));

    int exit_code = 0;
    EXPECT_TRUE(process.WaitForExitWithTimeout(
        TestTimeouts::action_max_timeout(), &exit_code));
    EXPECT_EQ(exit_code, test_case.expected_exit_code);
  }
}

TEST_F(AppCommandRunnerTest, NoApp) {
  AppCommandRunner app_command_runner;
  EXPECT_HRESULT_FAILED(AppCommandRunner::LoadAppCommand(
      GetTestScope(), kAppId1, kCmdId1, app_command_runner));
}

TEST_F(AppCommandRunnerTest, NoCmd) {
  AppCommandRunner app_command_runner;
  CreateAppCommandRegistry(GetTestScope(), kAppId1, kCmdId1, kCmdLineValid);

  EXPECT_HRESULT_FAILED(AppCommandRunner::LoadAppCommand(
      GetTestScope(), kAppId1, kCmdId2, app_command_runner));
}

TEST_F(AppCommandRunnerTest, Run) {
  const struct {
    const std::vector<std::wstring> input;
    const std::vector<std::wstring> substitutions;
    const int expected_exit_code;
  } test_cases[] = {
      {{L"/c", L"exit 7"}, {}, 7},
      {{L"/c", L"exit %1"}, {L"5420"}, 5420},
  };

  for (const auto& test_case : test_cases) {
    AppCommandRunner app_command_runner;
    base::Process process;
    ASSERT_EQ(app_command_runner.Run(test_case.substitutions, process),
              E_UNEXPECTED);

    ASSERT_HRESULT_SUCCEEDED(CreateAppCommandRunner(
        kAppId1, kCmdId1,
        base::StrCat({cmd_exe_command_line_.GetCommandLineString(), L" ",
                      base::JoinString(test_case.input, L" ")}),
        app_command_runner));

    ASSERT_HRESULT_SUCCEEDED(
        app_command_runner.Run(test_case.substitutions, process));

    int exit_code = 0;
    EXPECT_TRUE(process.WaitForExitWithTimeout(
        TestTimeouts::action_max_timeout(), &exit_code));
    EXPECT_EQ(exit_code, test_case.expected_exit_code);
  }
}

TEST_F(AppCommandRunnerTest, LoadAutoRunOnOsUpgradeAppCommands) {
  const struct {
    const std::vector<std::wstring> input;
    const wchar_t* command_id;
  } test_cases[] = {
      {{L"/c", L"exit 7"}, kCmdId1},
      {{L"/c", L"exit 5420"}, kCmdId2},
  };

  std::for_each(
      std::begin(test_cases), std::end(test_cases), [&](const auto& test_case) {
        CreateAppCommandOSUpgradeRegistry(
            GetTestScope(), kAppId1, test_case.command_id,
            base::StrCat({cmd_exe_command_line_.GetCommandLineString(), L" ",
                          base::JoinString(test_case.input, L" ")}));
      });

  const std::vector<AppCommandRunner> app_command_runners =
      AppCommandRunner::LoadAutoRunOnOsUpgradeAppCommands(GetTestScope(),
                                                          kAppId1);

  ASSERT_EQ(std::size(app_command_runners), std::size(test_cases));
  std::for_each(app_command_runners.begin(), app_command_runners.end(),
                [&](const auto& app_command_runner) {
                  base::Process process;
                  EXPECT_HRESULT_SUCCEEDED(app_command_runner.Run({}, process));
                });
}

}  // namespace updater
