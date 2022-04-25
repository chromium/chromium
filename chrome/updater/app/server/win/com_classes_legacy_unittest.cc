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

const wchar_t kBadCmdLine[] = L"cmd.exe";
const wchar_t kCmdLineExit0[] = L"c:\\windows\\system32\\cmd.exe /c \"exit 0\"";
const wchar_t kCmdLineExitX[] =
    L"c:\\windows\\system32\\cmd.exe /c \"exit %1\"";

const wchar_t kCmdId1[] = L"command 1";
const wchar_t kCmdId2[] = L"command 2";

}  // namespace

class LegacyAppCommandWebImplTest : public testing::Test {
 protected:
  void TearDown() override {
    base::win::RegKey(UpdaterScopeToHKeyRoot(GetTestScope()), L"",
                      Wow6432(DELETE))
        .DeleteKey(GetClientKeyName(kAppId1).c_str());
  }

  template <typename T>
  absl::optional<std::wstring> MakeCommandLine(
      T web,
      const std::vector<std::wstring>& parameters) {
    absl::optional<std::wstring> cmd = web->FormatCommandLine(parameters);
    if (!cmd)
      return absl::nullopt;

    std::wstring command_line =
        web->executable_.value().find_first_of(L' ') == std::wstring::npos
            ? web->executable_.value()
            : base::CommandLine(web->executable_).GetCommandLineString();
    if (!cmd->empty()) {
      command_line.push_back(L' ');
      command_line.append(*cmd);
    }

    return command_line;
  }

  absl::optional<std::wstring> FormatCommandLine(
      const std::wstring& command_line_format,
      const std::vector<std::wstring>& parameters) {
    LegacyAppCommandWebImpl web;
    if (HRESULT hr = web.Initialize(command_line_format); FAILED(hr))
      return absl::nullopt;

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
    CreateCommand(kAppId1, kCmdId1, kCmdLineExit0);

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
};

TEST_F(LegacyAppCommandWebImplTest, NoArguments) {
  EXPECT_EQ(FormatCommandLine(L"process.exe", {}).value(), L"process.exe");
  EXPECT_EQ(FormatCommandLine(L"\"process.exe\"", {}).value(), L"process.exe");
  EXPECT_EQ(FormatCommandLine(L"\"c:\\path to\\process.exe\"", {}).value(),
            L"\"c:\\path to\\process.exe\"");
}

TEST_F(LegacyAppCommandWebImplTest, UnformattedParameters) {
  std::wstring process_name;
  std::wstring arguments;
  EXPECT_EQ(FormatCommandLine(L"process.exe abc=1", {}).value(),
            L"process.exe abc=1");
  EXPECT_EQ(FormatCommandLine(L"process.exe abc=1 xyz=2", {}).value(),
            L"process.exe abc=1 xyz=2");
  EXPECT_EQ(FormatCommandLine(L"process.exe  abc=1  xyz=2   q ", {}).value(),
            L"process.exe abc=1 xyz=2 q");
  EXPECT_EQ(FormatCommandLine(L"process.exe \"abc = 1\"", {}).value(),
            L"process.exe \"abc = 1\"");
  EXPECT_EQ(FormatCommandLine(L"process.exe abc\" = \"1", {}).value(),
            L"process.exe \"abc = 1\"");

  EXPECT_EQ(FormatCommandLine(L"\"c:\\path to\\process.exe\" \"abc = 1\"", {})
                .value(),
            L"\"c:\\path to\\process.exe\" \"abc = 1\"");
  EXPECT_EQ(FormatCommandLine(L"\"c:\\path to\\process.exe\" abc\" = \"1", {})
                .value(),
            L"\"c:\\path to\\process.exe\" \"abc = 1\"");
}

TEST_F(LegacyAppCommandWebImplTest, SimpleParameters) {
  std::vector<std::wstring> parameters;
  parameters.push_back(L"p1");
  parameters.push_back(L"p2");
  parameters.push_back(L"p3");

  EXPECT_EQ(FormatCommandLine(L"process.exe abc=%1", parameters).value(),
            L"process.exe abc=p1");
  EXPECT_EQ(
      FormatCommandLine(L"process.exe abc=%1 %3 %2=x", parameters).value(),
      L"process.exe abc=p1 p3 p2=x");

  EXPECT_EQ(FormatCommandLine(L"process.exe %4", parameters), absl::nullopt);
}

TEST_F(LegacyAppCommandWebImplTest, SimpleParametersNoFormatParameters) {
  EXPECT_EQ(FormatCommandLine(L"process.exe abc=%1", {}), absl::nullopt);
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

  for (const auto& test_case : test_cases) {
    EXPECT_EQ(
        FormatCommandLine(base::StrCat({L"process.exe ", test_case.input}),
                          {L"p1", L"p2", L"p3"})
            .value(),
        base::StrCat({L"process.exe ", test_case.output}));
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

  for (const wchar_t* test_case : test_cases) {
    EXPECT_EQ(FormatCommandLine(base::StrCat({L"process.exe ", test_case}),
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

  for (const auto& test_case : test_cases) {
    std::wstring command_line =
        FormatCommandLine(L"process.exe %1", {test_case.input}).value();
    EXPECT_EQ(command_line, base::StrCat({L"process.exe ", test_case.output}));

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
  EXPECT_EQ(FormatCommandLine(kAppId1, kCmdId1, kCmdLineExit0, {}).value(),
            kCmdLineExit0);
}

TEST_F(LegacyAppCommandWebImplTest, Execute) {
  base::FilePath temp_file;
  EXPECT_TRUE(base::CreateTemporaryFile(&temp_file));
  EXPECT_TRUE(base::DeleteFile(temp_file));

  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  ASSERT_HRESULT_SUCCEEDED(CreateAppCommandWeb(
      kAppId1, kCmdId1,
      base::StrCat(
          {L"c:\\windows\\system32\\cmd.exe /c \"echo hello world! > \"",
           temp_file.value(), L"\"\""}),
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
  EXPECT_EQ(exit_code, 0U);

  EXPECT_TRUE(base::PathExists(temp_file));
  EXPECT_TRUE(base::DeleteFile(temp_file));
}

TEST_F(LegacyAppCommandWebImplTest, ExecuteParameterizedCommand) {
  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  ASSERT_HRESULT_SUCCEEDED(
      CreateAppCommandWeb(kAppId1, kCmdId1, kCmdLineExitX, app_command_web));

  ASSERT_HRESULT_SUCCEEDED(app_command_web->execute(
      base::win::ScopedVariant(L"3"), base::win::ScopedVariant::kEmptyVariant,
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
  EXPECT_EQ(exit_code, 3U);
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
  ASSERT_HRESULT_SUCCEEDED(CreateAppCommandWeb(
      kAppId1, kCmdId1,
      command_line.GetCommandLineStringWithUnsafeInsertSequences(),
      app_command_web));

  ASSERT_HRESULT_SUCCEEDED(
      app_command_web->execute(base::win::ScopedVariant(attr.name.c_str()),
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
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
}

}  // namespace updater
