// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_install_win_internal.h"

#include <string>

#include "base/test/task_environment.h"
#include "chrome/updater/update_service.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

struct AppInstallWinHandleInstallResultTestCase {
  const UpdateService::ErrorCategory error_category;
  const int error_code;
  const std::u16string expected_completion_message;
};

class AppInstallWinHandleInstallResultTest
    : public ::testing::TestWithParam<
          AppInstallWinHandleInstallResultTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    AppInstallWinHandleInstallResultTestCases,
    AppInstallWinHandleInstallResultTest,
    ::testing::ValuesIn(std::vector<AppInstallWinHandleInstallResultTestCase>{
        {UpdateService::ErrorCategory::kDownload,
         static_cast<int>(update_client::CrxDownloaderError::NO_URL),
         u"update_client::CrxDownloaderError::NO_URL"},
        {UpdateService::ErrorCategory::kDownload,
         static_cast<int>(update_client::CrxDownloaderError::NO_HASH),
         u"update_client::CrxDownloaderError::NO_HASH"},
        {UpdateService::ErrorCategory::kDownload,
         static_cast<int>(update_client::CrxDownloaderError::BAD_HASH),
         u"update_client::CrxDownloaderError::BAD_HASH"},
        {UpdateService::ErrorCategory::kDownload,
         static_cast<int>(
             update_client::CrxDownloaderError::BITS_TOO_MANY_JOBS),
         u"update_client::CrxDownloaderError::BITS_TOO_MANY_JOBS"},
        {UpdateService::ErrorCategory::kDownload,
         static_cast<int>(update_client::CrxDownloaderError::GENERIC_ERROR),
         u"update_client::CrxDownloaderError::GENERIC_ERROR"},
        {UpdateService::ErrorCategory::kDownload, 0xFFFF, u""},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kInvalidParams),
         u"update_client::UnpackerError::kInvalidParams"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kInvalidFile),
         u"update_client::UnpackerError::kInvalidFile"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kUnzipPathError),
         u"update_client::UnpackerError::kUnzipPathError"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kUnzipFailed),
         u"update_client::UnpackerError::kUnzipFailed"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kBadManifest),
         u"update_client::UnpackerError::kBadManifest"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kBadExtension),
         u"update_client::UnpackerError::kBadExtension"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kIoError),
         u"update_client::UnpackerError::kIoError"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kDeltaVerificationFailure),
         u"update_client::UnpackerError::kDeltaVerificationFailure"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kDeltaBadCommands),
         u"update_client::UnpackerError::kDeltaBadCommands"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kDeltaUnsupportedCommand),
         u"update_client::UnpackerError::kDeltaUnsupportedCommand"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kDeltaOperationFailure),
         u"update_client::UnpackerError::kDeltaOperationFailure"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kDeltaPatchProcessFailure),
         u"update_client::UnpackerError::kDeltaPatchProcessFailure"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kDeltaMissingExistingFile),
         u"update_client::UnpackerError::kDeltaMissingExistingFile"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kPuffinMissingPreviousCrx),
         u"update_client::UnpackerError::kPuffinMissingPreviousCrx"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kFailedToAddToCache),
         u"update_client::UnpackerError::kFailedToAddToCache"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kFailedToCreateCacheDir),
         u"update_client::UnpackerError::kFailedToCreateCacheDir"},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kCrxCacheNotProvided),
         u"update_client::UnpackerError::kCrxCacheNotProvided"},
        {UpdateService::ErrorCategory::kUnpack, 0xFFFF, u""},
        {UpdateService::ErrorCategory::kService,
         static_cast<int>(update_client::ServiceError::SERVICE_WAIT_FAILED),
         u"update_client::ServiceError::SERVICE_WAIT_FAILED"},
        {UpdateService::ErrorCategory::kService,
         static_cast<int>(update_client::ServiceError::UPDATE_DISABLED),
         u"update_client::ServiceError::UPDATE_DISABLED"},
        {UpdateService::ErrorCategory::kService,
         static_cast<int>(update_client::ServiceError::CANCELLED),
         u"update_client::ServiceError::CANCELLED"},
        {UpdateService::ErrorCategory::kService,
         static_cast<int>(update_client::ServiceError::CHECK_FOR_UPDATE_ONLY),
         u"update_client::ServiceError::CHECK_FOR_UPDATE_ONLY"},
        {UpdateService::ErrorCategory::kService, 0xFFFF, u""},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::RESPONSE_NOT_TRUSTED),
         u"update_client::ProtocolError::RESPONSE_NOT_TRUSTED"},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::MISSING_PUBLIC_KEY),
         u"update_client::ProtocolError::MISSING_PUBLIC_KEY"},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::MISSING_URLS),
         u"update_client::ProtocolError::MISSING_URLS"},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::PARSE_FAILED),
         u"update_client::ProtocolError::PARSE_FAILED"},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(
             update_client::ProtocolError::UPDATE_RESPONSE_NOT_FOUND),
         u"update_client::ProtocolError::UPDATE_RESPONSE_NOT_FOUND"},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::URL_FETCHER_FAILED),
         u"update_client::ProtocolError::URL_FETCHER_FAILED"},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::UNKNOWN_APPLICATION),
         u"update_client::ProtocolError::UNKNOWN_APPLICATION"},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::RESTRICTED_APPLICATION),
         u"update_client::ProtocolError::RESTRICTED_APPLICATION"},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::INVALID_APPID),
         u"update_client::ProtocolError::INVALID_APPID"},
        {UpdateService::ErrorCategory::kUpdateCheck, 0xFFFF, u""},
    }));

TEST_P(AppInstallWinHandleInstallResultTest, TestCases) {
  UpdateService::UpdateState update_state;
  update_state.error_category = GetParam().error_category;
  update_state.error_code = GetParam().error_code;
  update_state.app_id = "test1";
  update_state.state = UpdateService::UpdateState::State::kUpdateError;
  const ObserverCompletionInfo info = HandleInstallResult(update_state);
  ASSERT_EQ(info.apps_info.size(), 1u);
  ASSERT_EQ(info.apps_info[0].completion_message,
            GetParam().expected_completion_message);
}

}  // namespace updater
