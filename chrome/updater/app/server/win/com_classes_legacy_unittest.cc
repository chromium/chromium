// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/com_classes_legacy.h"

#include <shellapi.h>
#include <windows.h>
#include <wrl/client.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_variant.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/test/test_strings.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/win_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

const wchar_t kAppId1[] = L"{3B1A3CCA-0525-4418-93E6-A0DB3398EC9B}";

const wchar_t kCmdExe[] = L"cmd.exe";
const wchar_t kBadCmdLine[] = L"\"c:\\Program Files\\cmd.exe\"";
const wchar_t kCmdLineValid[] =
    L"\"C:\\Program Files\\Windows Media Player\\wmpnscfg.exe\" /Close";

const wchar_t kCmdId1[] = L"command 1";
const wchar_t kCmdId2[] = L"command 2";

}  // namespace

class LegacyAppCommandWebImplTest : public testing::Test {
 protected:
  LegacyAppCommandWebImplTest()
      : test_process_command_line_(base::CommandLine::NO_PROGRAM) {}
  ~LegacyAppCommandWebImplTest() override = default;

  void SetUp() override {
    base::FilePath system_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SYSTEM, &system_path));

    const base::FilePath from_test_process = system_path.Append(kCmdExe);
    if (GetTestScope() == UpdaterScope::kUser) {
      test_process_command_line_ = base::CommandLine(from_test_process);
      return;
    }

    base::FilePath programfiles_path;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_PROGRAM_FILES, &programfiles_path));
    ASSERT_TRUE(base::CreateTemporaryDirInDir(
        programfiles_path, L"com_classes_legacy_unittest", &temp_directory_));
    base::FilePath test_process_path;
    test_process_path = temp_directory_.Append(kCmdExe);

    ASSERT_TRUE(base::CopyFile(from_test_process, test_process_path));
    test_process_command_line_ = base::CommandLine(test_process_path);
  }

  void TearDown() override {
    base::win::RegKey(UpdaterScopeToHKeyRoot(GetTestScope()), L"",
                      Wow6432(DELETE))
        .DeleteKey(GetClientKeyName(kAppId1).c_str());

    if (!temp_directory_.empty())
      base::DeletePathRecursively(temp_directory_);
  }

  template <typename T>
  absl::optional<std::wstring> MakeCommandLine(
      T web,
      const std::vector<std::wstring>& parameters) {
    absl::optional<std::wstring> cmd = web->FormatCommandLine(parameters);
    if (!cmd)
      return absl::nullopt;

    const std::wstring command_line =
        web->executable_.value().find_first_of(L' ') == std::wstring::npos
            ? web->executable_.value()
            : base::CommandLine(web->executable_).GetCommandLineString();
    return cmd->empty() ? command_line
                        : base::StrCat({command_line, L" ", *cmd});
  }

  absl::optional<std::wstring> FormatCommandLine(
      const std::wstring& command_line_format,
      const std::vector<std::wstring>& parameters) {
    LegacyAppCommandWebImpl web;
    if (HRESULT hr = web.Initialize(GetTestScope(), command_line_format);
        FAILED(hr)) {
      return absl::nullopt;
    }

    return MakeCommandLine(&web, parameters);
  }

  absl::optional<std::wstring> FormatCommandLine(
      const std::wstring& app_id,
      const std::wstring& command_id,
      const std::wstring& command_line_format,
      const std::vector<std::wstring>& parameters) {
    CreateAppClientKey(app_id);
    CreateCommand(app_id, command_id, command_line_format);

    Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
    if (HRESULT hr = LegacyAppCommandWebImpl::CreateLegacyAppCommandWebImpl(
            GetTestScope(), app_id, command_id, app_command_web);
        FAILED(hr)) {
      return absl::nullopt;
    }

    return MakeCommandLine(app_command_web, parameters);
  }

  HRESULT CreateAppCommandWeb(
      const std::wstring& app_id,
      const std::wstring& command_id,
      const std::wstring& command_line_format,
      Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl>& app_command_web) {
    CreateAppClientKey(app_id);
    CreateCommand(app_id, command_id, command_line_format);

    return LegacyAppCommandWebImpl::CreateLegacyAppCommandWebImpl(
        GetTestScope(), app_id, command_id, app_command_web);
  }

  void NoAppTest() {
    Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
    EXPECT_HRESULT_FAILED(
        LegacyAppCommandWebImpl::CreateLegacyAppCommandWebImpl(
            GetTestScope(), kAppId1, kCmdId1, app_command_web));
  }

  void NoCmdTest() {
    Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
    CreateAppClientKey(kAppId1);
    CreateCommand(kAppId1, kCmdId1, kCmdLineValid);

    EXPECT_HRESULT_FAILED(
        LegacyAppCommandWebImpl::CreateLegacyAppCommandWebImpl(
            GetTestScope(), kAppId1, kCmdId2, app_command_web));
  }

  std::wstring GetClientKeyName(const std::wstring& app_id) {
    return base::StrCat({CLIENTS_KEY, app_id});
  }

  std::wstring GetCommandKeyName(const std::wstring& app_id,
                                 const std::wstring& command_id) {
    return base::StrCat(
        {CLIENTS_KEY, app_id, L"\\", kRegKeyCommands, command_id});
  }

  void CreateAppClientKey(const std::wstring& app_id) {
    base::win::RegKey client_key;
    EXPECT_EQ(
        client_key.Create(UpdaterScopeToHKeyRoot(GetTestScope()),
                          GetClientKeyName(app_id).c_str(), Wow6432(KEY_WRITE)),
        ERROR_SUCCESS);
  }

  void CreateCommand(const std::wstring& app_id,
                     const std::wstring& cmd_id,
                     const std::wstring& cmd_line) {
    base::win::RegKey command_key;
    EXPECT_EQ(command_key.Create(UpdaterScopeToHKeyRoot(GetTestScope()),
                                 GetCommandKeyName(app_id, cmd_id).c_str(),
                                 Wow6432(KEY_WRITE)),
              ERROR_SUCCESS);
    EXPECT_EQ(command_key.WriteValue(kRegValueCommandLine, cmd_line.c_str()),
              ERROR_SUCCESS);
  }

  void WaitForUpdateCompletion(
      Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl>& app_command_web,
      const base::TimeDelta& timeout) {
    const base::TimeTicks start_time = base::TimeTicks::Now();

    while (base::TimeTicks::Now() - start_time < timeout) {
      UINT status = 0;
      EXPECT_HRESULT_SUCCEEDED(app_command_web->get_status(&status));
      if (status == COMMAND_STATUS_COMPLETE)
        return;

      base::WaitableEvent().TimedWait(TestTimeouts::tiny_timeout());
    }
  }

  std::wstring GetCommandLine(int key, const std::wstring& exe_name) {
    base::FilePath programfiles_path;
    EXPECT_TRUE(base::PathService::Get(key, &programfiles_path));
    return base::CommandLine(programfiles_path.Append(exe_name))
        .GetCommandLineString();
  }

  base::CommandLine test_process_command_line_;
  base::FilePath temp_directory_;
};

TEST_F(LegacyAppCommandWebImplTest, InvalidPaths) {
  // Relative paths are invalid.
  EXPECT_EQ(FormatCommandLine(L"process.exe", {}), absl::nullopt);

  if (GetTestScope() == UpdaterScope::kUser)
    return;

  // Paths not under %ProgramFiles% or %ProgramFilesX86% are invalid for system.
  EXPECT_EQ(FormatCommandLine(L"\"C:\\foobar\\process.exe\"", {}),
            absl::nullopt);
  EXPECT_EQ(FormatCommandLine(L"C:\\ProgramFiles\\process.exe", {}),
            absl::nullopt);
  EXPECT_EQ(FormatCommandLine(L"C:\\windows\\system32\\cmd.exe", {}),
            absl::nullopt);
}

TEST_F(LegacyAppCommandWebImplTest, ProgramFilesPaths) {
  for (const int key : {base::DIR_PROGRAM_FILES, base::DIR_PROGRAM_FILESX86,
                        base::DIR_PROGRAM_FILES6432}) {
    const std::wstring process_command_line =
        GetCommandLine(key, L"process.exe");
    EXPECT_EQ(FormatCommandLine(process_command_line, {}).value(),
              process_command_line);
  }
}

TEST_F(LegacyAppCommandWebImplTest, UnformattedParameters) {
  std::wstring process_name;
  std::wstring arguments;
  const std::wstring process_command_line =
      GetCommandLine(base::DIR_PROGRAM_FILES, L"process.exe");

  EXPECT_EQ(FormatCommandLine(process_command_line + L" abc=1", {}).value(),
            process_command_line + L" abc=1");
  EXPECT_EQ(
      FormatCommandLine(process_command_line + L" abc=1 xyz=2", {}).value(),
      process_command_line + L" abc=1 xyz=2");
  EXPECT_EQ(FormatCommandLine(process_command_line + L"  abc=1  xyz=2   q ", {})
                .value(),
            process_command_line + L" abc=1 xyz=2 q");
  EXPECT_EQ(
      FormatCommandLine(process_command_line + L" \"abc = 1\"", {}).value(),
      process_command_line + L" \"abc = 1\"");
  EXPECT_EQ(
      FormatCommandLine(process_command_line + L" abc\" = \"1", {}).value(),
      process_command_line + L" \"abc = 1\"");

  EXPECT_EQ(
      FormatCommandLine(process_command_line + L" \"abc = 1\"", {}).value(),
      process_command_line + L" \"abc = 1\"");
  EXPECT_EQ(
      FormatCommandLine(process_command_line + L" abc\" = \"1", {}).value(),
      process_command_line + L" \"abc = 1\"");
}

TEST_F(LegacyAppCommandWebImplTest, SimpleParameters) {
  const std::vector<std::wstring> parameters = {L"p1", L"p2", L"p3"};
  const std::wstring process_command_line =
      GetCommandLine(base::DIR_PROGRAM_FILES, L"process.exe");

  EXPECT_EQ(
      FormatCommandLine(process_command_line + L" abc=%1", parameters).value(),
      process_command_line + L" abc=p1");
  EXPECT_EQ(
      FormatCommandLine(process_command_line + L" abc=%1 %3 %2=x", parameters)
          .value(),
      process_command_line + L" abc=p1 p3 p2=x");

  EXPECT_EQ(FormatCommandLine(process_command_line + L" %4", parameters),
            absl::nullopt);
}

TEST_F(LegacyAppCommandWebImplTest, SimpleParametersNoFormatParameters) {
  EXPECT_EQ(
      FormatCommandLine(
          GetCommandLine(base::DIR_PROGRAM_FILES, L"process.exe") + L" abc=%1",
          {}),
      absl::nullopt);
}

TEST_F(LegacyAppCommandWebImplTest, FormatParametersSucceeds) {
  const struct {
    const wchar_t* input;
    const wchar_t* output;
  } test_cases[] = {
      {L"%1", L"p1"},    {L"%%1", L"%1"},
      {L"%%%1", L"%p1"}, {L"abc%%def%%", L"abc%def%"},
      {L"%12", L"p12"},  {L"%1%2", L"p1p2"},
  };
  const std::wstring process_command_line =
      GetCommandLine(base::DIR_PROGRAM_FILES, L"process.exe");

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(FormatCommandLine(
                  base::StrCat({process_command_line, L" ", test_case.input}),
                  {L"p1", L"p2", L"p3"})
                  .value(),
              base::StrCat({process_command_line, L" ", test_case.output}));
  }
}

TEST_F(LegacyAppCommandWebImplTest, FormatParametersFails) {
  const wchar_t* test_cases[] = {
      L"unescaped percent %",
      L"unescaped %%% percents",
      L"always escape percent, otherwise %foobar",
      L"% percents need to be escaped%",
      L"placeholder needs to be between 1 and 9, not %A",
      L"placeholder %4  is > size of input substitutions",
      L"%1 is ok, but %8 or %9 is not ok",
  };
  const std::wstring process_command_line =
      GetCommandLine(base::DIR_PROGRAM_FILES, L"process.exe");

  for (const wchar_t* test_case : test_cases) {
    EXPECT_EQ(
        FormatCommandLine(base::StrCat({process_command_line, L" ", test_case}),
                          {L"p1", L"p2", L"p3"}),
        absl::nullopt);
  }
}

TEST_F(LegacyAppCommandWebImplTest, ParameterQuoting) {
  const struct {
    const wchar_t* input;
    const wchar_t* output;
  } test_cases[] = {
      // embedded \ and \\.
      {L"a\\b\\\\c", L"a\\b\\\\c"},
      // trailing \.
      {L"a\\", L"a\\"},
      // trailing \\.
      {L"a\\\\", L"a\\\\"},
      // only \\.
      {L"\\\\", L"\\\\"},
      // empty.
      {L"", L"\"\""},
      // embedded quote.
      {L"a\"b", L"a\\\"b"},
      // trailing quote.
      {L"abc\"", L"abc\\\""},
      // embedded \\".
      {L"a\\\\\"b", L"a\\\\\\\\\\\"b"},
      // trailing \\".
      {L"abc\\\\\"", L"abc\\\\\\\\\\\""},
      // embedded space.
      {L"abc def", L"\"abc def\""},
      // trailing space.
      {L"abcdef ", L"\"abcdef \""},
      // leading space.
      {L" abcdef", L"\" abcdef\""},
  };
  const std::wstring process_command_line =
      GetCommandLine(base::DIR_PROGRAM_FILES, L"process.exe");

  for (const auto& test_case : test_cases) {
    std::wstring command_line =
        FormatCommandLine(process_command_line + L" %1", {test_case.input})
            .value();
    EXPECT_EQ(command_line,
              base::StrCat({process_command_line, L" ", test_case.output}));

    // The formatted output is now sent through ::CommandLineToArgvW to verify
    // that it produces the original input.
    int num_args = 0;
    ScopedLocalAlloc argv_handle(
        ::CommandLineToArgvW(&command_line[0], &num_args));
    ASSERT_TRUE(argv_handle.is_valid());
    EXPECT_EQ(num_args, 2) << "Input '" << test_case.input
                           << "' gave command line '" << command_line
                           << "' which unexpectedly did not parse to a single "
                           << "argument.";

    EXPECT_STREQ(reinterpret_cast<const wchar_t**>(argv_handle.get())[1],
                 test_case.input)
        << "Input '" << test_case.input << "' gave command line '"
        << command_line << "' which did not parse back to the "
        << "original input";
  }
}

TEST_F(LegacyAppCommandWebImplTest, NoApp) {
  NoAppTest();
}

TEST_F(LegacyAppCommandWebImplTest, NoCmd) {
  NoCmdTest();
}

TEST_F(LegacyAppCommandWebImplTest, LoadCommand) {
  EXPECT_EQ(FormatCommandLine(kAppId1, kCmdId1, kCmdLineValid, {}).value(),
            kCmdLineValid);
}

TEST_F(LegacyAppCommandWebImplTest, Execute) {
  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  ASSERT_HRESULT_SUCCEEDED(CreateAppCommandWeb(
      kAppId1, kCmdId1,
      base::StrCat({test_process_command_line_.GetCommandLineString(),
                    L" /c \"exit 7\""}),
      app_command_web));

  UINT status = 0;
  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_status(&status));
  EXPECT_EQ(status, COMMAND_STATUS_INIT);
  DWORD exit_code = 0;
  EXPECT_HRESULT_FAILED(app_command_web->get_exitCode(&exit_code));

  ASSERT_HRESULT_SUCCEEDED(
      app_command_web->execute(base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant));

  WaitForUpdateCompletion(app_command_web, TestTimeouts::action_max_timeout());

  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_status(&status));
  EXPECT_EQ(status, COMMAND_STATUS_COMPLETE);
  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_exitCode(&exit_code));
  EXPECT_EQ(exit_code, 7U);
}

TEST_F(LegacyAppCommandWebImplTest, ExecuteParameterizedCommand) {
  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  ASSERT_HRESULT_SUCCEEDED(CreateAppCommandWeb(
      kAppId1, kCmdId1,
      base::StrCat({test_process_command_line_.GetCommandLineString(),
                    L" /c \"exit %1\""}),
      app_command_web));

  ASSERT_HRESULT_SUCCEEDED(
      app_command_web->execute(base::win::ScopedVariant(L"5420"),
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant));
  WaitForUpdateCompletion(app_command_web, TestTimeouts::action_max_timeout());

  DWORD exit_code = 0;
  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_exitCode(&exit_code));
  EXPECT_EQ(exit_code, 5420U);
}

TEST_F(LegacyAppCommandWebImplTest, FailedToLaunchStatus) {
  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  ASSERT_HRESULT_SUCCEEDED(
      CreateAppCommandWeb(kAppId1, kCmdId1, kBadCmdLine, app_command_web));

  EXPECT_HRESULT_FAILED(
      app_command_web->execute(base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant));

  DWORD exit_code = 0;
  EXPECT_HRESULT_FAILED(app_command_web->get_exitCode(&exit_code));
}

TEST_F(LegacyAppCommandWebImplTest, CommandRunningStatus) {
  if (GetTestScope() == UpdaterScope::kSystem)
    return;

  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  base::CommandLine command_line = GetTestProcessCommandLine(GetTestScope());

  const std::wstring event_name =
      base::StrCat({kTestProcessExecutableName, L"-",
                    base::NumberToWString(::GetCurrentProcessId())});
  NamedObjectAttributes attr;
  GetNamedObjectAttributes(event_name.c_str(), GetTestScope(), &attr);

  base::WaitableEvent event(base::win::ScopedHandle(
      ::CreateEvent(&attr.sa, FALSE, FALSE, attr.name.c_str())));
  ASSERT_NE(event.handle(), nullptr);

  command_line.AppendSwitchNative(kTestEventToWaitOn, L"%1");
  command_line.AppendSwitchNative(kTestExitCode, L"%2");

  ASSERT_HRESULT_SUCCEEDED(CreateAppCommandWeb(
      kAppId1, kCmdId1,
      command_line.GetCommandLineStringWithUnsafeInsertSequences(),
      app_command_web));

  ASSERT_HRESULT_SUCCEEDED(app_command_web->execute(
      base::win::ScopedVariant(attr.name.c_str()),
      base::win::ScopedVariant(L"999"), base::win::ScopedVariant::kEmptyVariant,
      base::win::ScopedVariant::kEmptyVariant,
      base::win::ScopedVariant::kEmptyVariant,
      base::win::ScopedVariant::kEmptyVariant,
      base::win::ScopedVariant::kEmptyVariant,
      base::win::ScopedVariant::kEmptyVariant,
      base::win::ScopedVariant::kEmptyVariant));

  UINT status = 0;
  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_status(&status));
  EXPECT_EQ(status, COMMAND_STATUS_RUNNING);

  event.Signal();

  WaitForUpdateCompletion(app_command_web, TestTimeouts::action_max_timeout());

  DWORD exit_code = 0;
  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_exitCode(&exit_code));
  EXPECT_EQ(exit_code, 999U);
}

}  // namespace updater
