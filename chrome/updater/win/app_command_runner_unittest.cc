// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/updater/win/app_command_runner.h"

#include <windows.h>

#include <shellapi.h>
#include <shlobj.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_timeouts.h"
#include "base/win/scoped_localalloc.h"
#include "build/branding_buildflags.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util_win.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class AppCommandRunnerTestBase : public ::testing::Test {
 protected:
  AppCommandRunnerTestBase() = default;
  ~AppCommandRunnerTestBase() override = default;

  void SetUp() override {
    test::SetupCmdExe(GetUpdaterScopeForTesting(), cmd_exe_command_line_,
                      temp_programfiles_dir_);
  }

  void TearDown() override {
    test::DeleteAppClientKey(GetUpdaterScopeForTesting(), kAppId1);
  }

  HResultOr<AppCommandRunner> CreateAppCommandRunner(
      const std::wstring& app_id,
      const std::wstring& command_id,
      const std::wstring& command_line_format) {
    test::CreateAppCommandRegistry(GetUpdaterScopeForTesting(), app_id,
                                   command_id, command_line_format);
    return AppCommandRunner::LoadAppCommand(GetUpdaterScopeForTesting(), app_id,
                                            command_id);
  }

  HResultOr<AppCommandRunner> CreateProcessLauncherRunner(
      const std::wstring& app_id,
      const std::wstring& name,
      const std::wstring& pv,
      const std::wstring& command_id,
      const std::wstring& command_line_format) {
    EXPECT_TRUE(IsSystemInstall(GetUpdaterScopeForTesting()));
    test::CreateLaunchCmdElevatedRegistry(app_id, name, pv, command_id,
                                          command_line_format);
    return AppCommandRunner::LoadAppCommand(GetUpdaterScopeForTesting(), app_id,
                                            command_id);
  }

  base::CommandLine cmd_exe_command_line_{base::CommandLine::NO_PROGRAM};
  base::ScopedTempDir temp_programfiles_dir_;
};

class AppCommandRunnerTest : public AppCommandRunnerTestBase {};

struct AppCommandFormatComponentsInvalidPathsTestCase {
  const UpdaterScope scope;
  const wchar_t* const command_format;
};

class AppCommandFormatComponentsInvalidPathsTest
    : public ::testing::WithParamInterface<
          AppCommandFormatComponentsInvalidPathsTestCase>,
      public AppCommandRunnerTestBase {};

INSTANTIATE_TEST_SUITE_P(
    AppCommandFormatComponentsInvalidPathsTestCases,
    AppCommandFormatComponentsInvalidPathsTest,
    ::testing::ValuesIn(std::vector<
                        AppCommandFormatComponentsInvalidPathsTestCase>{
        // Relative paths are invalid.
        {UpdaterScope::kUser, L"process.exe"},

        // Paths not under %ProgramFiles% or %ProgramFilesX86% are invalid for
        // system.
        {UpdaterScope::kSystem, L"\"C:\\foobar\\process.exe\""},
        {UpdaterScope::kSystem, L"C:\\ProgramFiles\\process.exe"},
        {UpdaterScope::kSystem, L"C:\\windows\\system32\\cmd.exe"},
    }));

TEST_P(AppCommandFormatComponentsInvalidPathsTest, TestCases) {
  base::FilePath executable;
  std::vector<std::wstring> parameters;
  EXPECT_EQ(
      AppCommandRunner::GetAppCommandFormatComponents(
          GetParam().scope, GetParam().command_format, executable, parameters),
      E_INVALIDARG);
}

class AppCommandFormatComponentsProgramFilesPathsTest
    : public ::testing::WithParamInterface<int>,
      public AppCommandRunnerTestBase {};

INSTANTIATE_TEST_SUITE_P(AppCommandFormatComponentsProgramFilesPathsTestCases,
                         AppCommandFormatComponentsProgramFilesPathsTest,
                         ::testing::ValuesIn(std::vector<int>{
                             base::DIR_PROGRAM_FILES,
                             base::DIR_PROGRAM_FILESX86,
                             base::DIR_PROGRAM_FILES6432}));

TEST_P(AppCommandFormatComponentsProgramFilesPathsTest, TestCases) {
  base::FilePath executable;
  std::vector<std::wstring> parameters;
  const std::wstring process_command_line =
      GetCommandLine(GetParam(), L"process.exe");
  ASSERT_EQ(AppCommandRunner::GetAppCommandFormatComponents(
                GetUpdaterScopeForTesting(), process_command_line, executable,
                parameters),
            S_OK);
  EXPECT_EQ(executable,
            base::CommandLine::FromString(process_command_line).GetProgram());
  EXPECT_TRUE(parameters.empty());
}

struct AppCommandFormatParameterTestCase {
  const wchar_t* const format_string;
  const wchar_t* const expected_output;
  const std::vector<std::wstring> substitutions;
};

class AppCommandFormatParameterTest
    : public ::testing::WithParamInterface<AppCommandFormatParameterTestCase>,
      public AppCommandRunnerTestBase {};

INSTANTIATE_TEST_SUITE_P(
    AppCommandFormatParameterTestCases,
    AppCommandFormatParameterTest,
    ::testing::ValuesIn(std::vector<AppCommandFormatParameterTestCase>{
        // Format string does not have any placeholders.
        {L"abc=1 xyz=2 q", L"abc=1 xyz=2 q", {}},
        {L" abc=1    xyz=2 q ", L" abc=1    xyz=2 q ", {}},

        // Format string has placeholders.
        {L"abc=%1", L"abc=p1", {L"p1", L"p2", L"p3"}},
        {L"abc=%1  %3 %2=x  ", L"abc=p1  p3 p2=x  ", {L"p1", L"p2", L"p3"}},

        // Format string has correctly escaped literal `%` signs.
        {L"%1", L"p1", {L"p1", L"p2", L"p3"}},
        {L"%%1", L"%1", {L"p1", L"p2", L"p3"}},
        {L"%%%1", L"%p1", {L"p1", L"p2", L"p3"}},
        {L"abc%%def%%", L"abc%def%", {L"p1", L"p2", L"p3"}},
        {L"%12", L"p12", {L"p1", L"p2", L"p3"}},
        {L"%1%2", L"p1p2", {L"p1", L"p2", L"p3"}},

        // Format string has incorrect escaped `%` signs.
        {L"unescaped percent %", nullptr, {L"p1", L"p2", L"p3"}},
        {L"unescaped %%% percents", nullptr, {L"p1", L"p2", L"p3"}},
        {L"always escape percent otherwise %error",
         nullptr,
         {L"p1", L"p2", L"p3"}},
        {L"% percents need to be escaped%", nullptr, {L"p1", L"p2", L"p3"}},

        // Format string has invalid values for the placeholder index.
        {L"placeholder needs to be between 1 and 9, not %A",
         nullptr,
         {L"p1", L"p2", L"p3"}},
        {L"placeholder %4  is > size of substitutions vector",
         nullptr,
         {L"p1", L"p2", L"p3"}},
        {L"%1 is ok, but %8 or %9 is not ok", nullptr, {L"p1", L"p2", L"p3"}},
        {L"%4", nullptr, {L"p1", L"p2", L"p3"}},
        {L"abc=%1", nullptr, {}},
    }));

TEST_P(AppCommandFormatParameterTest, TestCases) {
  std::optional<std::wstring> output = AppCommandRunner::FormatParameter(
      GetParam().format_string, GetParam().substitutions);
  if (GetParam().expected_output) {
    EXPECT_EQ(output.value(), GetParam().expected_output);
  } else {
    EXPECT_EQ(output, std::nullopt);
  }
}

struct AppCommandFormatComponentsAndCommandLineTestCase {
  const std::vector<std::wstring> input;
  const wchar_t* const output;
  const std::vector<std::wstring> substitutions;
};

class AppCommandFormatComponentsAndCommandLineTest
    : public ::testing::WithParamInterface<
          AppCommandFormatComponentsAndCommandLineTestCase>,
      public AppCommandRunnerTestBase {};

INSTANTIATE_TEST_SUITE_P(
    AppCommandFormatComponentsAndCommandLineTestCases,
    AppCommandFormatComponentsAndCommandLineTest,
    ::testing::ValuesIn(
        std::vector<AppCommandFormatComponentsAndCommandLineTestCase>{
            // Unformatted parameters.
            {{L"abc=1"}, L"abc=1", {}},
            {{L"abc=1", L"xyz=2"}, L"abc=1 xyz=2", {}},
            {{L"abc=1", L"xyz=2", L"q"}, L"abc=1 xyz=2 q", {}},
            {{L" abc=1  ", L"  xyz=2", L"q "}, L"abc=1 xyz=2 q", {}},
            {{L"\"abc = 1\""}, L"\"abc = 1\"", {}},
            {{L"abc\" = \"1", L"xyz=2"}, L"\"abc = 1\" xyz=2", {}},
            {{L"\"abc = 1\""}, L"\"abc = 1\"", {}},
            {{L"abc\" = \"1"}, L"\"abc = 1\"", {}},

            // Simple parameters.
            {{L"abc=%1"}, L"abc=p1", {L"p1", L"p2", L"p3"}},
            {{L"abc=%1 ", L" %3", L" %2=x  "},
             L"abc=p1 p3 p2=x",
             {L"p1", L"p2", L"p3"}},

            // Escaping valid `%` signs.
            {{L"%1"}, L"p1", {L"p1", L"p2", L"p3"}},
            {{L"%%1"}, L"%1", {L"p1", L"p2", L"p3"}},
            {{L"%%%1"}, L"%p1", {L"p1", L"p2", L"p3"}},
            {{L"abc%%def%%"}, L"abc%def%", {L"p1", L"p2", L"p3"}},
            {{L"%12"}, L"p12", {L"p1", L"p2", L"p3"}},
            {{L"%1%2"}, L"p1p2", {L"p1", L"p2", L"p3"}},

            // Invalid `%` signs.
            {{L"unescaped", L"percent", L"%"}, nullptr, {L"p1", L"p2", L"p3"}},
            {{L"unescaped", L"%%%", L"percents"},
             nullptr,
             {L"p1", L"p2", L"p3"}},
            {{L"always", L"escape", L"percent", L"otherwise", L"%foobar"},
             nullptr,
             {L"p1", L"p2", L"p3"}},
            {{L"%", L"percents", L"need", L"to", L"be", L"escaped%"},
             nullptr,
             {L"p1", L"p2", L"p3"}},

            // Parameter index invalid or greater than substitutions.
            {{L"placeholder", L"needs", L"to", L"be", L"between", L"1", L"and",
              L"9,", L"not", L"%A"},
             nullptr,
             {L"p1", L"p2", L"p3"}},
            {{L"placeholder", L"%4 ", L"is", L">", L"size", L"of", L"input",
              L"substitutions"},
             nullptr,
             {L"p1", L"p2", L"p3"}},
            {{L"%1", L"is", L"ok,", L"but", L"%8", L"or", L"%9", L"is", L"not",
              L"ok"},
             nullptr,
             {L"p1", L"p2", L"p3"}},
            {{L"%4"}, nullptr, {L"p1", L"p2", L"p3"}},
            {{L"abc=%1"}, nullptr, {}},

            // Special characters in the substitution.
            // embedded \ and \\.
            {{L"%1"}, L"\"a\\b\\\\c\"", {L"a\\b\\\\c"}},
            // trailing \.
            {{L"%1"}, L"\"a\\\\\"", {L"a\\"}},
            // trailing \\.
            {{L"%1"}, L"\"a\\\\\\\\\"", {L"a\\\\"}},
            // only \\.
            {{L"%1"}, L"\"\\\\\\\\\"", {L"\\\\"}},
            // embedded quote.
            {{L"%1"}, L"\"a\\\"b\"", {L"a\"b"}},
            // trailing quote.
            {{L"%1"}, L"\"abc\\\"\"", {L"abc\""}},
            // embedded \\".
            {{L"%1"}, L"\"a\\\\\\\\\\\"b\"", {L"a\\\\\"b"}},
            // trailing \\".
            {{L"%1"}, L"\"abc\\\\\\\\\\\"\"", {L"abc\\\\\""}},
            // embedded space.
            {{L"%1"}, L"\"abc def\"", {L"abc def"}},
            // trailing space.
            {{L"%1"}, L"\"abcdef \"", {L"abcdef "}},
            // leading space.
            {{L"%1"}, L"\" abcdef\"", {L" abcdef"}},
        }));

TEST_P(AppCommandFormatComponentsAndCommandLineTest, TestCases) {
  const std::wstring process_command_line =
      GetCommandLine(base::DIR_PROGRAM_FILES, L"process.exe");
  base::FilePath executable;
  std::vector<std::wstring> parameters;

  ASSERT_EQ(AppCommandRunner::GetAppCommandFormatComponents(
                GetUpdaterScopeForTesting(),
                base::StrCat({process_command_line, L" ",
                              base::JoinString(GetParam().input, L" ")}),
                executable, parameters),
            S_OK);
  EXPECT_EQ(executable,
            base::CommandLine::FromString(process_command_line).GetProgram());
  EXPECT_EQ(parameters.size(), GetParam().input.size());

  std::optional<std::wstring> command_line =
      AppCommandRunner::FormatAppCommandLine(parameters,
                                             GetParam().substitutions);
  if (!GetParam().output) {
    EXPECT_EQ(command_line, std::nullopt);
    return;
  }

  EXPECT_EQ(command_line.value(), GetParam().output);

  if (GetParam().input[0] != L"%1" || GetParam().substitutions.size() != 1) {
    return;
  }

  // The formatted output is now sent through ::CommandLineToArgvW to
  // verify that it produces the original substitution.
  std::wstring cmd = base::StrCat({L"process.exe ", command_line.value()});
  int num_args = 0;
  base::win::ScopedLocalAllocTyped<wchar_t*> argv_handle(
      ::CommandLineToArgvW(&cmd[0], &num_args));
  ASSERT_TRUE(argv_handle);
  EXPECT_EQ(num_args, 2) << "substitution '" << GetParam().substitutions[0]
                         << "' gave command line '" << cmd
                         << "' which unexpectedly did not parse to a single "
                         << "argument.";

  EXPECT_EQ(argv_handle.get()[1], GetParam().substitutions[0])
      << "substitution '" << GetParam().substitutions[0]
      << "' gave command line '" << cmd << "' which did not parse back to the "
      << "original substitution";
}

struct AppCommandTestCase {
  const std::vector<std::wstring> input;
  const std::vector<std::wstring> substitutions;
  const int expected_exit_code;
};

class AppCommandExecuteTest
    : public ::testing::WithParamInterface<AppCommandTestCase>,
      public AppCommandRunnerTestBase {};

INSTANTIATE_TEST_SUITE_P(AppCommandExecuteTestCases,
                         AppCommandExecuteTest,
                         ::testing::ValuesIn(std::vector<AppCommandTestCase>{
                             {{L"/c", L"exit 7"}, {}, 7},
                             {{L"/c", L"exit %1"}, {L"5420"}, 5420},
                         }));

TEST_P(AppCommandExecuteTest, TestCases) {
  base::Process process;
  ASSERT_HRESULT_SUCCEEDED(AppCommandRunner::ExecuteAppCommand(
      cmd_exe_command_line_.GetProgram(), GetParam().input,
      GetParam().substitutions, process));

  int exit_code = 0;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, GetParam().expected_exit_code);
}

TEST_F(AppCommandRunnerTest, NoApp) {
  HResultOr<AppCommandRunner> app_command_runner =
      AppCommandRunner::LoadAppCommand(GetUpdaterScopeForTesting(), kAppId1,
                                       kCmdId1);
  EXPECT_FALSE(app_command_runner.has_value());
}

TEST_F(AppCommandRunnerTest, NoCmd) {
  test::CreateAppCommandRegistry(GetUpdaterScopeForTesting(), kAppId1, kCmdId1,
                                 kCmdLineValid);
  HResultOr<AppCommandRunner> app_command_runner =
      AppCommandRunner::LoadAppCommand(GetUpdaterScopeForTesting(), kAppId1,
                                       kCmdId2);
  EXPECT_FALSE(app_command_runner.has_value());
}

class RunAppCommandFormatTest
    : public ::testing::WithParamInterface<AppCommandTestCase>,
      public AppCommandRunnerTestBase {};

INSTANTIATE_TEST_SUITE_P(RunAppCommandFormatTestCases,
                         RunAppCommandFormatTest,
                         ::testing::ValuesIn(std::vector<AppCommandTestCase>{
                             {{L"/c", L"exit 7"}, {}, 7},
                             {{L"/c", L"exit %1"}, {L"5420"}, 5420},
                         }));

TEST_P(RunAppCommandFormatTest, TestCases) {
  AppCommandRunner app_command_runner;

  base::Process process;
  ASSERT_EQ(app_command_runner.Run(GetParam().substitutions, process),
            E_UNEXPECTED);

  ASSERT_OK_AND_ASSIGN(
      app_command_runner,
      CreateAppCommandRunner(
          kAppId1, kCmdId1,
          base::StrCat({cmd_exe_command_line_.GetCommandLineString(), L" ",
                        base::JoinString(GetParam().input, L" ")})));
  ASSERT_HRESULT_SUCCEEDED(
      app_command_runner.Run(GetParam().substitutions, process));

  int exit_code = 0;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, GetParam().expected_exit_code);
}

TEST_F(AppCommandRunnerTest, CheckChromeBrandedName) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_STREQ("Google Chrome", BROWSER_PRODUCT_NAME_STRING);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

struct RunProcessLauncherFormatTestCase {
  const wchar_t* const app_name;
  const wchar_t* const app_version;
  const wchar_t* const cmd_id;
  const std::vector<std::wstring> input;
  const int expected_exit_code;
  const int expected_hr;
};

class RunProcessLauncherFormatTest
    : public ::testing::WithParamInterface<RunProcessLauncherFormatTestCase>,
      public AppCommandRunnerTestBase {};

INSTANTIATE_TEST_SUITE_P(
    RunProcessLauncherFormatTestCases,
    RunProcessLauncherFormatTest,
    ::testing::ValuesIn(std::vector<RunProcessLauncherFormatTestCase>{
        {L"foo", L"1.0.0.0", L"cmd1", {L"/c", L"exit 7"}, 7, E_INVALIDARG},
        {L"foo", L"110.0.5434.0", L"cmd", {L"/c", L"exit 7"}, 7, E_INVALIDARG},
        {L"Chrome",
         L"110.0.5434.0",
         L"cmd",
         {L"/c", L"exit 7"},
         7,
         E_INVALIDARG},
        {L"Not" BROWSER_PRODUCT_NAME_STRING,
         L"110.0.5434.0",
         L"cmd",
         {L"/c", L"exit 7"},
         7,
         E_INVALIDARG},
        {L"" BROWSER_PRODUCT_NAME_STRING,
         L"110.0.5434.0",
         L"cmd",
         {L"/c", L"exit 7"},
         7,
         S_OK},
        {L"" BROWSER_PRODUCT_NAME_STRING L" Beta",
         L"1.0.0.0",
         L"cmd",
         {L"/c", L"exit 7"},
         7,
         S_OK},
        {L"" BROWSER_PRODUCT_NAME_STRING " Dev",
         L"110.0.0.0",
         L"cmd",
         {L"/c", L"exit 7"},
         7,
         S_OK},
        {L"" BROWSER_PRODUCT_NAME_STRING " SxS",
         L"110.0.0.0",
         L"cmd",
         {L"/c", L"exit 7"},
         7,
         S_OK},
    }));

TEST_P(RunProcessLauncherFormatTest, TestCases) {
  if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
    return;
  }

  HResultOr<AppCommandRunner> app_command_runner;
  base::Process process;
  ASSERT_EQ(app_command_runner->Run({}, process), E_UNEXPECTED);

  app_command_runner = CreateProcessLauncherRunner(
      kAppId1, GetParam().app_name, GetParam().app_version, GetParam().cmd_id,
      base::StrCat({cmd_exe_command_line_.GetCommandLineString(), L" ",
                    base::JoinString(GetParam().input, L" ")}));
  ASSERT_EQ(app_command_runner.error_or(S_OK), GetParam().expected_hr);
  if (FAILED(GetParam().expected_hr)) {
    return;
  }

  ASSERT_HRESULT_SUCCEEDED(app_command_runner->Run({}, process));

  int exit_code = 0;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, GetParam().expected_exit_code);
}

struct RunBothFormatsTestCase {
  const wchar_t* const cmd_id_to_execute;
  const wchar_t* const cmd_id_appcommand;
  const std::vector<std::wstring> input_appcommand;
  const wchar_t* const cmd_id_processlauncher;
  const std::vector<std::wstring> input_processlauncher;
  const int expected_exit_code;
};

class RunBothFormatsTest
    : public ::testing::WithParamInterface<RunBothFormatsTestCase>,
      public AppCommandRunnerTestBase {};

INSTANTIATE_TEST_SUITE_P(
    RunBothFormatsTestCases,
    RunBothFormatsTest,
    ::testing::ValuesIn(std::vector<RunBothFormatsTestCase>{
        // both formats in registry; AppCommand overrides ProcessLauncher entry.
        {L"cmd", L"cmd", {L"/c", L"exit 7"}, L"cmd", {L"/c", L"exit 14"}, 7},
        // only AppCommand format in registry.
        {L"cmd", L"cmd", {L"/c", L"exit 21"}, {}, {}, 21},
        // only ProcessLauncher format in registry.
        {L"cmd", {}, {}, L"cmd", {L"/c", L"exit 28"}, 28},
        // both formats in registry, but AppCommand has a different command ID,
        // so does not override ProcessLauncher entry.
        {L"cmd", L"cmd2", {L"/c", L"exit 7"}, L"cmd", {L"/c", L"exit 35"}, 35},
    }));

TEST_P(RunBothFormatsTest, TestCases) {
  if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }

  AppCommandRunner app_command_runner;
  base::Process process;
  ASSERT_EQ(app_command_runner.Run({}, process), E_UNEXPECTED);

  if (GetParam().cmd_id_appcommand) {
    test::CreateAppCommandRegistry(
        GetUpdaterScopeForTesting(), kAppId1, GetParam().cmd_id_appcommand,
        base::StrCat({cmd_exe_command_line_.GetCommandLineString(), L" ",
                      base::JoinString(GetParam().input_appcommand, L" ")}));
  }

  if (GetParam().cmd_id_processlauncher) {
    test::CreateLaunchCmdElevatedRegistry(
        kAppId1, L"" BROWSER_PRODUCT_NAME_STRING, L"1.0.0.0",
        GetParam().cmd_id_processlauncher,
        base::StrCat(
            {cmd_exe_command_line_.GetCommandLineString(), L" ",
             base::JoinString(GetParam().input_processlauncher, L" ")}));
  }

  ASSERT_OK_AND_ASSIGN(
      app_command_runner,
      AppCommandRunner::LoadAppCommand(GetUpdaterScopeForTesting(), kAppId1,
                                       GetParam().cmd_id_to_execute));

  ASSERT_HRESULT_SUCCEEDED(app_command_runner.Run({}, process));

  int exit_code = 0;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, GetParam().expected_exit_code);

  test::DeleteAppClientKey(GetUpdaterScopeForTesting(), kAppId1);
}

struct EachAppCommand {
  const std::vector<std::wstring> input;
  const wchar_t* const command_id;
};

using LoadAutoRunOnOsUpgradeAppCommandsTestCase = std::vector<EachAppCommand>;

class LoadAutoRunOnOsUpgradeAppCommandsTest
    : public ::testing::WithParamInterface<
          LoadAutoRunOnOsUpgradeAppCommandsTestCase>,
      public AppCommandRunnerTestBase {};

INSTANTIATE_TEST_SUITE_P(
    LoadAutoRunOnOsUpgradeAppCommandsTestCases,
    LoadAutoRunOnOsUpgradeAppCommandsTest,
    ::testing::ValuesIn(std::vector<LoadAutoRunOnOsUpgradeAppCommandsTestCase>{
        {
            {{L"/c", L"exit 7"}, kCmdId1},
            {{L"/c", L"exit 5420"}, kCmdId2},
        },
        {
            {{L"/c", L"exit 7"}, kCmdId1},
            {{L"/c", L"exit 5420"}, kCmdId2},
            {{L"/c", L"exit 3"}, L"command 3"},
            {{L"/c", L"exit 4"}, L"command 4"},
        }}));

TEST_P(LoadAutoRunOnOsUpgradeAppCommandsTest, TestCases) {
  base::ranges::for_each(GetParam(), [&](const auto& app_command) {
    test::CreateAppCommandOSUpgradeRegistry(
        GetUpdaterScopeForTesting(), kAppId1, app_command.command_id,
        base::StrCat({cmd_exe_command_line_.GetCommandLineString(), L" ",
                      base::JoinString(app_command.input, L" ")}));
  });

  const std::vector<AppCommandRunner> app_command_runners =
      AppCommandRunner::LoadAutoRunOnOsUpgradeAppCommands(
          GetUpdaterScopeForTesting(), kAppId1);

  ASSERT_EQ(std::size(app_command_runners), std::size(GetParam()));
  base::ranges::for_each(
      app_command_runners, [&](const auto& app_command_runner) {
        base::Process process;
        EXPECT_HRESULT_SUCCEEDED(app_command_runner.Run({}, process));
        EXPECT_TRUE(process.WaitForExit(/*exit_code=*/nullptr));
      });
}

}  // namespace updater
