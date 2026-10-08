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
  ObserverCompletionInfo info;
  info.completion_code = CompletionCodes::COMPLETION_CODE_ERROR;
  info.completion_text = text;
  return info;
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
  const UpdateService::UpdateState::State state;
  const UpdateService::ErrorCategory error_category;
  const int error_code;
  const std::wstring lang;
  const CompletionCodes expected_completion_code;
  const std::u16string expected_completion_text;
  const std::u16string expected_completion_message;
};

class AppInstallWinHandleInstallResultTest
    : public ::testing::TestWithParam<
          AppInstallWinHandleInstallResultTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    AppInstallWinHandleInstallResultTestCases,
    AppInstallWinHandleInstallResultTest,
    ::testing::ValuesIn(std::vector<AppInstallWinHandleInstallResultTestCase>{
        {UpdateService::UpdateState::State::kUpdated,
         UpdateService::ErrorCategory::kNone,
         0,
         {},
         CompletionCodes::COMPLETION_CODE_SUCCESS,
         base::WideToUTF16(
             GetLocalizedString(IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE)),
         {}},
        {UpdateService::UpdateState::State::kNoUpdate,
         UpdateService::ErrorCategory::kNone,
         0,
         {},
         CompletionCodes::COMPLETION_CODE_ERROR,
         base::WideToUTF16(GetLocalizedString(IDS_NO_UPDATE_RESPONSE_BASE)),
         {}},
        {UpdateService::UpdateState::State::kUpdateError,
         UpdateService::ErrorCategory::kNone,
         0,
         {},
         CompletionCodes::COMPLETION_CODE_ERROR,
         base::WideToUTF16(GetLocalizedString(IDS_INSTALL_UPDATER_FAILED_BASE)),
         {}},
        {UpdateService::UpdateState::State::kNotStarted,
         UpdateService::ErrorCategory::kNone,
         kErrorWrongUser,
         L"de",
         CompletionCodes::COMPLETION_CODE_ERROR,
         base::WideToUTF16(GetLocalizedString(
             ::IsUserAnAdmin() ? IDS_WRONG_USER_DEELEVATION_REQUIRED_ERROR_BASE
                               : IDS_WRONG_USER_ELEVATION_REQUIRED_ERROR_BASE,
             L"de")),
         {}},
        {UpdateService::UpdateState::State::kNotStarted,
         UpdateService::ErrorCategory::kNone,
         kErrorFailedToLockSetupMutex,
         L"hi",
         CompletionCodes::COMPLETION_CODE_ERROR,
         base::WideToUTF16(GetLocalizedString(IDS_UNABLE_TO_GET_SETUP_LOCK_BASE,
                                              L"hi")),
         {}},
        {UpdateService::UpdateState::State::kNotStarted,
         UpdateService::ErrorCategory::kNone,
         kErrorFailedToLockPrefsMutex,
         L"ar",
         CompletionCodes::COMPLETION_CODE_ERROR,
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_STARTUP_ERROR_BASE,
             GetTextForSystemError(kErrorFailedToLockPrefsMutex),
             L"ar")),
         {}},
    }));

TEST_P(AppInstallWinHandleInstallResultTest, TestCases) {
  UpdateService::UpdateState update_state;
  update_state.state = GetParam().state;
  update_state.error_category = GetParam().error_category;
  update_state.error_code = GetParam().error_code;
  update_state.app_id = "test1";
  const ObserverCompletionInfo info =
      HandleInstallResult(update_state, GetParam().lang);
  ASSERT_EQ(info.apps_info.size(), 1u);
  ASSERT_EQ(info.completion_code, GetParam().expected_completion_code);
  ASSERT_EQ(info.completion_text, GetParam().expected_completion_text);
  ASSERT_EQ(info.apps_info[0].completion_message,
            GetParam().expected_completion_message);
}

}  // namespace updater
