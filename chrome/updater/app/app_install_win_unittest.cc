// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_install_win_internal.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/updater/update_service.h"
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
  const std::wstring expected_completion_text;
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
         GetLocalizedString(IDS_BUNDLE_INSTALLED_SUCCESSFULLY_BASE),
         {}},
        {UpdateService::UpdateState::State::kNoUpdate,
         UpdateService::ErrorCategory::kNone,
         0,
         CompletionCodes::COMPLETION_CODE_ERROR,
         GetLocalizedString(IDS_NO_UPDATE_RESPONSE_BASE),
         {}},
        {UpdateService::UpdateState::State::kUpdateError,
         UpdateService::ErrorCategory::kNone,
         0,
         CompletionCodes::COMPLETION_CODE_ERROR,
         GetLocalizedString(IDS_INSTALL_UPDATER_FAILED_BASE),
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
