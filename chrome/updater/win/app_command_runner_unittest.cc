// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/app_command_runner.h"

#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>

#include <array>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/test_timeouts.h"
#include "base/win/scoped_localalloc.h"
#include "build/branding_buildflags.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/unittest_util_win.h"
#include "chrome/updater/util/win_util.h"
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

  HResultOr<AppCommandRunner> CreateAppCommandRunner(
      const std::wstring& app_id,
      const std::wstring& command_id,
      const std::wstring& command_line_format) {
    CreateAppCommandRegistry(GetTestScope(), app_id, command_id,
                             command_line_format);
    return AppCommandRunner::LoadAppCommand(GetTestScope(), app_id, command_id);
  }

  HResultOr<AppCommandRunner> CreateProcessLauncherRunner(
      const std::wstring& app_id,
      const std::wstring& name,
      const std::wstring& pv,
      const std::wstring& command_id,
      const std::wstring& command_line_format) {
    EXPECT_TRUE(IsSystemInstall(GetTestScope()));
    CreateLaunchCmdElevatedRegistry(app_id, name, pv, command_id,
                                    command_line_format);
    return AppCommandRunner::LoadAppCommand(GetTestScope(), app_id, command_id);
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

TEST_F(AppCommandRunnerTest, FormatParameter) {
  const std::vector<std::wstring> no_substitutions = {};
  const std::vector<std::wstring> p1p2p3 = {L"p1", L"p2", L"p3"};

  const struct {
    const wchar_t* format_string;
    const wchar_t* expected_output;
    const std::vector<std::wstring>& substitutions;
  } test_cases[] = {
      // Format string does not have any placeholders.
      {L"abc=1 xyz=2 q", L"abc=1 xyz=2 q", no_substitutions},
      {L" abc=1    xyz=2 q ", L" abc=1    xyz=2 q ", no_substitutions},

      // Format string has placeholders.
      {L"abc=%1", L"abc=p1", p1p2p3},
      {L"abc=%1  %3 %2=x  ", L"abc=p1  p3 p2=x  ", p1p2p3},

      // Format string has correctly escaped literal `%` signs.
      {L"%1", L"p1", p1p2p3},
      {L"%%1", L"%1", p1p2p3},
      {L"%%%1", L"%p1", p1p2p3},
      {L"abc%%def%%", L"abc%def%", p1p2p3},
      {L"%12", L"p12", p1p2p3},
      {L"%1%2", L"p1p2", p1p2p3},

      // Format string has incorrect escaped `%` signs.
      {L"unescaped percent %", nullptr, p1p2p3},
      {L"unescaped %%% percents", nullptr, p1p2p3},
      {L"always escape percent otherwise %error", nullptr, p1p2p3},
      {L"% percents need to be escaped%", nullptr, p1p2p3},

      // Format string has invalid values for the placeholder index.
      {L"placeholder needs to be between 1 and 9, not %A", nullptr, p1p2p3},
      {L"placeholder %4  is > size of substitutions vector", nullptr, p1p2p3},
      {L"%1 is ok, but %8 or %9 is not ok", nullptr, p1p2p3},
      {L"%4", nullptr, p1p2p3},
      {L"abc=%1", nullptr, no_substitutions},
  };

  for (const auto& test_case : test_cases) {
    absl::optional<std::wstring> output = AppCommandRunner::FormatParameter(
        test_case.format_string, test_case.substitutions);
    if (test_case.expected_output) {
      EXPECT_EQ(output.value(), test_case.expected_output);
    } else {
      EXPECT_EQ(output, absl::nullopt);
    }
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

    if (test_case.input[0] != L"%1" || test_case.substitutions.size() != 1) {
      continue;
    }

    // The formatted output is now sent through ::CommandLineToArgvW to
    // verify that it produces the original substitution.
    std::wstring cmd = base::StrCat({L"process.exe ", command_line.value()});
    int num_args = 0;
    base::win::ScopedLocalAllocTyped<wchar_t*> argv_handle(
        ::CommandLineToArgvW(&cmd[0], &num_args));
    ASSERT_TRUE(argv_handle);
    EXPECT_EQ(num_args, 2) << "substitution '" << test_case.substitutions[0]
                           << "' gave command line '" << cmd
                           << "' which unexpectedly did not parse to a single "
                           << "argument.";

    EXPECT_EQ(argv_handle.get()[1], test_case.substitutions[0])
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
  HResultOr<AppCommandRunner> app_command_runner =
      AppCommandRunner::LoadAppCommand(GetTestScope(), kAppId1, kCmdId1);
  EXPECT_FALSE(app_command_runner.has_value());
}

TEST_F(AppCommandRunnerTest, NoCmd) {
  CreateAppCommandRegistry(GetTestScope(), kAppId1, kCmdId1, kCmdLineValid);
  HResultOr<AppCommandRunner> app_command_runner =
      AppCommandRunner::LoadAppCommand(GetTestScope(), kAppId1, kCmdId2);
  EXPECT_FALSE(app_command_runner.has_value());
}

TEST_F(AppCommandRunnerTest, RunAppCommandFormat) {
  const struct {
    const std::vector<std::wstring> input;
    const std::vector<std::wstring> substitutions;
    const int expected_exit_code;
  } test_cases[] = {
      {{L"/c", L"exit 7"}, {}, 7},
      {{L"/c", L"exit %1"}, {L"5420"}, 5420},
  };

  for (const auto& test_case : test_cases) {
    HResultOr<AppCommandRunner> app_command_runner;

    base::Process process;
    ASSERT_EQ(app_command_runner->Run(test_case.substitutions, process),
              E_UNEXPECTED);

    app_command_runner = CreateAppCommandRunner(
        kAppId1, kCmdId1,
        base::StrCat({cmd_exe_command_line_.GetCommandLineString(), L" ",
                      base::JoinString(test_case.input, L" ")}));
    ASSERT_TRUE(app_command_runner.has_value());
    ASSERT_HRESULT_SUCCEEDED(
        app_command_runner->Run(test_case.substitutions, process));

    int exit_code = 0;
    EXPECT_TRUE(process.WaitForExitWithTimeout(
        TestTimeouts::action_max_timeout(), &exit_code));
    EXPECT_EQ(exit_code, test_case.expected_exit_code);
  }
}

TEST_F(AppCommandRunnerTest, CheckChromeBrandedName) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_STREQ("Google Chrome", BROWSER_PRODUCT_NAME_STRING);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

TEST_F(AppCommandRunnerTest, RunProcessLauncherFormat) {
  if (!IsSystemInstall(GetTestScope())) {
    return;
  }

  const struct {
    const wchar_t* app_name;
    const wchar_t* app_version;
    const wchar_t* cmd_id;
    const std::vector<std::wstring> input;
    const int expected_exit_code;
    const int expected_hr;
  } test_cases[] = {
      {L"foo", L"1.0.0.0", L"cmd1", {L"/c", L"exit 7"}, 7, E_INVALIDARG},
      {L"foo", L"110.0.5434.0", L"cmd", {L"/c", L"exit 7"}, 7, E_INVALIDARG},
      {L"Chrome", L"110.0.5434.0", L"cmd", {L"/c", L"exit 7"}, 7, E_INVALIDARG},
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
  };

  for (const auto& test_case : test_cases) {
    HResultOr<AppCommandRunner> app_command_runner;
    base::Process process;
    ASSERT_EQ(app_command_runner->Run({}, process), E_UNEXPECTED);

    app_command_runner = CreateProcessLauncherRunner(
        kAppId1, test_case.app_name, test_case.app_version, test_case.cmd_id,
        base::StrCat({cmd_exe_command_line_.GetCommandLineString(), L" ",
                      base::JoinString(test_case.input, L" ")}));
    ASSERT_EQ(
        app_command_runner.has_value() ? S_OK : app_command_runner.error(),
        test_case.expected_hr);
    if (FAILED(test_case.expected_hr)) {
      continue;
    }

    ASSERT_HRESULT_SUCCEEDED(app_command_runner->Run({}, process));

    int exit_code = 0;
    EXPECT_TRUE(process.WaitForExitWithTimeout(
        TestTimeouts::action_max_timeout(), &exit_code));
    EXPECT_EQ(exit_code, test_case.expected_exit_code);
  }
}

TEST_F(AppCommandRunnerTest, RunBothFormats) {
  if (!IsSystemInstall(GetTestScope())) {
    return;
  }

  const struct {
    const wchar_t* cmd_id_to_execute;
    const wchar_t* cmd_id_appcommand;
    const std::vector<std::wstring> input_appcommand;
    const wchar_t* cmd_id_processlauncher;
    const std::vector<std::wstring> input_processlauncher;
    const int expected_exit_code;
  } test_cases[] = {
      // both formats in registry; AppCommand overrides ProcessLauncher entry.
      {L"cmd", L"cmd", {L"/c", L"exit 7"}, L"cmd", {L"/c", L"exit 14"}, 7},
      // only AppCommand format in registry.
      {L"cmd", L"cmd", {L"/c", L"exit 21"}, {}, {}, 21},
      // only ProcessLauncher format in registry.
      {L"cmd", {}, {}, L"cmd", {L"/c", L"exit 28"}, 28},
      // both formats in registry, but AppCommand has a different command ID, so
      // does not override ProcessLauncher entry.
      {L"cmd", L"cmd2", {L"/c", L"exit 7"}, L"cmd", {L"/c", L"exit 35"}, 35},
  };

  for (const auto& test_case : test_cases) {
    HResultOr<AppCommandRunner> app_command_runner;
    base::Process process;
    ASSERT_EQ(app_command_runner->Run({}, process), E_UNEXPECTED);

    if (test_case.cmd_id_appcommand) {
      CreateAppCommandRegistry(
          GetTestScope(), kAppId1, test_case.cmd_id_appcommand,
          base::StrCat({cmd_exe_command_line_.GetCommandLineString(), L" ",
                        base::JoinString(test_case.input_appcommand, L" ")}));
    }

    if (test_case.cmd_id_processlauncher) {
      CreateLaunchCmdElevatedRegistry(
          kAppId1, L"" BROWSER_PRODUCT_NAME_STRING, L"1.0.0.0",
          test_case.cmd_id_processlauncher,
          base::StrCat(
              {cmd_exe_command_line_.GetCommandLineString(), L" ",
               base::JoinString(test_case.input_processlauncher, L" ")}));
    }

    app_command_runner = AppCommandRunner::LoadAppCommand(
        GetTestScope(), kAppId1, test_case.cmd_id_to_execute);
    ASSERT_TRUE(app_command_runner.has_value());

    ASSERT_HRESULT_SUCCEEDED(app_command_runner->Run({}, process));

    int exit_code = 0;
    EXPECT_TRUE(process.WaitForExitWithTimeout(
        TestTimeouts::action_max_timeout(), &exit_code));
    EXPECT_EQ(exit_code, test_case.expected_exit_code);

    DeleteAppClientKey(GetTestScope(), kAppId1);
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

  base::ranges::for_each(test_cases, [&](const auto& test_case) {
    CreateAppCommandOSUpgradeRegistry(
        GetTestScope(), kAppId1, test_case.command_id,
        base::StrCat({cmd_exe_command_line_.GetCommandLineString(), L" ",
                      base::JoinString(test_case.input, L" ")}));
  });

  const std::vector<AppCommandRunner> app_command_runners =
      AppCommandRunner::LoadAutoRunOnOsUpgradeAppCommands(GetTestScope(),
                                                          kAppId1);

  ASSERT_EQ(std::size(app_command_runners), std::size(test_cases));
  base::ranges::for_each(
      app_command_runners, [&](const auto& app_command_runner) {
        base::Process process;
        EXPECT_HRESULT_SUCCEEDED(app_command_runner.Run({}, process));
        EXPECT_TRUE(process.WaitForExit(/*exit_code=*/nullptr));
      });
}

}  // namespace updater
