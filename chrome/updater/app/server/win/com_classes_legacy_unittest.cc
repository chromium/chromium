// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/com_classes_legacy.h"

#include <windows.h>

#include <shellapi.h>
#include <wrl/client.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_variant.h"
#include "base/win/win_util.h"
#include "build/branding_buildflags.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/test/unit_test_util_win.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/test/test_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater::test {
namespace {

constexpr wchar_t kAppId1[] = L"{3B1A3CCA-0525-4418-93E6-A0DB3398EC9B}";

constexpr wchar_t kBadCmdLine[] = L"\"c:\\Program Files (x86)\\cmd.exe\"";
constexpr wchar_t kCmdLineValid[] =
    L"\"C:\\Program Files\\Windows Media Player\\wmpnscfg.exe\" /Close";

constexpr wchar_t kCmdId1[] = L"command 1";
constexpr wchar_t kCmdId2[] = L"command 2";

}  // namespace

class LegacyAppCommandWebImplTest : public testing::Test {
 protected:
  LegacyAppCommandWebImplTest()
      : cmd_exe_command_line_(base::CommandLine::NO_PROGRAM) {}
  ~LegacyAppCommandWebImplTest() override = default;

  void SetUp() override {
    SetupCmdExe(GetUpdaterScopeForTesting(), cmd_exe_command_line_,
                temp_programfiles_dir_);
  }

  void TearDown() override {
    DeleteAppClientKey(GetUpdaterScopeForTesting(), kAppId1);
  }

  [[nodiscard]] HRESULT CreateAppCommandWeb(
      const std::wstring& app_id,
      const std::wstring& command_id,
      const std::wstring& command_line_format,
      LegacyAppCommandWebImpl::PingSender ping_sender,
      Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl>& app_command_web) {
    CreateAppCommandRegistry(GetUpdaterScopeForTesting(), app_id, command_id,
                             command_line_format);
    return MakeAndInitializeComObject<LegacyAppCommandWebImpl>(
        app_command_web, GetUpdaterScopeForTesting(), app_id, command_id,
        ping_sender);
  }

  void WaitForUpdateCompletion(
      Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl>& app_command_web) {
    EXPECT_TRUE(test::WaitFor([&] {
      UINT status = 0;
      EXPECT_HRESULT_SUCCEEDED(app_command_web->get_status(&status));
      return status == COMMAND_STATUS_COMPLETE;
    }));
  }

  base::test::TaskEnvironment environment_;
  base::CommandLine cmd_exe_command_line_;
  base::ScopedTempDir temp_programfiles_dir_;
};

TEST_F(LegacyAppCommandWebImplTest, NoApp) {
  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  EXPECT_HRESULT_FAILED(MakeAndInitializeComObject<LegacyAppCommandWebImpl>(
      app_command_web, GetUpdaterScopeForTesting(), kAppId1, kCmdId1));
}

TEST_F(LegacyAppCommandWebImplTest, NoCmd) {
  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  CreateAppCommandRegistry(GetUpdaterScopeForTesting(), kAppId1, kCmdId1,
                           kCmdLineValid);
  EXPECT_HRESULT_FAILED(MakeAndInitializeComObject<LegacyAppCommandWebImpl>(
      app_command_web, GetUpdaterScopeForTesting(), kAppId1, kCmdId2));
}

TEST_F(LegacyAppCommandWebImplTest, Execute) {
  bool ping_sent = false;
  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  ASSERT_HRESULT_SUCCEEDED(CreateAppCommandWeb(
      kAppId1, kCmdId1,
      base::StrCat(
          {cmd_exe_command_line_.GetCommandLineString(), L" /c \"exit 7\""}),
      base::BindLambdaForTesting(
          [&ping_sent](UpdaterScope scope, const std::string& app_id,
                       const std::string& command_id,
                       LegacyAppCommandWebImpl::ErrorParams error_params) {
            ping_sent = true;
            EXPECT_EQ(GetUpdaterScopeForTesting(), scope);
            EXPECT_EQ(app_id, base::WideToASCII(kAppId1));
            EXPECT_EQ(command_id, base::WideToASCII(kCmdId1));
            EXPECT_EQ(error_params.error_code, 7);
            EXPECT_EQ(error_params.extra_code1, 0);
          }),
      app_command_web));
  UINT status = 0;
  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_status(&status));
  EXPECT_EQ(status, COMMAND_STATUS_INIT);
  DWORD exit_code = 0;
  EXPECT_EQ(app_command_web->get_exitCode(&exit_code), S_FALSE);

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

  WaitForUpdateCompletion(app_command_web);

  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_status(&status));
  EXPECT_EQ(status, COMMAND_STATUS_COMPLETE);
  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_exitCode(&exit_code));
  EXPECT_EQ(exit_code, 7U);
  EXPECT_TRUE(ping_sent);
}

TEST_F(LegacyAppCommandWebImplTest, ExecuteParameterizedCommand) {
  bool ping_sent = false;
  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  ASSERT_HRESULT_SUCCEEDED(CreateAppCommandWeb(
      kAppId1, kCmdId1,
      base::StrCat(
          {cmd_exe_command_line_.GetCommandLineString(), L" /c \"exit %1\""}),
      base::BindLambdaForTesting(
          [&ping_sent](UpdaterScope scope, const std::string& app_id,
                       const std::string& command_id,
                       LegacyAppCommandWebImpl::ErrorParams error_params) {
            ping_sent = true;
            EXPECT_EQ(GetUpdaterScopeForTesting(), scope);
            EXPECT_EQ(app_id, base::WideToASCII(kAppId1));
            EXPECT_EQ(command_id, base::WideToASCII(kCmdId1));
            EXPECT_EQ(error_params.error_code, 5420);
            EXPECT_EQ(error_params.extra_code1, 0);
          }),
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
  WaitForUpdateCompletion(app_command_web);

  DWORD exit_code = 0;
  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_exitCode(&exit_code));
  EXPECT_EQ(exit_code, 5420U);
  EXPECT_TRUE(ping_sent);
}

TEST_F(LegacyAppCommandWebImplTest, FailedToLaunchStatus) {
  bool ping_sent = false;
  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  ASSERT_HRESULT_SUCCEEDED(CreateAppCommandWeb(
      kAppId1, kCmdId1, kBadCmdLine,
      base::BindLambdaForTesting(
          [&ping_sent](UpdaterScope scope, const std::string& app_id,
                       const std::string& command_id,
                       LegacyAppCommandWebImpl::ErrorParams error_params) {
            ping_sent = true;
            EXPECT_EQ(GetUpdaterScopeForTesting(), scope);
            EXPECT_EQ(app_id, base::WideToASCII(kAppId1));
            EXPECT_EQ(command_id, base::WideToASCII(kCmdId1));
            EXPECT_EQ(error_params.error_code,
                      HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
            EXPECT_EQ(error_params.extra_code1, kErrorAppCommandLaunchFailed);
          }),
      app_command_web));

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
  EXPECT_EQ(app_command_web->get_exitCode(&exit_code), S_FALSE);
  EXPECT_TRUE(ping_sent);
}

TEST_F(LegacyAppCommandWebImplTest, CommandRunningStatus) {
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    return;
  }

  bool ping_sent = false;
  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  base::CommandLine command_line = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());

  command_line.AppendSwitchNative(kTestEventToWaitOn, L"%1");
  command_line.AppendSwitchNative(kTestExitCode, L"%2");

  ASSERT_HRESULT_SUCCEEDED(CreateAppCommandWeb(
      kAppId1, kCmdId1,
      command_line.GetCommandLineStringWithUnsafeInsertSequences(),
      base::BindLambdaForTesting(
          [&ping_sent](UpdaterScope scope, const std::string& app_id,
                       const std::string& command_id,
                       LegacyAppCommandWebImpl::ErrorParams error_params) {
            ping_sent = true;
            EXPECT_EQ(GetUpdaterScopeForTesting(), scope);
            EXPECT_EQ(app_id, base::WideToASCII(kAppId1));
            EXPECT_EQ(command_id, base::WideToASCII(kCmdId1));
            EXPECT_EQ(error_params.error_code, 999);
            EXPECT_EQ(error_params.extra_code1, 0);
          }),
      app_command_web));

  test::EventHolder event_holder(test::CreateWaitableEventForTest());

  ASSERT_HRESULT_SUCCEEDED(app_command_web->execute(
      base::win::ScopedVariant(event_holder.name.c_str()),
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

  event_holder.event.Signal();

  WaitForUpdateCompletion(app_command_web);

  DWORD exit_code = 0;
  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_exitCode(&exit_code));
  EXPECT_EQ(exit_code, 999U);
  EXPECT_TRUE(ping_sent);
}

TEST_F(LegacyAppCommandWebImplTest, CheckLegacyTypeLibAndInterfaceExist) {
  base::FilePath typelib_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &typelib_path));

  Microsoft::WRL::ComPtr<ITypeLib> type_lib;
  ASSERT_HRESULT_SUCCEEDED(::LoadTypeLib(
      typelib_path.Append(GetExecutableRelativePath())
          .Append(GetComTypeLibResourceIndex(__uuidof(IAppCommandWeb)))
          .value()
          .c_str(),
      &type_lib));

  Microsoft::WRL::ComPtr<ITypeInfo> type_info;
  EXPECT_HRESULT_SUCCEEDED(
      type_lib->GetTypeInfoOfGuid(__uuidof(IAppCommandWeb), &type_info))
      << " Could not load type info for legacy interface IAppCommandWeb, "
         "IID_IAppCommand: "
      << StringFromGuid(__uuidof(IAppCommandWeb));
}

TEST(LegacyCOMClassesTest, CheckLegacyInterfaceIDs) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(StringFromGuid(__uuidof(GoogleUpdate3WebUserClass)),
            L"{22181302-A8A6-4F84-A541-E5CBFC70CC43}");
  EXPECT_EQ(StringFromGuid(__uuidof(GoogleUpdate3WebSystemClass)),
            L"{8A1D4361-2C08-4700-A351-3EAA9CBFF5E4}");
  EXPECT_EQ(StringFromGuid(__uuidof(GoogleUpdate3WebServiceClass)),
            L"{534F5323-3569-4F42-919D-1E1CF93E5BF6}");
  EXPECT_EQ(StringFromGuid(__uuidof(PolicyStatusUserClass)),
            L"{6DDCE70D-A4AE-4E97-908C-BE7B2DB750AD}");
  EXPECT_EQ(StringFromGuid(__uuidof(PolicyStatusSystemClass)),
            L"{521FDB42-7130-4806-822A-FC5163FAD983}");
  EXPECT_EQ(StringFromGuid(__uuidof(ProcessLauncherClass)),
            L"{ABC01078-F197-4B0B-ADBC-CFE684B39C82}");
  EXPECT_EQ(StringFromGuid(__uuidof(IAppVersionWeb)),
            L"{0CD01D1E-4A1C-489D-93B9-9B6672877C57}");
  EXPECT_EQ(StringFromGuid(__uuidof(ICurrentState)),
            L"{247954F9-9EDC-4E68-8CC3-150C2B89EADF}");
  EXPECT_EQ(StringFromGuid(__uuidof(IGoogleUpdate3Web)),
            L"{494B20CF-282E-4BDD-9F5D-B70CB09D351E}");
  EXPECT_EQ(StringFromGuid(__uuidof(IAppBundleWeb)),
            L"{DD42475D-6D46-496A-924E-BD5630B4CBBA}");
  EXPECT_EQ(StringFromGuid(__uuidof(IAppWeb)),
            L"{18D0F672-18B4-48E6-AD36-6E6BF01DBBC4}");
  EXPECT_EQ(StringFromGuid(__uuidof(IAppCommandWeb)),
            L"{8476CE12-AE1F-4198-805C-BA0F9B783F57}");
  EXPECT_EQ(StringFromGuid(__uuidof(IPolicyStatus)),
            L"{F63F6F8B-ACD5-413C-A44B-0409136D26CB}");
  EXPECT_EQ(StringFromGuid(__uuidof(IPolicyStatus2)),
            L"{34527502-D3DB-4205-A69B-789B27EE0414}");
  EXPECT_EQ(StringFromGuid(__uuidof(IPolicyStatus3)),
            L"{05A30352-EB25-45B6-8449-BCA7B0542CE5}");
  EXPECT_EQ(StringFromGuid(__uuidof(IPolicyStatus4)),
            L"{FD0FDA43-AF97-4F1C-BD68-3355FB4F1C92}");
  EXPECT_EQ(StringFromGuid(__uuidof(IPolicyStatusValue)),
            L"{27634814-8E41-4C35-8577-980134A96544}");
  EXPECT_EQ(StringFromGuid(__uuidof(IProcessLauncher)),
            L"{128C2DA6-2BC0-44C0-B3F6-4EC22E647964}");
  EXPECT_EQ(StringFromGuid(__uuidof(IProcessLauncher2)),
            L"{D106AB5F-A70E-400E-A21B-96208C1D8DBB}");
#else
  EXPECT_EQ(StringFromGuid(__uuidof(GoogleUpdate3WebUserClass)),
            L"{75828ED1-7BE8-45D0-8950-AA85CBF74510}");
  EXPECT_EQ(StringFromGuid(__uuidof(GoogleUpdate3WebSystemClass)),
            L"{283209B7-C761-41CA-BE8D-B5321CD78FD6}");
  EXPECT_EQ(StringFromGuid(__uuidof(GoogleUpdate3WebServiceClass)),
            L"{B52C8B56-9541-4B78-9B2F-665366B78A9C}");
  EXPECT_EQ(StringFromGuid(__uuidof(PolicyStatusUserClass)),
            L"{4DAC24AB-B340-4B7E-AD01-1504A7F59EEA}");
  EXPECT_EQ(StringFromGuid(__uuidof(PolicyStatusSystemClass)),
            L"{83FE19AC-72A6-4A72-B136-724444121586}");
  EXPECT_EQ(StringFromGuid(__uuidof(ProcessLauncherClass)),
            L"{811A664F-703E-407C-A323-E6E31D1EFFA0}");
  EXPECT_EQ(StringFromGuid(__uuidof(IAppVersionWeb)),
            L"{3057E1F8-2498-4C19-99B5-F7F207DA4DC7}");
  EXPECT_EQ(StringFromGuid(__uuidof(ICurrentState)),
            L"{BE5D3E90-A66C-4A0A-9B7B-1A6B9BF3971E}");
  EXPECT_EQ(StringFromGuid(__uuidof(IGoogleUpdate3Web)),
            L"{027234BD-61BB-4F5C-9386-7FE804171C8C}");
  EXPECT_EQ(StringFromGuid(__uuidof(IAppBundleWeb)),
            L"{D734C877-21F4-496E-B857-3E5B2E72E4CC}");
  EXPECT_EQ(StringFromGuid(__uuidof(IAppWeb)),
            L"{2C6218B9-088D-4D25-A4F8-570558124142}");
  EXPECT_EQ(StringFromGuid(__uuidof(IAppCommandWeb)),
            L"{87DBF75E-F590-4802-93FD-F8D07800E2E9}");
  EXPECT_EQ(StringFromGuid(__uuidof(IPolicyStatus)),
            L"{7D908375-C9D0-44C5-BB98-206F3C24A74C}");
  EXPECT_EQ(StringFromGuid(__uuidof(IPolicyStatus2)),
            L"{9D31EA63-2E06-4D41-98C7-CB1F307DB597}");
  EXPECT_EQ(StringFromGuid(__uuidof(IPolicyStatus3)),
            L"{5C674FC1-80E3-48D2-987B-79D9D286065B}");
  EXPECT_EQ(StringFromGuid(__uuidof(IPolicyStatus4)),
            L"{4F08E832-C4AF-4D77-840F-8884083E8324}");
  EXPECT_EQ(StringFromGuid(__uuidof(IPolicyStatusValue)),
            L"{47C8886A-A4B5-4F6C-865A-41A207074DFA}");
  EXPECT_EQ(StringFromGuid(__uuidof(IProcessLauncher)),
            L"{EED70106-3604-4385-866E-6D540E99CA1A}");
  EXPECT_EQ(StringFromGuid(__uuidof(IProcessLauncher2)),
            L"{BAEE6326-C925-4FA4-AFE9-5FA69902B021}");
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace updater::test
