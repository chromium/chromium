// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <shlobj.h>

#include <optional>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/app/app_install_progress.h"
#include "chrome/updater/app/app_install_win_internal.h"
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace {

using ::testing::_;
using ::testing::Field;
using ::testing::StrictMock;

class MockAppInstallProgress : public AppInstallProgress {
 public:
  MOCK_METHOD(void, OnCheckingForUpdate, (), (override));
  MOCK_METHOD(void,
              OnUpdateAvailable,
              (const std::string& app_id,
               const std::u16string& app_name,
               const base::Version& version),
              (override));
  MOCK_METHOD(void,
              OnWaitingToDownload,
              (const std::string& app_id, const std::u16string& app_name),
              (override));
  MOCK_METHOD(void,
              OnDownloading,
              (const std::string& app_id,
               const std::u16string& app_name,
               std::optional<base::TimeDelta> time_remaining,
               int pos),
              (override));
  MOCK_METHOD(void,
              OnWaitingRetryDownload,
              (const std::string& app_id,
               const std::u16string& app_name,
               base::Time next_retry_time),
              (override));
  MOCK_METHOD(void,
              OnWaitingToInstall,
              (const std::string& app_id, const std::u16string& app_name),
              (override));
  MOCK_METHOD(void,
              OnInstalling,
              (const std::string& app_id,
               const std::u16string& app_name,
               std::optional<base::TimeDelta> time_remaining,
               int pos),
              (override));
  MOCK_METHOD(void, OnPause, (), (override));
  MOCK_METHOD(void,
              OnComplete,
              (const ObserverCompletionInfo& observer_info),
              (override));
};

ObserverCompletionInfo MakeCompletionInfo(const std::u16string& text) {
  return {.completion_code = CompletionCodes::COMPLETION_CODE_ERROR,
          .completion_text = text};
}

}  // namespace

class AppInstallProgressForwarderTest : public ::testing::Test {
 protected:
  // Matching a `const std::u16string&` argument against a `u""` literal needs
  // an explicit `std::u16string`: gmock only has a literal shortcut for
  // `std::string`.
  const std::string app_id_ = "app";
  const std::u16string app_name_ = u"App";
};

TEST_F(AppInstallProgressForwarderTest, ForwardsToTarget) {
  StrictMock<MockAppInstallProgress> target;
  AppInstallProgressForwarder forwarder;
  forwarder.SetTarget(&target);

  EXPECT_CALL(target, OnCheckingForUpdate());
  forwarder.OnCheckingForUpdate();

  EXPECT_CALL(target, OnDownloading(app_id_, app_name_, _, 42));
  forwarder.OnDownloading(app_id_, app_name_, std::nullopt, 42);
}

TEST_F(AppInstallProgressForwarderTest, DropsCallsWithoutTargetUntilSet) {
  StrictMock<MockAppInstallProgress> target;
  AppInstallProgressForwarder forwarder;

  // Nothing to forward to yet: the call is remembered, not dropped for good.
  forwarder.OnComplete(MakeCompletionInfo(u"done"));

  EXPECT_CALL(target, OnComplete(Field(&ObserverCompletionInfo::completion_text,
                                       u"done")));
  forwarder.SetTarget(&target);
}

// A UI which takes over after the install completed must show the completion
// state, since no further notifications arrive.
TEST_F(AppInstallProgressForwarderTest, ReplaysCompletionToNewTarget) {
  StrictMock<MockAppInstallProgress> first;
  StrictMock<MockAppInstallProgress> second;
  AppInstallProgressForwarder forwarder;
  forwarder.SetTarget(&first);

  EXPECT_CALL(first, OnInstalling(app_id_, app_name_, _, 70));
  EXPECT_CALL(first, OnComplete(Field(&ObserverCompletionInfo::completion_text,
                                      u"done")));
  forwarder.OnInstalling(app_id_, app_name_, std::nullopt, 70);
  forwarder.OnComplete(MakeCompletionInfo(u"done"));

  // Only the most recent call is replayed.
  EXPECT_CALL(second, OnComplete(Field(&ObserverCompletionInfo::completion_text,
                                       u"done")));
  forwarder.SetTarget(&second);

  // Later calls go to the new target only.
  EXPECT_CALL(second, OnPause());
  forwarder.OnPause();
}

// A UI which takes over mid-install shows the current progress instead of its
// initial state.
TEST_F(AppInstallProgressForwarderTest, ReplaysProgressToNewTarget) {
  StrictMock<MockAppInstallProgress> first;
  StrictMock<MockAppInstallProgress> second;
  AppInstallProgressForwarder forwarder;
  forwarder.SetTarget(&first);

  EXPECT_CALL(first, OnCheckingForUpdate());
  EXPECT_CALL(first, OnDownloading(app_id_, app_name_, _, 25));
  forwarder.OnCheckingForUpdate();
  forwarder.OnDownloading(app_id_, app_name_, base::Seconds(10), 25);

  EXPECT_CALL(
      second,
      OnDownloading(app_id_, app_name_,
                    std::optional<base::TimeDelta>(base::Seconds(10)), 25));
  forwarder.SetTarget(&second);
}

TEST_F(AppInstallProgressForwarderTest, NullTargetDropsCalls) {
  StrictMock<MockAppInstallProgress> target;
  AppInstallProgressForwarder forwarder;
  forwarder.SetTarget(&target);

  EXPECT_CALL(target, OnCheckingForUpdate());
  forwarder.OnCheckingForUpdate();

  forwarder.SetTarget(nullptr);
  forwarder.OnPause();
}

struct AppInstallWinHandleInstallResultTestCase {
  const std::string name;
  const UpdateService::UpdateState::State state;
  const UpdateService::ErrorCategory error_category =
      UpdateService::ErrorCategory::kNone;
  const int error_code = 0;
  const int extra_code1 = 0;
  const std::string installer_text;
  const std::string installer_cmd_line;
  const std::wstring lang;
  const CompletionCodes expected_completion_code;
  const std::u16string expected_completion_text;
  const AppCompletionInfo expected_app_info;
};

class AppInstallWinHandleInstallResultTest
    : public ::testing::TestWithParam<
          AppInstallWinHandleInstallResultTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    AppInstallWinHandleInstallResultTestCases,
    AppInstallWinHandleInstallResultTest,
    ::testing::ValuesIn(std::vector<AppInstallWinHandleInstallResultTestCase>{
        {.name = "Updated",
         .state = UpdateService::UpdateState::State::kUpdated,
         .expected_completion_code = CompletionCodes::COMPLETION_CODE_SUCCESS,
         .expected_completion_text = base::WideToUTF16(
             GetLocalizedString(IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE)),
         .expected_app_info = {.app_id = "test1"}},
        {.name = "UpdatedWithLaunchCommand",
         .state = UpdateService::UpdateState::State::kUpdated,
         .installer_cmd_line = "launch.exe",
         .expected_completion_code = CompletionCodes::COMPLETION_CODE_SUCCESS,
         .expected_completion_text = base::WideToUTF16(
             GetLocalizedString(IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE)),
         .expected_app_info =
             {.app_id = "test1",
              .completion_code = CompletionCodes::
                  COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND,
              .post_install_launch_command_line = "launch.exe"}},
        {.name = "NoUpdate",
         .state = UpdateService::UpdateState::State::kNoUpdate,
         .expected_completion_code = CompletionCodes::COMPLETION_CODE_ERROR,
         .expected_completion_text =
             base::WideToUTF16(GetLocalizedString(IDS_NO_UPDATE_RESPONSE_BASE)),
         .expected_app_info = {}},
        {.name = "UpdateError",
         .state = UpdateService::UpdateState::State::kUpdateError,
         .expected_completion_code = CompletionCodes::COMPLETION_CODE_ERROR,
         .expected_completion_text = base::WideToUTF16(
             GetLocalizedString(IDS_INSTALL_UPDATER_FAILED_BASE)),
         .expected_app_info = {.app_id = "test1"}},
        {.name = "InstallerError",
         .state = UpdateService::UpdateState::State::kUpdateError,
         .error_category = UpdateService::ErrorCategory::kInstaller,
         .error_code = 1603,
         .extra_code1 = 5,
         .installer_text = "Installer error",
         .installer_cmd_line = "launch.exe",
         .expected_completion_code = CompletionCodes::COMPLETION_CODE_ERROR,
         .expected_completion_text = base::WideToUTF16(
             GetLocalizedString(IDS_INSTALL_UPDATER_FAILED_BASE)),
         .expected_app_info =
             {.app_id = "test1",
              .completion_message = u"Installer error",
              .completion_code = CompletionCodes::COMPLETION_CODE_ERROR,
              .error_code = 1603,
              .extra_code1 = 5,
              .post_install_launch_command_line = "launch.exe"}},
        // An install that needs a reboot succeeds and reports the reboot code.
        {.name = "RebootInitiated",
         .state = UpdateService::UpdateState::State::kUpdated,
         .error_code = ERROR_SUCCESS_REBOOT_INITIATED,
         .expected_completion_code = CompletionCodes::COMPLETION_CODE_SUCCESS,
         .expected_completion_text = base::WideToUTF16(
             GetLocalizedString(IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE)),
         .expected_app_info =
             {.app_id = "test1",
              .completion_code =
                  CompletionCodes::COMPLETION_CODE_REBOOT_NOTICE_ONLY,
              .error_code = ERROR_SUCCESS_REBOOT_INITIATED}},
        {.name = "RebootRequired",
         .state = UpdateService::UpdateState::State::kUpdated,
         .error_code = ERROR_SUCCESS_REBOOT_REQUIRED,
         .expected_completion_code = CompletionCodes::COMPLETION_CODE_SUCCESS,
         .expected_completion_text = base::WideToUTF16(
             GetLocalizedString(IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE)),
         .expected_app_info =
             {.app_id = "test1",
              .completion_code =
                  CompletionCodes::COMPLETION_CODE_REBOOT_NOTICE_ONLY,
              .error_code = ERROR_SUCCESS_REBOOT_REQUIRED}},
        {.name = "RestartRequired",
         .state = UpdateService::UpdateState::State::kUpdated,
         .error_code = ERROR_SUCCESS_RESTART_REQUIRED,
         .expected_completion_code = CompletionCodes::COMPLETION_CODE_SUCCESS,
         .expected_completion_text = base::WideToUTF16(
             GetLocalizedString(IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE)),
         .expected_app_info =
             {.app_id = "test1",
              .completion_code =
                  CompletionCodes::COMPLETION_CODE_REBOOT_NOTICE_ONLY,
              .error_code = ERROR_SUCCESS_RESTART_REQUIRED}},
        {.name = "WrongUser",
         .state = UpdateService::UpdateState::State::kNotStarted,
         .error_code = kErrorWrongUser,
         .lang = L"de",
         .expected_completion_code = CompletionCodes::COMPLETION_CODE_ERROR,
         .expected_completion_text = base::WideToUTF16(GetLocalizedString(
             ::IsUserAnAdmin() ? IDS_WRONG_USER_DEELEVATION_REQUIRED_ERROR_BASE
                               : IDS_WRONG_USER_ELEVATION_REQUIRED_ERROR_BASE,
             L"de")),
         .expected_app_info = {.app_id = kUpdaterAppId}},
        {.name = "SetupLockFailed",
         .state = UpdateService::UpdateState::State::kNotStarted,
         .error_code = kErrorFailedToLockSetupMutex,
         .lang = L"hi",
         .expected_completion_code = CompletionCodes::COMPLETION_CODE_ERROR,
         .expected_completion_text = base::WideToUTF16(
             GetLocalizedString(IDS_UNABLE_TO_GET_SETUP_LOCK_BASE, L"hi")),
         .expected_app_info = {.app_id = kUpdaterAppId}},
        {.name = "StartupError",
         .state = UpdateService::UpdateState::State::kNotStarted,
         .error_code = kErrorFailedToLockPrefsMutex,
         .lang = L"ar",
         .expected_completion_code = CompletionCodes::COMPLETION_CODE_ERROR,
         .expected_completion_text = base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_STARTUP_ERROR_BASE,
             GetTextForSystemError(kErrorFailedToLockPrefsMutex),
             L"ar")),
         .expected_app_info = {.app_id = kUpdaterAppId}},
    }),
    [](const ::testing::TestParamInfo<AppInstallWinHandleInstallResultTestCase>&
           info) { return info.param.name; });

TEST_P(AppInstallWinHandleInstallResultTest, TestCases) {
  UpdateService::UpdateState update_state;
  update_state.app_id = "test1";
  update_state.state = GetParam().state;
  update_state.error_category = GetParam().error_category;
  update_state.error_code = GetParam().error_code;
  update_state.extra_code1 = GetParam().extra_code1;
  update_state.installer_text = GetParam().installer_text;
  update_state.installer_cmd_line = GetParam().installer_cmd_line;
  const ObserverCompletionInfo info =
      HandleInstallResult(update_state, GetParam().lang);
  EXPECT_EQ(info.completion_code, GetParam().expected_completion_code);
  EXPECT_EQ(info.completion_text, GetParam().expected_completion_text);
  ASSERT_EQ(info.apps_info.size(), 1u);
  const AppCompletionInfo& app_info = info.apps_info[0];
  const AppCompletionInfo& expected_app_info = GetParam().expected_app_info;
  EXPECT_EQ(app_info.app_id, expected_app_info.app_id);
  EXPECT_EQ(app_info.completion_message, expected_app_info.completion_message);
  EXPECT_EQ(app_info.completion_code, expected_app_info.completion_code);
  EXPECT_EQ(app_info.error_code, expected_app_info.error_code);
  EXPECT_EQ(app_info.extra_code1, expected_app_info.extra_code1);
  EXPECT_EQ(app_info.post_install_launch_command_line,
            expected_app_info.post_install_launch_command_line);
}

}  // namespace updater
