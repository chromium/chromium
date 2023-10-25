// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/enrollment_screen_handler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/policy_oauth2_token_fetcher.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/login/cookie_waiter.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {
namespace {

// Enrollment step names.
const char kEnrollmentStepSignin[] = "signin";
const char kEnrollmentStepSuccess[] = "success";
const char kEnrollmentStepWorking[] = "working";
const char kEnrollmentStepTPMChecking[] = "tpm-checking";

// Enrollment mode constants used in the UI. This needs to be kept in sync with
// oobe_screen_oauth_enrollment.js.
const char kEnrollmentModeUIForced[] = "forced";
const char kEnrollmentModeUIManual[] = "manual";
const char kEnrollmentModeUIRecovery[] = "recovery";

constexpr char kOAUTHCodeCookie[] = "oauth_code";

// Converts `mode` to a mode identifier for the UI.
std::string EnrollmentModeToUIMode(policy::EnrollmentConfig::Mode mode) {
  switch (mode) {
    case policy::EnrollmentConfig::MODE_NONE:
    case policy::EnrollmentConfig::DEPRECATED_MODE_ENROLLED_ROLLBACK:
    case policy::EnrollmentConfig::DEPRECATED_MODE_OFFLINE_DEMO:
      break;
    case policy::EnrollmentConfig::MODE_MANUAL:
    case policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT:
    case policy::EnrollmentConfig::MODE_LOCAL_ADVERTISED:
    case policy::EnrollmentConfig::MODE_SERVER_ADVERTISED:
    case policy::EnrollmentConfig::MODE_ATTESTATION:
      return kEnrollmentModeUIManual;
    case policy::EnrollmentConfig::MODE_LOCAL_FORCED:
    case policy::EnrollmentConfig::MODE_SERVER_FORCED:
    case policy::EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED:
    case policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED:
    case policy::EnrollmentConfig::MODE_ATTESTATION_MANUAL_FALLBACK:
    case policy::EnrollmentConfig::MODE_INITIAL_SERVER_FORCED:
    case policy::EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED:
    case policy::EnrollmentConfig::MODE_ATTESTATION_INITIAL_MANUAL_FALLBACK:
    case policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED:
    case policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_MANUAL_FALLBACK:
      return kEnrollmentModeUIForced;
    case policy::EnrollmentConfig::MODE_RECOVERY:
      return kEnrollmentModeUIRecovery;
  }

  NOTREACHED() << "Bad enrollment mode " << mode;
  return kEnrollmentModeUIManual;
}

std::string GetFlowString(EnrollmentScreenView::FlowType type) {
  switch (type) {
    case EnrollmentScreenView::FlowType::kEnterprise:
      return "enterprise";
    case EnrollmentScreenView::FlowType::kCFM:
      return "cfm";
    case EnrollmentScreenView::FlowType::kEnterpriseLicense:
      return "enterpriseLicense";
    case EnrollmentScreenView::FlowType::kEducationLicense:
      return "educationLicense";
  }
}

std::string GetLicenseString(policy::LicenseType type) {
  switch (type) {
    case policy::LicenseType::kNone:
      return std::string();
    case policy::LicenseType::kEnterprise:
      return "enterprise";
    case policy::LicenseType::kEducation:
      return "education";
    case policy::LicenseType::kTerminal:
      return "terminal";
  }
}

// String constants should be in sync with `OobeTypes.GaiaDialogButtonsType`.
std::string GetGaiaButtonsTypeString(
    EnrollmentScreenView::GaiaButtonsType type) {
  switch (type) {
    case EnrollmentScreenView::GaiaButtonsType::kDefault:
      return "default";
    case EnrollmentScreenView::GaiaButtonsType::kEnterprisePreferred:
      return "enterprise-preferred";
    case EnrollmentScreenView::GaiaButtonsType::kKioskPreferred:
      return "kiosk-preferred";
  }
}

bool ShouldSpecifyLicenseType(const policy::EnrollmentConfig& config) {
  // Check that license_type is specified.
  if (config.license_type == policy::LicenseType::kNone) {
    return false;
  }
  // Retrieve if the device is Education for InitialEnrollment.
  if (features::IsEducationEnrollmentOobeFlowEnabled()) {
    return true;
  }

  // Retrieve the License already used for enrollment from DMServer
  // for AutoEnrollment from message DeviceStateRetrievalResponse.
  if (features::IsAutoEnrollmentKioskInOobeEnabled() &&
      config.is_mode_attestation()) {
    return true;
  }

  return false;
}

}  // namespace

// EnrollmentScreenHandler, public ------------------------------

EnrollmentScreenHandler::EnrollmentScreenHandler()
    : BaseScreenHandler(kScreenId) {}

EnrollmentScreenHandler::~EnrollmentScreenHandler() = default;

// EnrollmentScreenHandler
//      EnrollmentScreenActor implementation -----------------------------------

void EnrollmentScreenHandler::SetEnrollmentConfig(
    const policy::EnrollmentConfig& config) {
  CHECK(config.should_enroll());
  config_ = config;
}

void EnrollmentScreenHandler::SetEnrollmentController(Controller* controller) {
  controller_ = controller;
}

void EnrollmentScreenHandler::Show() {
  if (!IsJavascriptAllowed())
    show_on_init_ = true;
  else
    DoShow();
}

void EnrollmentScreenHandler::Hide() {
  show_on_init_ = false;
}

void EnrollmentScreenHandler::ShowSigninScreen() {
  ShowStep(kEnrollmentStepSignin);
}

void EnrollmentScreenHandler::ReloadSigninScreen() {
  CallExternalAPI("doReload");
}

void EnrollmentScreenHandler::ResetEnrollmentScreen() {
  // The empty string will be replaced by the correct initial step in the screen
  // initialization code.
  ShowStep(std::string());
}

void EnrollmentScreenHandler::ShowUserError(const std::string& email) {
  // Reset the state of the GAIA so after error user would retry enrollment and
  // start from enter your account view.
  CallExternalAPI("doReload");

  if (features::IsEducationEnrollmentOobeFlowEnabled() &&
      config_.license_type == policy::LicenseType::kEducation) {
    ShowErrorMessage(
        l10n_util::GetStringFUTF8(
            IDS_ENTERPRISE_ENROLLMENT_CONSUMER_ACCOUNT_WITH_EDU_PACKAGED_LICENSE_ACCOUNT_CHECK,
            base::ASCIIToUTF16(email)),
        /*retry=*/true);
  } else {
    ShowErrorMessage(
        l10n_util::GetStringFUTF8(
            IDS_ENTERPRISE_ENROLLMENT_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE_ACCOUNT_CHECK,
            base::ASCIIToUTF16(email)),
        /*retry=*/true);
  }
}

void EnrollmentScreenHandler::ShowAttributePromptScreen(
    const std::string& asset_id,
    const std::string& location) {
  CallExternalAPI("showAttributePromptStep", asset_id, location);
}

void EnrollmentScreenHandler::ShowEnrollmentWorkingScreen() {
  ShowStep(kEnrollmentStepWorking);
}

void EnrollmentScreenHandler::ShowEnrollmentTPMCheckingScreen() {
  ShowInWebUI();
  ShowStep(kEnrollmentStepTPMChecking);
}

void EnrollmentScreenHandler::SetEnterpriseDomainInfo(
    const std::string& manager,
    const std::u16string& device_type) {
  CallExternalAPI("setEnterpriseDomainInfo", manager, device_type);
}

void EnrollmentScreenHandler::SetFlowType(FlowType flow_type) {
  flow_type_ = flow_type;
}

void EnrollmentScreenHandler::SetGaiaButtonsType(GaiaButtonsType buttons_type) {
  gaia_buttons_type_ = buttons_type;
}

void EnrollmentScreenHandler::ShowEnrollmentSuccessScreen() {
  ShowStep(kEnrollmentStepSuccess);
}

void EnrollmentScreenHandler::ShowAuthError(
    const GoogleServiceAuthError& error) {
  switch (error.state()) {
    case GoogleServiceAuthError::NONE:
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::REQUEST_CANCELED:
    case GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR:
    case GoogleServiceAuthError::CHALLENGE_RESPONSE_REQUIRED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_AUTH_FATAL_ERROR, /*retry=*/false);
      return;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_AUTH_ACCOUNT_ERROR, /*retry=*/true);
      return;
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_AUTH_NETWORK_ERROR, /*retry=*/true);
      return;
    case GoogleServiceAuthError::NUM_STATES:
      break;
  }
  NOTREACHED();
}

void EnrollmentScreenHandler::ShowOtherError(
    EnrollmentLauncher::OtherError error) {
  switch (error) {
    case EnrollmentLauncher::OTHER_ERROR_DOMAIN_MISMATCH:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_WRONG_USER,
                /*retry=*/true);
      return;
    case EnrollmentLauncher::OTHER_ERROR_FATAL:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_FATAL_ENROLLMENT_ERROR,
                /*retry=*/true);
      return;
  }
  NOTREACHED();
}

void EnrollmentScreenHandler::Shutdown() {
  shutdown_ = true;
}

void EnrollmentScreenHandler::ShowEnrollmentStatus(
    policy::EnrollmentStatus status) {
  switch (status.enrollment_code()) {
    case policy::EnrollmentStatus::Code::kSuccess:
      ShowEnrollmentSuccessScreen();
      return;
    case policy::EnrollmentStatus::Code::kNoStateKeys:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_NO_STATE_KEYS,
                /*retry=*/false);
      return;
    case policy::EnrollmentStatus::Code::kRegistrationFailed:
      // Some special cases for generating a nicer message that's more helpful.
      switch (status.client_status()) {
        case policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
          if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
            ShowError(IDS_ENTERPRISE_ENROLLMENT_ACCOUNT_ERROR_MEETS,
                      /*retry=*/true);
          } else {
            ShowError(IDS_ENTERPRISE_ENROLLMENT_ACCOUNT_ERROR, /*retry=*/true);
          }
          break;
        case policy::DM_STATUS_SERVICE_MISSING_LICENSES:
          if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
            ShowError(IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR_MEETS,
                      /*retry=*/true);
          } else {
            ShowError(IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR,
                      /*retry=*/true);
          }
          break;
        case policy::DM_STATUS_SERVICE_DEPROVISIONED:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_DEPROVISIONED_ERROR,
                    /*retry=*/true);
          break;
        case policy::DM_STATUS_SERVICE_DOMAIN_MISMATCH:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_DOMAIN_MISMATCH_ERROR,
                    /*retry=*/true);
          break;
        case policy::DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE:
          if (features::IsEducationEnrollmentOobeFlowEnabled() &&
              config_.license_type == policy::LicenseType::kEducation) {
            ShowError(
                IDS_ENTERPRISE_ENROLLMENT_CONSUMER_ACCOUNT_WITH_EDU_PACKAGED_LICENSE,
                /*retry=*/true);
            break;
          } else {
            ShowError(
                IDS_ENTERPRISE_ENROLLMENT_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE,
                /*retry=*/true);
            break;
          }

        case policy::
            DM_STATUS_SERVICE_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL:
          ShowError(
              IDS_ENTERPRISE_ENROLLMENT_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL,
              /*retry=*/true);
          break;
        case policy::DM_STATUS_SERVICE_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED:
          if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
            ShowError(
                IDS_ENTERPRISE_ENROLLMENT_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED_MEETS,
                /*retry=*/true);
          } else {
            ShowError(
                IDS_ENTERPRISE_ENROLLMENT_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED,
                /*retry=*/true);
          }
          break;
        case policy::DM_STATUS_SERVICE_ILLEGAL_ACCOUNT_FOR_PACKAGED_EDU_LICENSE:
          ShowError(
              IDS_ENTERPRISE_ENROLLMENT_ILLEGAL_ACCOUNT_FOR_PACKAGED_EDU_LICENSE,
              /*retry=*/true);
          break;
        case policy::DM_STATUS_SERVICE_INVALID_PACKAGED_DEVICE_FOR_KIOSK:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_INVALID_PACKAGED_DEVICE_FOR_KIOSK,
                    true);
          break;
        default:
          ShowErrorMessage(
              l10n_util::GetStringFUTF8(
                  IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_FAILED,
                  policy::FormatDeviceManagementStatus(status.client_status())),
              /*retry=*/true);
      }
      return;
    case policy::EnrollmentStatus::Code::kRobotAuthFetchFailed:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ROBOT_AUTH_FETCH_FAILED,
                /*retry=*/true);
      return;
    case policy::EnrollmentStatus::Code::kRobotRefreshFetchFailed:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ROBOT_REFRESH_FETCH_FAILED,
                /*retry=*/true);
      return;
    case policy::EnrollmentStatus::Code::kRobotRefreshStoreFailed:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ROBOT_REFRESH_STORE_FAILED,
                /*retry=*/true);
      return;
    case policy::EnrollmentStatus::Code::kRegistrationBadMode:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_BAD_MODE,
                /*retry=*/false);
      return;
    case policy::EnrollmentStatus::Code::kRegistrationCertFetchFailed:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_CERT_FETCH_FAILED,
                /*retry=*/true);
      return;
    case policy::EnrollmentStatus::Code::kPolicyFetchFailed:
      ShowErrorMessage(
          l10n_util::GetStringFUTF8(
              IDS_ENTERPRISE_ENROLLMENT_STATUS_POLICY_FETCH_FAILED,
              policy::FormatDeviceManagementStatus(status.client_status())),
          /*retry=*/true);
      return;
    case policy::EnrollmentStatus::Code::kValidationFailed:
      ShowErrorMessage(
          l10n_util::GetStringFUTF8(
              IDS_ENTERPRISE_ENROLLMENT_STATUS_VALIDATION_FAILED,
              policy::FormatValidationStatus(status.validation_status())),
          /*retry=*/true);
      return;
    case policy::EnrollmentStatus::Code::kLockError:
      switch (status.lock_status()) {
        case InstallAttributes::LOCK_SUCCESS:
        case InstallAttributes::LOCK_NOT_READY:
          // LOCK_SUCCESS is in contradiction of STATUS_LOCK_ERROR.
          // LOCK_NOT_READY is transient, if retries are given up, LOCK_TIMEOUT
          // is reported instead.  This piece of code is unreached.
          LOG(FATAL) << "Invalid lock status.";
          return;
        case InstallAttributes::LOCK_TIMEOUT:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_TIMEOUT,
                    /*retry=*/false);
          return;
        case InstallAttributes::LOCK_BACKEND_INVALID:
        case InstallAttributes::LOCK_ALREADY_LOCKED:
        case InstallAttributes::LOCK_SET_ERROR:
        case InstallAttributes::LOCK_FINALIZE_ERROR:
        case InstallAttributes::LOCK_READBACK_ERROR:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_ERROR,
                    /*retry=*/false);
          return;
        case InstallAttributes::LOCK_WRONG_DOMAIN:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_WRONG_USER,
                    /*retry=*/true);
          return;
        case InstallAttributes::LOCK_WRONG_MODE:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_WRONG_MODE,
                    /*retry=*/true);
          return;
      }
      NOTREACHED();
      return;
    case policy::EnrollmentStatus::Code::kStoreError:
      ShowErrorMessage(
          l10n_util::GetStringFUTF8(
              IDS_ENTERPRISE_ENROLLMENT_STATUS_STORE_ERROR,
              policy::FormatStoreStatus(status.store_status(),
                                        status.validation_status())),
          /*retry=*/true);
      return;
    case policy::EnrollmentStatus::Code::kAttributeUpdateFailed:
      ShowErrorForDevice(IDS_ENTERPRISE_ENROLLMENT_ATTRIBUTE_ERROR,
                         /*retry=*/false);
      return;
    case policy::EnrollmentStatus::Code::kNoMachineIdentification:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_NO_MACHINE_IDENTIFICATION,
                /*retry=*/false);
      return;
    case policy::EnrollmentStatus::Code::kActiveDirectoryPolicyFetchFailed:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ERROR_ACTIVE_DIRECTORY_POLICY_FETCH,
                /*retry=*/false);
      return;
    case policy::EnrollmentStatus::Code::kDmTokenStoreFailed:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ERROR_SAVE_DEVICE_CONFIGURATION,
                /*retry=*/false);
      return;
    case policy::EnrollmentStatus::Code::kMayNotBlockDevMode:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ERROR_MAY_NOT_BLOCK_DEV_MODE,
                /*retry=*/false);
      return;
  }
  NOTREACHED();
}

// EnrollmentScreenHandler BaseScreenHandler implementation -----

void EnrollmentScreenHandler::InitAfterJavascriptAllowed() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void EnrollmentScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("oauthEnrollScreenTitle",
               IDS_ENTERPRISE_ENROLLMENT_SCREEN_TITLE);
  builder->Add("enrollmentAccountCheckTitle",
               IDS_ENTERPRISE_ACCOUNT_CHECK_TITLE);
  builder->Add("oauthEducationEnrollScreenTitle",
               IDS_EDUCATION_ENROLLMENT_SCREEN_TITLE);
  builder->Add("oauthEnrollNextBtn", IDS_OFFLINE_LOGIN_NEXT_BUTTON_TEXT);
  builder->Add("oauthEnrollSkip", IDS_ENTERPRISE_ENROLLMENT_SKIP);
  if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
    // Use Next text since the setup is not finished.
    builder->Add("oauthEnrollDone", IDS_EULA_NEXT_BUTTON);
  } else {
    builder->Add("oauthEnrollDone", IDS_ENTERPRISE_ENROLLMENT_DONE);
  }
  builder->Add("enterpriseEnrollmentButton",
               IDS_ENTERPRISE_ENROLLMENT_ENROLL_ENTERPRISE);
  builder->Add("kioskEnrollmentButton", IDS_ENTERPRISE_ENROLLMENT_ENROLL_KIOSK);

  builder->Add("enrollmentInProgress",
               IDS_ENTERPRISE_ENROLLMENT_SCREEN_PROGRESS_LABEL);
  builder->Add("oauthEnrollRetry", IDS_ENTERPRISE_ENROLLMENT_RETRY);
  builder->Add("oauthEnrollManualEnrollment",
               IDS_ENTERPRISE_ENROLLMENT_ENROLL_MANUALLY);
  builder->AddF("oauthEnrollSuccess", IDS_ENTERPRISE_ENROLLMENT_SUCCESS,
                ui::GetChromeOSDeviceName());
  builder->Add("oauthEnrollSuccessTitle",
               IDS_ENTERPRISE_ENROLLMENT_SUCCESS_TITLE);
  builder->Add("oauthEnrollEducationSuccessTitle",
               IDS_EDUCATION_ENROLLMENT_SUCCESS_TITLE);
  builder->Add("oauthEnrollSuccessEnterpriseIconExplanation",
               IDS_ENTERPRISE_ENROLLMENT_SUCCESS_ENTERPRISE_ICON_EXPLANATION);
  builder->Add("oauthEnrollErrorTitle", IDS_ENTERPRISE_ENROLLMENT_ERROR_TITLE);
  builder->Add("oauthEducationEnrollErrorTitle",
               IDS_EDUCATION_ENROLLMENT_ERROR_TITLE);
  builder->Add("oauthEnrollDeviceInformation",
               IDS_ENTERPRISE_ENROLLMENT_DEVICE_INFORMATION);
  builder->Add("oauthEnrollExplainAttributeLink",
               IDS_ENTERPRISE_ENROLLMENT_EXPLAIN_ATTRIBUTE_LINK);
  builder->Add("oauthEnrollAttributeExplanation",
               IDS_ENTERPRISE_ENROLLMENT_ATTRIBUTE_EXPLANATION);
  builder->Add("enrollmentAssetIdLabel",
               IDS_ENTERPRISE_ENROLLMENT_ASSET_ID_LABEL);
  builder->Add("enrollmentLocationLabel",
               IDS_ENTERPRISE_ENROLLMENT_LOCATION_LABEL);
  builder->Add("oauthEnrollWorking", IDS_ENTERPRISE_ENROLLMENT_WORKING_MESSAGE);
  // Kiosk enrollment related string.
  builder->Add("oauthEnrollKioskEnrollmentConfirmTitle",
               IDS_ENTERPRISE_ENROLLMENT_KIOSK_CONFIRM_TITLE);
  builder->Add("oauthEnrollKioskEnrollmentConfirmMessage",
               IDS_ENTERPRISE_ENROLLMENT_KIOSK_CONFIRM_MESSAGE);
  builder->Add("oauthEnrollKioskEnrollmentConfirmPowerwashMessage",
               IDS_ENTERPRISE_ENROLLMENT_KIOSK_CONFIRM_POWERWASH_MESSAGE);
  builder->Add("oauthEnrollKioskCancelEnrollmentButton",
               IDS_ENTERPRISE_ENROLLMENT_KIOSK_CANCEL_BUTTON);
  builder->Add("oauthEnrollKioskEnrollmentConfirmButton",
               IDS_ENTERPRISE_ENROLLMENT_KIOSK_CONFIRM_BUTTON);
  builder->Add("oauthEnrollKioskEnrollmentWorkingTitle",
               IDS_ENTERPRISE_ENROLLMENT_KIOSK_WORKING_TITLE);
  builder->Add("oauthEnrollKioskEnrollmentSuccessTitle",
               IDS_ENTERPRISE_ENROLLMENT_KIOSK_SUCCESS_TITLE);
  // Do not use AddF for this string as it will be rendered by the JS code.
  builder->Add("oauthEnrollAbeSuccessDomain",
               IDS_ENTERPRISE_ENROLLMENT_SUCCESS_DOMAIN);
  builder->Add("fatalEnrollmentError",
               IDS_ENTERPRISE_ENROLLMENT_AUTH_FATAL_ERROR);
  builder->Add("insecureURLEnrollmentError",
               IDS_ENTERPRISE_ENROLLMENT_AUTH_INSECURE_URL_ERROR);

  // TPM checking spinner strings.
  builder->Add("TPMCheckTitle", IDS_TPM_CHECK_TITLE);
  builder->Add("TPMCheckSubtitle", IDS_TPM_CHECK_SUBTITLE);
  builder->Add("cancelButton", IDS_CANCEL);

  // Skip Confirmation Dialogue strings.
  builder->Add("skipConfirmationDialogTitle", IDS_SKIP_ENROLLMENT_DIALOG_TITLE);
  builder->Add("skipConfirmationDialogText", IDS_SKIP_ENROLLMENT_DIALOG_TEXT);
  builder->Add("skipConfirmationDialogEducationTitle",
               IDS_SKIP_ENROLLMENT_DIALOG_EDUCATION_TITLE);
  builder->Add("skipConfirmationDialogEducationText",
               IDS_SKIP_ENROLLMENT_DIALOG_EDUCATION_TEXT);
  builder->Add("skipConfirmationGoBackButton",
               IDS_SKIP_ENROLLMENT_DIALOG_GO_BACK_BUTTON);
  builder->Add("skipConfirmationSkipButton",
               IDS_SKIP_ENROLLMENT_DIALOG_SKIP_BUTTON);

  /* Active Directory strings */
  // TODO(b/280560446) Remove once references in HTML/JS are removed.
  builder->Add("oauthEnrollAdMachineNameInput", IDS_AD_DEVICE_NAME_INPUT_LABEL);
  builder->Add("oauthEnrollAdDomainJoinWelcomeMessage",
               IDS_AD_DOMAIN_JOIN_WELCOME_MESSAGE);
  builder->Add("adAuthLoginUsername", IDS_AD_AUTH_LOGIN_USER);
  builder->Add("adLoginInvalidUsername", IDS_AD_INVALID_USERNAME);
  builder->Add("adLoginPassword", IDS_AD_LOGIN_PASSWORD);
  builder->Add("adLoginInvalidPassword", IDS_AD_INVALID_PASSWORD);
  builder->Add("adJoinErrorMachineNameInvalid", IDS_AD_DEVICE_NAME_INVALID);
  builder->Add("adJoinErrorMachineNameTooLong", IDS_AD_DEVICE_NAME_TOO_LONG);
  builder->Add("adJoinErrorMachineNameInvalidFormat",
               IDS_AD_DEVICE_NAME_INVALID_FORMAT);
  builder->Add("adJoinMoreOptions", IDS_AD_MORE_OPTIONS_BUTTON);
  builder->Add("adUnlockTitle", IDS_AD_UNLOCK_TITLE_MESSAGE);
  builder->Add("adUnlockSubtitle", IDS_AD_UNLOCK_SUBTITLE_MESSAGE);
  builder->Add("adUnlockPassword", IDS_AD_UNLOCK_CONFIG_PASSWORD);
  builder->Add("adUnlockIncorrectPassword", IDS_AD_UNLOCK_INCORRECT_PASSWORD);
  builder->Add("adUnlockPasswordSkip", IDS_AD_UNLOCK_PASSWORD_SKIP);
  builder->Add("adJoinOrgUnit", IDS_AD_ORG_UNIT_HINT);
  builder->Add("adJoinCancel", IDS_AD_CANCEL_BUTTON);
  builder->Add("adJoinSave", IDS_AD_SAVE_BUTTON);
  builder->Add("selectEncryption", IDS_AD_ENCRYPTION_SELECTION_SELECT);
  builder->Add("selectConfiguration", IDS_AD_CONFIG_SELECTION_SELECT);
  /* End of Active Directory strings */
}

void EnrollmentScreenHandler::DeclareJSCallbacks() {
  AddCallback("toggleFakeEnrollment",
              &EnrollmentScreenHandler::HandleToggleFakeEnrollment);
  AddCallback("oauthEnrollClose", &EnrollmentScreenHandler::HandleClose);
  AddCallback("oauthEnrollCompleteLogin",
              &EnrollmentScreenHandler::HandleCompleteLogin);
  AddCallback("enterpriseIdentifierEntered",
              &EnrollmentScreenHandler::HandleIdentifierEntered);
  AddCallback("oauthEnrollRetry", &EnrollmentScreenHandler::HandleRetry);
  AddCallback("frameLoadingCompleted",
              &EnrollmentScreenHandler::HandleFrameLoadingCompleted);
  AddCallback("oauthEnrollAttributes",
              &EnrollmentScreenHandler::HandleDeviceAttributesProvided);
  AddCallback("oauthEnrollOnLearnMore",
              &EnrollmentScreenHandler::HandleOnLearnMore);
}

void EnrollmentScreenHandler::GetAdditionalParameters(
    base::Value::Dict* parameters) {
  // TODO(b/280560446) Remove this placeholder once
  // chrome/browser/resources/chromeos/login/screens/common/offline_ad_login.js
  // is removed (currently, some tests still depend on this list to be
  // non-empty).
  parameters->Set(
      "encryptionTypesList",
      base::Value::List().Append(base::Value::Dict()
                                     .Set("title", "some title")
                                     .Set("subtitle", "some subtitle")
                                     .Set("value", 42)
                                     .Set("selected", false)));
}

bool EnrollmentScreenHandler::IsOnEnrollmentScreen() {
  return (GetCurrentScreen() == kScreenId);
}

void EnrollmentScreenHandler::ShowSkipConfirmationDialog() {
  CallExternalAPI("showSkipConfirmationDialog");
}

// EnrollmentScreenHandler, private -----------------------------
void EnrollmentScreenHandler::HandleToggleFakeEnrollment() {
  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "HandleToggleFakeEnrollment";
  policy::PolicyOAuth2TokenFetcher::UseFakeTokensForTesting();
  WizardController::SkipEnrollmentPromptsForTesting();
  use_fake_login_for_testing_ = true;
}

void EnrollmentScreenHandler::HandleClose(const std::string& reason) {
  DCHECK(controller_);
  if (reason == "cancel") {
    controller_->OnCancel();
  } else if (reason == "done") {
    controller_->OnConfirmationClosed();
  } else {
    NOTREACHED();
  }
}

void EnrollmentScreenHandler::HandleCompleteLogin(const std::string& user,
                                                  int license_type) {
  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "HandleCompleteLogin";

  // When the network service is enabled, the webRequest API doesn't expose
  // cookie headers. So manually fetch the cookies for the GAIA URL from the
  // CookieManager.
  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  content::StoragePartition* partition =
      signin_partition_manager->GetCurrentStoragePartition();

  // Validity check that partition did not change during enrollment flow.
  DCHECK_EQ(signin_partition_manager->GetCurrentStoragePartitionName(),
            signin_partition_name_);

  network::mojom::CookieManager* cookie_manager =
      partition->GetCookieManagerForBrowserProcess();
  if (!oauth_code_waiter_) {
    // Set listener before requesting the cookies to avoid race conditions.
    oauth_code_waiter_ = std::make_unique<CookieWaiter>(
        cookie_manager, kOAUTHCodeCookie,
        base::BindRepeating(&EnrollmentScreenHandler::
                                ContinueAuthenticationWhenCookiesAvailable,
                            weak_ptr_factory_.GetWeakPtr(), user, license_type),
        base::BindOnce(&EnrollmentScreenHandler::OnCookieWaitTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  ContinueAuthenticationWhenCookiesAvailable(user, license_type);
}

void EnrollmentScreenHandler::ContinueAuthenticationWhenCookiesAvailable(
    const std::string& user,
    int license_type) {
  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  content::StoragePartition* partition =
      signin_partition_manager->GetCurrentStoragePartition();

  // Validity check that partition did not change during enrollment flow.
  DCHECK_EQ(signin_partition_manager->GetCurrentStoragePartitionName(),
            signin_partition_name_);

  network::mojom::CookieManager* cookie_manager =
      partition->GetCookieManagerForBrowserProcess();
  cookie_manager->GetCookieList(
      GaiaUrls::GetInstance()->gaia_url(),
      net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection::Todo(),
      base::BindOnce(&EnrollmentScreenHandler::OnGetCookiesForCompleteLogin,
                     weak_ptr_factory_.GetWeakPtr(), user, license_type));
}

void EnrollmentScreenHandler::OnGetCookiesForCompleteLogin(
    const std::string& user,
    int license_type,
    const net::CookieAccessResultList& cookies,
    const net::CookieAccessResultList& excluded_cookies) {
  std::string auth_code;
  for (const auto& cookie_with_access_result : cookies) {
    if (cookie_with_access_result.cookie.Name() == kOAUTHCodeCookie) {
      auth_code = cookie_with_access_result.cookie.Value();
      break;
    }
  }

  // Allow testing to continue without a oauth cookie.
  if (auth_code.empty() && !use_fake_login_for_testing_) {
    // Will try again from oauth_code_waiter callback.

    // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
    // in the logs.
    LOG(WARNING) << "OAuth cookie empty, still waiting";
    return;
  }

  oauth_code_waiter_.reset();
  DCHECK(controller_);
  controller_->OnLoginDone(gaia::SanitizeEmail(user), license_type, auth_code);
}

void EnrollmentScreenHandler::OnCookieWaitTimeout() {
  LOG(ERROR) << "Timeout waiting for OAuth cookie";
  oauth_code_waiter_.reset();

  // If enrollment ends and the browser is being restarted, the renderers are
  // killed so we can not talk to them anymore.
  if (!shutdown_)
    ShowError(IDS_LOGIN_FATAL_ERROR_NO_AUTH_TOKEN, true);
}

void EnrollmentScreenHandler::HandleIdentifierEntered(
    const std::string& email) {
  DCHECK(controller_);
  controller_->OnIdentifierEntered(email);
}

void EnrollmentScreenHandler::HandleRetry() {
  DCHECK(controller_);
  controller_->OnRetry();
}

void EnrollmentScreenHandler::HandleFrameLoadingCompleted() {
  controller_->OnFrameLoadingCompleted();
}

void EnrollmentScreenHandler::HandleDeviceAttributesProvided(
    const std::string& asset_id,
    const std::string& location) {
  controller_->OnDeviceAttributeProvided(asset_id, location);
}

void EnrollmentScreenHandler::HandleOnLearnMore() {
  if (!help_app_.get())
    help_app_ = new HelpAppLauncher(
        LoginDisplayHost::default_host()->GetNativeWindow());
  help_app_->ShowHelpTopic(HelpAppLauncher::HELP_DEVICE_ATTRIBUTES);
}

void EnrollmentScreenHandler::ShowStep(const std::string& step) {
  CallExternalAPI("showStep", step);
}

void EnrollmentScreenHandler::ShowError(int message_id, bool retry) {
  ShowErrorMessage(l10n_util::GetStringUTF8(message_id), retry);
}

void EnrollmentScreenHandler::ShowErrorForDevice(int message_id, bool retry) {
  ShowErrorMessage(
      l10n_util::GetStringFUTF8(message_id, ui::GetChromeOSDeviceName()),
      retry);
}

void EnrollmentScreenHandler::ShowEnrollmentDuringTrialNotAllowedError() {
  ShowInWebUI();
  ShowErrorMessage(
      l10n_util::GetStringFUTF8(
          IDS_ENTERPRISE_ENROLLMENT_STATUS_CLOUD_READY_NOT_ALLOWED,
          l10n_util::GetStringUTF16(IDS_INSTALLED_PRODUCT_OS_NAME)),
      /*retry=*/false);
}

void EnrollmentScreenHandler::ShowErrorMessage(const std::string& message,
                                               bool retry) {
  CallExternalAPI("showError", message, retry);
}

void EnrollmentScreenHandler::DoShow() {
  if (config_.is_mode_attestation()) {
    // Don't need sign-in partition for attestation enrollment.
    DoShowWithData(ScreenDataForAttestationEnrollment());
    return;
  }

  // Start a new session with SigninPartitionManager, generating a unique
  // StoragePartition.
  login::SigninPartitionManager* signin_partition_manager =
      login::SigninPartitionManager::Factory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  signin_partition_manager->StartSigninSession(
      web_ui()->GetWebContents(),
      base::BindOnce(&EnrollmentScreenHandler::DoShowWithPartition,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentScreenHandler::DoShowWithPartition(
    const std::string& partition_name) {
  // If enrollment ends and the browser is being restarted, the renderers are
  // killed so we can not talk to them anymore.
  if (shutdown_)
    return;

  signin_partition_name_ = partition_name;

  DoShowWithData(ScreenDataForOAuthEnrollment());
}

void EnrollmentScreenHandler::DoShowWithData(base::Value::Dict screen_data) {
  ShowInWebUI(std::move(screen_data));
  if (first_show_) {
    first_show_ = false;
    controller_->OnFirstShow();
  }
}

base::Value::Dict
EnrollmentScreenHandler::ScreenDataForAttestationEnrollment() {
  // Attestation-based enrollment doesn't require additional screen data.
  return ScreenDataCommon();
}

base::Value::Dict EnrollmentScreenHandler::ScreenDataForOAuthEnrollment() {
  base::Value::Dict screen_data = ScreenDataCommon();

  screen_data.Set("webviewPartitionName", signin_partition_name_);
  screen_data.Set("gaiaUrl", GaiaUrls::GetInstance()->gaia_url().spec());
  screen_data.Set(
      "gaiaPath",
      GaiaUrls::GetInstance()->embedded_setup_chromeos_url().path().substr(1));
  screen_data.Set("clientId",
                  GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  screen_data.Set("management_domain", config_.management_domain);
  screen_data.Set("gaia_buttons_type",
                  GetGaiaButtonsTypeString(gaia_buttons_type_));
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  if (!app_locale.empty())
    screen_data.Set("hl", app_locale);
  const std::string& email = config_.enrollment_nudge_email;
  if (!email.empty()) {
    screen_data.Set("email", email);
  }
  return screen_data;
}

base::Value::Dict EnrollmentScreenHandler::ScreenDataCommon() {
  base::Value::Dict screen_data;

  screen_data.Set("enrollment_mode", EnrollmentModeToUIMode(config_.mode));
  screen_data.Set("is_enrollment_enforced", config_.is_forced());
  screen_data.Set("attestationBased", config_.is_mode_attestation());
  screen_data.Set("flow", GetFlowString(flow_type_));

  if (ShouldSpecifyLicenseType(config_)) {
    screen_data.Set("license", GetLicenseString(config_.license_type));
  }

  return screen_data;
}

}  // namespace ash
