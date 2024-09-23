// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <shlobj.h>

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/updater/app/app_install_win_internal.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

struct AppInstallWinHandleInstallResultTestCase {
  const UpdateService::UpdateState::State state;
  const UpdateService::ErrorCategory error_category;
  const int error_code;
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
         CompletionCodes::COMPLETION_CODE_SUCCESS,
         base::WideToUTF16(
             GetLocalizedString(IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE)),
         {}},
        {UpdateService::UpdateState::State::kNoUpdate,
         UpdateService::ErrorCategory::kNone,
         0,
         CompletionCodes::COMPLETION_CODE_ERROR,
         base::WideToUTF16(GetLocalizedString(IDS_NO_UPDATE_RESPONSE_BASE)),
         {}},
        {UpdateService::UpdateState::State::kUpdateError,
         UpdateService::ErrorCategory::kNone,
         0,
         CompletionCodes::COMPLETION_CODE_ERROR,
         base::WideToUTF16(GetLocalizedString(IDS_INSTALL_UPDATER_FAILED_BASE)),
         {}},
        {UpdateService::UpdateState::State::kNotStarted,
         UpdateService::ErrorCategory::kNone,
         kErrorWrongUser,
         CompletionCodes::COMPLETION_CODE_ERROR,
         base::WideToUTF16(GetLocalizedString(
             ::IsUserAnAdmin() ? IDS_WRONG_USER_DEELEVATION_REQUIRED_ERROR_BASE
                               : IDS_WRONG_USER_ELEVATION_REQUIRED_ERROR_BASE)),
         {}},
        {UpdateService::UpdateState::State::kNotStarted,
         UpdateService::ErrorCategory::kNone,
         kErrorFailedToLockSetupMutex,
         CompletionCodes::COMPLETION_CODE_ERROR,
         base::WideToUTF16(
             GetLocalizedString(IDS_UNABLE_TO_GET_SETUP_LOCK_BASE)),
         {}},
        {UpdateService::UpdateState::State::kNotStarted,
         UpdateService::ErrorCategory::kNone,
         kErrorFailedToLockPrefsMutex,
         CompletionCodes::COMPLETION_CODE_ERROR,
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_STARTUP_ERROR_BASE,
             GetTextForSystemError(kErrorFailedToLockPrefsMutex))),
         {}},
    }));

TEST_P(AppInstallWinHandleInstallResultTest, TestCases) {
  UpdateService::UpdateState update_state;
  update_state.state = GetParam().state;
  update_state.error_category = GetParam().error_category;
  update_state.error_code = GetParam().error_code;
  update_state.app_id = "test1";
  const ObserverCompletionInfo info = HandleInstallResult(update_state);
  ASSERT_EQ(info.apps_info.size(), 1u);
  ASSERT_EQ(info.completion_code, GetParam().expected_completion_code);
  ASSERT_EQ(info.completion_text, GetParam().expected_completion_text);
  ASSERT_EQ(info.apps_info[0].completion_message,
            GetParam().expected_completion_message);
}

}  // namespace updater
