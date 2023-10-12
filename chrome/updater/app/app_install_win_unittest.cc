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
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_DOWNLOAD_ERROR_BASE,
             L"update_client::CrxDownloaderError::NO_URL"))},
        {UpdateService::ErrorCategory::kDownload,
         static_cast<int>(update_client::CrxDownloaderError::NO_HASH),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_DOWNLOAD_ERROR_BASE,
             L"update_client::CrxDownloaderError::NO_HASH"))},
        {UpdateService::ErrorCategory::kDownload,
         static_cast<int>(update_client::CrxDownloaderError::BAD_HASH),
         base::WideToUTF16(
             GetLocalizedString(IDS_DOWNLOAD_HASH_MISMATCH_BASE))},
        {UpdateService::ErrorCategory::kDownload,
         static_cast<int>(
             update_client::CrxDownloaderError::BITS_TOO_MANY_JOBS),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_DOWNLOAD_ERROR_BASE,
             L"update_client::CrxDownloaderError::BITS_TOO_MANY_JOBS"))},
        {UpdateService::ErrorCategory::kDownload,
         static_cast<int>(update_client::CrxDownloaderError::GENERIC_ERROR),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_DOWNLOAD_ERROR_BASE,
             L"update_client::CrxDownloaderError::GENERIC_ERROR"))},
        {UpdateService::ErrorCategory::kDownload, 0xFFFF,
         base::WideToUTF16(GetLocalizedStringF(IDS_GENERIC_DOWNLOAD_ERROR_BASE,
                                               L"0xffff"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kInvalidParams),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kInvalidParams"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kInvalidFile),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kInvalidFile"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kUnzipPathError),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kUnzipPathError"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kUnzipFailed),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kUnzipFailed"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kBadManifest),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kBadManifest"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kBadExtension),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kBadExtension"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kIoError),
         base::WideToUTF16(
             GetLocalizedStringF(IDS_GENERIC_UNPACK_ERROR_BASE,
                                 L"update_client::UnpackerError::kIoError"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kDeltaVerificationFailure),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaVerificationFailure"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kDeltaBadCommands),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaBadCommands"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kDeltaUnsupportedCommand),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaUnsupportedCommand"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kDeltaOperationFailure),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaOperationFailure"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kDeltaPatchProcessFailure),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaPatchProcessFailure"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kDeltaMissingExistingFile),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaMissingExistingFile"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kPuffinMissingPreviousCrx),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kPuffinMissingPreviousCrx"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kFailedToAddToCache),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_UNPACK_CACHING_ERROR_BASE,
             L"update_client::UnpackerError::kFailedToAddToCache"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kFailedToCreateCacheDir),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_UNPACK_CACHING_ERROR_BASE,
             L"update_client::UnpackerError::kFailedToCreateCacheDir"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kCrxCacheNotProvided),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kCrxCacheNotProvided"))},
        {UpdateService::ErrorCategory::kUnpack, 0xFFFF,
         base::WideToUTF16(GetLocalizedStringF(IDS_GENERIC_UNPACK_ERROR_BASE,
                                               L"0xffff"))},
        {UpdateService::ErrorCategory::kService,
         static_cast<int>(update_client::ServiceError::SERVICE_WAIT_FAILED),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_SERVICE_ERROR_BASE,
             L"update_client::ServiceError::SERVICE_WAIT_FAILED"))},
        {UpdateService::ErrorCategory::kService,
         static_cast<int>(update_client::ServiceError::UPDATE_DISABLED),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_SERVICE_ERROR_BASE,
             L"update_client::ServiceError::UPDATE_DISABLED"))},
        {UpdateService::ErrorCategory::kService,
         static_cast<int>(update_client::ServiceError::CANCELLED),
         base::WideToUTF16(
             GetLocalizedStringF(IDS_GENERIC_SERVICE_ERROR_BASE,
                                 L"update_client::ServiceError::CANCELLED"))},
        {UpdateService::ErrorCategory::kService,
         static_cast<int>(update_client::ServiceError::CHECK_FOR_UPDATE_ONLY),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_SERVICE_ERROR_BASE,
             L"update_client::ServiceError::CHECK_FOR_UPDATE_ONLY"))},
        {UpdateService::ErrorCategory::kService, 0xFFFF,
         base::WideToUTF16(GetLocalizedStringF(IDS_GENERIC_SERVICE_ERROR_BASE,
                                               L"0xffff"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::RESPONSE_NOT_TRUSTED),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::RESPONSE_NOT_TRUSTED"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::MISSING_PUBLIC_KEY),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::MISSING_PUBLIC_KEY"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::MISSING_URLS),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::MISSING_URLS"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::PARSE_FAILED),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::PARSE_FAILED"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(
             update_client::ProtocolError::UPDATE_RESPONSE_NOT_FOUND),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::UPDATE_RESPONSE_NOT_FOUND"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::URL_FETCHER_FAILED),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::URL_FETCHER_FAILED"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::UNKNOWN_APPLICATION),
         base::WideToUTF16(GetLocalizedString(IDS_UNKNOWN_APPLICATION_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::RESTRICTED_APPLICATION),
         base::WideToUTF16(
             GetLocalizedString(IDS_RESTRICTED_RESPONSE_FROM_SERVER_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::INVALID_APPID),
         base::WideToUTF16(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::INVALID_APPID"))},
        {UpdateService::ErrorCategory::kUpdateCheck, 401,
         base::WideToUTF16(
             GetLocalizedString(IDS_ERROR_HTTPSTATUS_UNAUTHORIZED_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck, 403,
         base::WideToUTF16(
             GetLocalizedString(IDS_ERROR_HTTPSTATUS_FORBIDDEN_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck, 407,
         base::WideToUTF16(GetLocalizedString(
             IDS_ERROR_HTTPSTATUS_PROXY_AUTH_REQUIRED_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck, 404,
         base::WideToUTF16(
             GetLocalizedStringF(IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
                                 L"HTTP 404"))},
        {UpdateService::ErrorCategory::kUpdateCheck, 0xFFFF,
         base::WideToUTF16(
             GetLocalizedStringF(IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
                                 L"0xffff"))},
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
