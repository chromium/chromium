// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <shlobj.h>

#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
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

struct AppInstallWinGetInstallerTextTestCase {
  const UpdateService::ErrorCategory error_category;
  const int error_code;
  const std::string expected_completion_message;
  std::optional<int> extra_code;
};

class AppInstallWinGetInstallerTextTest
    : public ::testing::TestWithParam<AppInstallWinGetInstallerTextTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    AppInstallWinGetInstallerTextTestCases,
    AppInstallWinGetInstallerTextTest,
    ::testing::ValuesIn(std::vector<AppInstallWinGetInstallerTextTestCase>{
        {UpdateService::ErrorCategory::kDownload,
         static_cast<int>(update_client::CrxDownloaderError::NO_URL),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_DOWNLOAD_ERROR_BASE,
             L"update_client::CrxDownloaderError::NO_URL"))},
        {UpdateService::ErrorCategory::kDownload,
         static_cast<int>(update_client::CrxDownloaderError::NO_HASH),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_DOWNLOAD_ERROR_BASE,
             L"update_client::CrxDownloaderError::NO_HASH"))},
        {UpdateService::ErrorCategory::kDownload,
         static_cast<int>(update_client::CrxDownloaderError::BAD_HASH),
         base::WideToUTF8(GetLocalizedString(IDS_DOWNLOAD_HASH_MISMATCH_BASE))},
        {UpdateService::ErrorCategory::kDownload,
         static_cast<int>(
             update_client::CrxDownloaderError::BITS_TOO_MANY_JOBS),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_DOWNLOAD_ERROR_BASE,
             L"update_client::CrxDownloaderError::BITS_TOO_MANY_JOBS"))},
        {UpdateService::ErrorCategory::kDownload,
         static_cast<int>(update_client::CrxDownloaderError::GENERIC_ERROR),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_DOWNLOAD_ERROR_BASE,
             L"update_client::CrxDownloaderError::GENERIC_ERROR"))},
        {UpdateService::ErrorCategory::kDownload, 0xFFFF,
         base::WideToUTF8(GetLocalizedStringF(IDS_GENERIC_DOWNLOAD_ERROR_BASE,
                                              L"0xffff"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kInvalidParams),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kInvalidParams"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kInvalidFile),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kInvalidFile"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kUnzipPathError),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kUnzipPathError"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kUnzipFailed),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kUnzipFailed"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kBadManifest),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kBadManifest"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kBadExtension),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kBadExtension"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kIoError),
         base::WideToUTF8(
             GetLocalizedStringF(IDS_GENERIC_UNPACK_ERROR_BASE,
                                 L"update_client::UnpackerError::kIoError"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kDeltaVerificationFailure),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaVerificationFailure"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kDeltaBadCommands),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaBadCommands"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kDeltaUnsupportedCommand),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaUnsupportedCommand"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kDeltaOperationFailure),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaOperationFailure"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kDeltaPatchProcessFailure),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaPatchProcessFailure"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kDeltaMissingExistingFile),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaMissingExistingFile"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kPuffinMissingPreviousCrx),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kPuffinMissingPreviousCrx"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kFailedToAddToCache),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_UNPACK_CACHING_ERROR_BASE,
             L"update_client::UnpackerError::kFailedToAddToCache"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(
             update_client::UnpackerError::kFailedToCreateCacheDir),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_UNPACK_CACHING_ERROR_BASE,
             L"update_client::UnpackerError::kFailedToCreateCacheDir"))},
        {UpdateService::ErrorCategory::kUnpack,
         static_cast<int>(update_client::UnpackerError::kCrxCacheNotProvided),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kCrxCacheNotProvided"))},
        {UpdateService::ErrorCategory::kUnpack, 0xFFFF,
         base::WideToUTF8(GetLocalizedStringF(IDS_GENERIC_UNPACK_ERROR_BASE,
                                              L"0xffff"))},
        {UpdateService::ErrorCategory::kService,
         static_cast<int>(update_client::ServiceError::SERVICE_WAIT_FAILED),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_SERVICE_ERROR_BASE,
             L"update_client::ServiceError::SERVICE_WAIT_FAILED"))},
        {UpdateService::ErrorCategory::kService,
         static_cast<int>(update_client::ServiceError::UPDATE_DISABLED),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_SERVICE_ERROR_BASE,
             L"update_client::ServiceError::UPDATE_DISABLED"))},
        {UpdateService::ErrorCategory::kService,
         static_cast<int>(update_client::ServiceError::CANCELLED),
         base::WideToUTF8(
             GetLocalizedString(IDS_SERVICE_ERROR_CANCELLED_BASE))},
        {UpdateService::ErrorCategory::kService,
         static_cast<int>(update_client::ServiceError::CHECK_FOR_UPDATE_ONLY),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_SERVICE_ERROR_BASE,
             L"update_client::ServiceError::CHECK_FOR_UPDATE_ONLY"))},
        {UpdateService::ErrorCategory::kService, 0xFFFF,
         base::WideToUTF8(GetLocalizedStringF(IDS_GENERIC_SERVICE_ERROR_BASE,
                                              L"0xffff"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::RESPONSE_NOT_TRUSTED),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::RESPONSE_NOT_TRUSTED"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::MISSING_PUBLIC_KEY),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::MISSING_PUBLIC_KEY"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::MISSING_URLS),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::MISSING_URLS"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::PARSE_FAILED),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::PARSE_FAILED"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(
             update_client::ProtocolError::UPDATE_RESPONSE_NOT_FOUND),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::UPDATE_RESPONSE_NOT_FOUND"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::URL_FETCHER_FAILED),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::URL_FETCHER_FAILED"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::UNKNOWN_APPLICATION),
         base::WideToUTF8(GetLocalizedString(IDS_UNKNOWN_APPLICATION_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::RESTRICTED_APPLICATION),
         base::WideToUTF8(
             GetLocalizedString(IDS_RESTRICTED_RESPONSE_FROM_SERVER_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         static_cast<int>(update_client::ProtocolError::INVALID_APPID),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::INVALID_APPID"))},
        {UpdateService::ErrorCategory::kUpdateCheck, 401,
         base::WideToUTF8(
             GetLocalizedString(IDS_ERROR_HTTPSTATUS_UNAUTHORIZED_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck, 403,
         base::WideToUTF8(
             GetLocalizedString(IDS_ERROR_HTTPSTATUS_FORBIDDEN_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck, 407,
         base::WideToUTF8(GetLocalizedString(
             IDS_ERROR_HTTPSTATUS_PROXY_AUTH_REQUIRED_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck, 404,
         base::WideToUTF8(
             GetLocalizedStringF(IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
                                 L"HTTP 404"))},
        {UpdateService::ErrorCategory::kUpdateCheck, 0xFFFF,
         base::WideToUTF8(
             GetLocalizedStringF(IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
                                 L"0xffff"))},
        {UpdateService::ErrorCategory::kInstall,
         static_cast<int>(
             update_client::InstallError::FINGERPRINT_WRITE_FAILED),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::FINGERPRINT_WRITE_FAILED"))},
        {UpdateService::ErrorCategory::kInstall,
         static_cast<int>(update_client::InstallError::BAD_MANIFEST),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::BAD_MANIFEST"))},
        {UpdateService::ErrorCategory::kInstall,
         static_cast<int>(update_client::InstallError::GENERIC_ERROR),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::GENERIC_ERROR"))},
        {UpdateService::ErrorCategory::kInstall,
         static_cast<int>(update_client::InstallError::MOVE_FILES_ERROR),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::MOVE_FILES_ERROR"))},
        {UpdateService::ErrorCategory::kInstall,
         static_cast<int>(update_client::InstallError::SET_PERMISSIONS_FAILED),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::SET_PERMISSIONS_FAILED"))},
        {UpdateService::ErrorCategory::kInstall,
         static_cast<int>(update_client::InstallError::INVALID_VERSION),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::INVALID_VERSION"))},
        {UpdateService::ErrorCategory::kInstall,
         static_cast<int>(update_client::InstallError::VERSION_NOT_UPGRADED),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::VERSION_NOT_UPGRADED"))},
        {UpdateService::ErrorCategory::kInstall,
         static_cast<int>(update_client::InstallError::NO_DIR_COMPONENT_USER),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::NO_DIR_COMPONENT_USER"))},
        {UpdateService::ErrorCategory::kInstall,
         static_cast<int>(
             update_client::InstallError::CLEAN_INSTALL_DIR_FAILED),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::CLEAN_INSTALL_DIR_FAILED"))},
        {UpdateService::ErrorCategory::kInstall,
         static_cast<int>(
             update_client::InstallError::INSTALL_VERIFICATION_FAILED),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::INSTALL_VERIFICATION_FAILED"))},
        {UpdateService::ErrorCategory::kInstall,
         static_cast<int>(update_client::InstallError::MISSING_INSTALL_PARAMS),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::MISSING_INSTALL_PARAMS"))},
        {UpdateService::ErrorCategory::kInstall,
         static_cast<int>(update_client::InstallError::LAUNCH_PROCESS_FAILED),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::LAUNCH_PROCESS_FAILED"))},
        {UpdateService::ErrorCategory::kInstall,
         static_cast<int>(update_client::InstallError::CUSTOM_ERROR_BASE),
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::CUSTOM_ERROR_BASE"))},

        // `2` is also the value for
        // `update_client::InstallError::FINGERPRINT_WRITE_FAILED`, but since
        // this is coded as an "installer_error", the error will be interpreted
        // as the Windows error code for `ERROR_FILE_NOT_FOUND` instead.
        {UpdateService::ErrorCategory::kInstaller,
         2,
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALLER_ERROR_BASE,
             L"The system cannot find the file specified. ")),
         {}},
        {UpdateService::ErrorCategory::kInstaller,
         GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
         base::WideToUTF8(
             GetLocalizedStringF(IDS_APP_INSTALL_DISABLED_BY_GROUP_POLICY_BASE,
                                 L"GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY")),
         {}},
        {UpdateService::ErrorCategory::kInstaller,
         GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY,
         base::WideToUTF8(
             GetLocalizedStringF(IDS_APP_INSTALL_DISABLED_BY_GROUP_POLICY_BASE,
                                 L"GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY")),
         {}},
        {UpdateService::ErrorCategory::kInstaller,
         GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL,
         base::WideToUTF8(GetLocalizedStringF(
             IDS_APP_INSTALL_DISABLED_BY_GROUP_POLICY_BASE,
             L"GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL")),
         {}},
        {UpdateService::ErrorCategory::kInstaller,
         GOOPDATEINSTALL_E_FILENAME_INVALID,
         base::WideToUTF8(base::StrCat(
             {GetLocalizedString(IDS_INVALID_INSTALLER_FILENAME_BASE), L"\n",
              GetLocalizedStringF(IDS_EXTRA_CODE_BASE,
                                  base::UTF8ToWide(base::StringPrintf(
                                      "%#x",
                                      kErrorMissingInstallParams)))})),
         kErrorMissingInstallParams},
        {UpdateService::ErrorCategory::kInstaller,
         GOOPDATEINSTALL_E_INSTALLER_FAILED_START,
         base::WideToUTF8(base::StrCat(
             {GetLocalizedString(IDS_INSTALLER_FAILED_TO_START_BASE), L"\n",
              GetLocalizedStringF(IDS_EXTRA_CODE_BASE, L"0x2")})),
         ERROR_FILE_NOT_FOUND},
        {UpdateService::ErrorCategory::kInstaller,
         GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT,
         base::WideToUTF8(GetLocalizedString(IDS_INSTALLER_TIMED_OUT_BASE)),
         {}},

        {UpdateService::ErrorCategory::kInstaller,
         ERROR_SUCCESS_REBOOT_INITIATED,
         base::WideToUTF8(GetLocalizedStringF(
             IDS_INSTALL_REBOOT_BASE,
             GetTextForSystemError(ERROR_SUCCESS_REBOOT_INITIATED))),
         {}},
        {UpdateService::ErrorCategory::kInstaller,
         ERROR_SUCCESS_REBOOT_REQUIRED,
         base::WideToUTF8(GetLocalizedStringF(
             IDS_INSTALL_REBOOT_BASE,
             GetTextForSystemError(ERROR_SUCCESS_REBOOT_REQUIRED))),
         {}},
        {UpdateService::ErrorCategory::kInstaller,
         ERROR_SUCCESS_RESTART_REQUIRED,
         base::WideToUTF8(GetLocalizedStringF(
             IDS_INSTALL_REBOOT_BASE,
             GetTextForSystemError(ERROR_SUCCESS_RESTART_REQUIRED))),
         {}},
    }));

TEST_P(AppInstallWinGetInstallerTextTest, TestCases) {
  ASSERT_EQ(GetInstallerText(GetParam().error_category, GetParam().error_code,
                             GetParam().extra_code.value_or(0)),
            GetParam().expected_completion_message);
}

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
