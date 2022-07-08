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
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_variant.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/unittest_util_win.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/test/test_strings.h"
#include "chrome/updater/win/win_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

constexpr wchar_t kAppId1[] = L"{3B1A3CCA-0525-4418-93E6-A0DB3398EC9B}";

constexpr wchar_t kBadCmdLine[] = L"\"c:\\Program Files\\cmd.exe\"";
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
    SetupCmdExe(GetTestScope(), cmd_exe_command_line_, temp_programfiles_dir_);
  }

  void TearDown() override { DeleteAppClientKey(GetTestScope(), kAppId1); }

  HRESULT CreateAppCommandWeb(
      const std::wstring& app_id,
      const std::wstring& command_id,
      const std::wstring& command_line_format,
      Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl>& app_command_web) {
    CreateAppCommandRegistry(GetTestScope(), app_id, command_id,
                             command_line_format);

    return Microsoft::WRL::MakeAndInitialize<LegacyAppCommandWebImpl>(
        &app_command_web, GetTestScope(), app_id, command_id);
  }

  void WaitForUpdateCompletion(
      Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl>& app_command_web) {
    EXPECT_TRUE(test::WaitFor(base::BindLambdaForTesting([&]() {
      UINT status = 0;
      EXPECT_HRESULT_SUCCEEDED(app_command_web->get_status(&status));
      return status == COMMAND_STATUS_COMPLETE;
    })));
  }

  base::CommandLine cmd_exe_command_line_;
  base::ScopedTempDir temp_programfiles_dir_;
};

TEST_F(LegacyAppCommandWebImplTest, NoApp) {
  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  EXPECT_HRESULT_FAILED(
      Microsoft::WRL::MakeAndInitialize<LegacyAppCommandWebImpl>(
          &app_command_web, GetTestScope(), kAppId1, kCmdId1));
}

TEST_F(LegacyAppCommandWebImplTest, NoCmd) {
  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  CreateAppCommandRegistry(GetTestScope(), kAppId1, kCmdId1, kCmdLineValid);

  EXPECT_HRESULT_FAILED(
      Microsoft::WRL::MakeAndInitialize<LegacyAppCommandWebImpl>(
          &app_command_web, GetTestScope(), kAppId1, kCmdId2));
}

TEST_F(LegacyAppCommandWebImplTest, Execute) {
  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  ASSERT_HRESULT_SUCCEEDED(CreateAppCommandWeb(
      kAppId1, kCmdId1,
      base::StrCat(
          {cmd_exe_command_line_.GetCommandLineString(), L" /c \"exit 7\""}),
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

  WaitForUpdateCompletion(app_command_web);

  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_status(&status));
  EXPECT_EQ(status, COMMAND_STATUS_COMPLETE);
  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_exitCode(&exit_code));
  EXPECT_EQ(exit_code, 7U);
}

TEST_F(LegacyAppCommandWebImplTest, ExecuteParameterizedCommand) {
  Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl> app_command_web;
  ASSERT_HRESULT_SUCCEEDED(CreateAppCommandWeb(
      kAppId1, kCmdId1,
      base::StrCat(
          {cmd_exe_command_line_.GetCommandLineString(), L" /c \"exit %1\""}),
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

  WaitForUpdateCompletion(app_command_web);

  DWORD exit_code = 0;
  EXPECT_HRESULT_SUCCEEDED(app_command_web->get_exitCode(&exit_code));
  EXPECT_EQ(exit_code, 999U);
}

}  // namespace updater
