// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/enrollment_screen_handler.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/authpolicy/authpolicy_helper.h"
#include "chrome/browser/ash/login/error_screens_histogram_helper.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_requisition_manager.h"
#include "chrome/browser/chromeos/policy/policy_oauth2_token_fetcher.h"
#include "chrome/browser/policy/enrollment_status.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/cookie_waiter.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/login/localized_values_builder.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {
namespace {

// Enrollment step names.
const char kEnrollmentStepSignin[] = "signin";
const char kEnrollmentStepAdJoin[] = "ad-join";
const char kEnrollmentStepSuccess[] = "success";
const char kEnrollmentStepWorking[] = "working";

// Enrollment mode constants used in the UI. This needs to be kept in sync with
// oobe_screen_oauth_enrollment.js.
const char kEnrollmentModeUIForced[] = "forced";
const char kEnrollmentModeUIManual[] = "manual";
const char kEnrollmentModeUIRecovery[] = "recovery";

constexpr char kActiveDirectoryJoinHistogram[] =
    "Enterprise.ActiveDirectoryJoin";

constexpr char kOAUTHCodeCookie[] = "oauth_code";

// Converts `mode` to a mode identifier for the UI.
std::string EnrollmentModeToUIMode(policy::EnrollmentConfig::Mode mode) {
  switch (mode) {
    case policy::EnrollmentConfig::MODE_NONE:
    case policy::EnrollmentConfig::MODE_OFFLINE_DEMO:
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
    case policy::EnrollmentConfig::MODE_ATTESTATION_ENROLLMENT_TOKEN:
      return kEnrollmentModeUIForced;
    case policy::EnrollmentConfig::MODE_RECOVERY:
    case policy::EnrollmentConfig::MODE_ENROLLED_ROLLBACK:
      return kEnrollmentModeUIRecovery;
  }

  NOTREACHED() << "Bad enrollment mode " << mode;
  return kEnrollmentModeUIManual;
}

// Returns network name by service path.
std::string GetNetworkName(const std::string& service_path) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (!network)
    return std::string();
  return network->name();
}

bool IsBehindCaptivePortal(NetworkStateInformer::State state,
                           NetworkError::ErrorReason reason) {
  return state == NetworkStateInformer::CAPTIVE_PORTAL ||
         reason == NetworkError::ERROR_REASON_PORTAL_DETECTED;
}

bool IsProxyError(NetworkStateInformer::State state,
                  NetworkError::ErrorReason reason) {
  return state == NetworkStateInformer::PROXY_AUTH_REQUIRED ||
         reason == NetworkError::ERROR_REASON_PROXY_AUTH_CANCELLED ||
         reason == NetworkError::ERROR_REASON_PROXY_CONNECTION_FAILED;
}

constexpr struct {
  const char* id;
  int title_id;
  int subtitle_id;
  authpolicy::KerberosEncryptionTypes encryption_types;
} kEncryptionTypes[] = {
    {"strong", IDS_AD_ENCRYPTION_STRONG_TITLE,
     IDS_AD_ENCRYPTION_STRONG_SUBTITLE,
     authpolicy::KerberosEncryptionTypes::ENC_TYPES_STRONG},
    {"all", IDS_AD_ENCRYPTION_ALL_TITLE, IDS_AD_ENCRYPTION_ALL_SUBTITLE,
     authpolicy::KerberosEncryptionTypes::ENC_TYPES_ALL},
    {"legacy", IDS_AD_ENCRYPTION_LEGACY_TITLE,
     IDS_AD_ENCRYPTION_LEGACY_SUBTITLE,
     authpolicy::KerberosEncryptionTypes::ENC_TYPES_LEGACY}};

base::ListValue GetEncryptionTypesList() {
  const authpolicy::KerberosEncryptionTypes default_types =
      authpolicy::KerberosEncryptionTypes::ENC_TYPES_STRONG;
  base::ListValue encryption_list;
  for (const auto& enc_types : kEncryptionTypes) {
    base::DictionaryValue enc_option;
    enc_option.SetKey(
        "title", base::Value(l10n_util::GetStringUTF16(enc_types.title_id)));
    enc_option.SetKey(
        "subtitle",
        base::Value(l10n_util::GetStringUTF16(enc_types.subtitle_id)));
    enc_option.SetKey("value", base::Value(enc_types.id));
    enc_option.SetKey("selected",
                      base::Value(default_types == enc_types.encryption_types));
    encryption_list.Append(std::move(enc_option));
  }
  return encryption_list;
}

authpolicy::KerberosEncryptionTypes TranslateEncryptionTypesString(
    const std::string& string_id) {
  for (const auto& enc_types : kEncryptionTypes) {
    if (enc_types.id == string_id)
      return enc_types.encryption_types;
  }
  NOTREACHED();
  return authpolicy::KerberosEncryptionTypes::ENC_TYPES_STRONG;
}

}  // namespace

constexpr StaticOobeScreenId EnrollmentScreenView::kScreenId;

// EnrollmentScreenHandler, public ------------------------------

EnrollmentScreenHandler::EnrollmentScreenHandler(
    JSCallsContainer* js_calls_container,
    const scoped_refptr<NetworkStateInformer>& network_state_informer,
    ErrorScreen* error_screen)
    : BaseScreenHandler(kScreenId, js_calls_container),
      network_state_informer_(network_state_informer),
      error_screen_(error_screen),
      histogram_helper_(new ErrorScreensHistogramHelper("Enrollment")) {
  DCHECK(network_state_informer_.get());
  DCHECK(error_screen_);
  network_state_informer_->AddObserver(this);
}

EnrollmentScreenHandler::~EnrollmentScreenHandler() {
  network_state_informer_->RemoveObserver(this);
}

// EnrollmentScreenHandler, WebUIMessageHandler implementation --

void EnrollmentScreenHandler::RegisterMessages() {
  AddCallback("toggleFakeEnrollment",
              &EnrollmentScreenHandler::HandleToggleFakeEnrollment);
  AddCallback("oauthEnrollClose", &EnrollmentScreenHandler::HandleClose);
  AddCallback("oauthEnrollCompleteLogin",
              &EnrollmentScreenHandler::HandleCompleteLogin);
  AddCallback("oauthEnrollAdCompleteLogin",
              &EnrollmentScreenHandler::HandleAdCompleteLogin);
  AddCallback("oauthEnrollAdUnlockConfiguration",
              &EnrollmentScreenHandler::HandleAdUnlockConfiguration);
  AddCallback("oauthEnrollRetry", &EnrollmentScreenHandler::HandleRetry);
  AddCallback("frameLoadingCompleted",
              &EnrollmentScreenHandler::HandleFrameLoadingCompleted);
  AddCallback("oauthEnrollAttributes",
              &EnrollmentScreenHandler::HandleDeviceAttributesProvided);
  AddCallback("oauthEnrollOnLearnMore",
              &EnrollmentScreenHandler::HandleOnLearnMore);
}

// EnrollmentScreenHandler
//      EnrollmentScreenActor implementation -----------------------------------

void EnrollmentScreenHandler::SetEnrollmentConfig(
    Controller* controller,
    const policy::EnrollmentConfig& config) {
  CHECK(config.should_enroll());
  controller_ = controller;
  config_ = config;
}

void EnrollmentScreenHandler::Show() {
  if (!page_is_ready())
    show_on_init_ = true;
  else
    DoShow();
}

void EnrollmentScreenHandler::Hide() {}

void EnrollmentScreenHandler::ShowSigninScreen() {
  observe_network_failure_ = true;
  ShowStep(kEnrollmentStepSignin);
}

void EnrollmentScreenHandler::ShowActiveDirectoryScreen(
    const std::string& domain_join_config,
    const std::string& machine_name,
    const std::string& username,
    authpolicy::ErrorType error) {
  observe_network_failure_ = false;
  if (active_directory_join_type_ == ActiveDirectoryDomainJoinType::COUNT) {
    active_directory_join_type_ =
        ActiveDirectoryDomainJoinType::WITHOUT_CONFIGURATION;
  }

  if (!domain_join_config.empty()) {
    active_directory_domain_join_config_ = domain_join_config;
    active_directory_join_type_ =
        ActiveDirectoryDomainJoinType::NOT_USING_CONFIGURATION;
  }
  switch (error) {
    case authpolicy::ERROR_NONE: {
      CallJS("login.OAuthEnrollmentScreen.setAdJoinParams",
             std::string() /* machineName */, std::string() /* userName */,
             static_cast<int>(ActiveDirectoryErrorState::NONE),
             !active_directory_domain_join_config_
                  .empty() /* show_unlock_password */);
      ShowStep(kEnrollmentStepAdJoin);
      return;
    }
    case authpolicy::ERROR_NETWORK_PROBLEM:
      // Could be a network problem, but could also be a misspelled domain name.
      ShowError(IDS_AD_AUTH_NETWORK_ERROR, true);
      return;
    case authpolicy::ERROR_PARSE_UPN_FAILED:
    case authpolicy::ERROR_BAD_USER_NAME:
      CallJS("login.OAuthEnrollmentScreen.setAdJoinParams", machine_name,
             username,
             static_cast<int>(ActiveDirectoryErrorState::BAD_USERNAME),
             false /* show_unlock_password */);
      ShowStep(kEnrollmentStepAdJoin);
      return;
    case authpolicy::ERROR_BAD_PASSWORD:
      CallJS("login.OAuthEnrollmentScreen.setAdJoinParams", machine_name,
             username,
             static_cast<int>(ActiveDirectoryErrorState::BAD_AUTH_PASSWORD),
             false /* show_unlock_password */);
      ShowStep(kEnrollmentStepAdJoin);
      return;
    case authpolicy::ERROR_MACHINE_NAME_TOO_LONG:
      CallJS("login.OAuthEnrollmentScreen.setAdJoinParams", machine_name,
             username,
             static_cast<int>(ActiveDirectoryErrorState::MACHINE_NAME_TOO_LONG),
             false /* show_unlock_password */);
      ShowStep(kEnrollmentStepAdJoin);
      return;
    case authpolicy::ERROR_INVALID_MACHINE_NAME:
      CallJS("login.OAuthEnrollmentScreen.setAdJoinParams", machine_name,
             username,
             static_cast<int>(ActiveDirectoryErrorState::MACHINE_NAME_INVALID),
             false /* show_unlock_password */);
      ShowStep(kEnrollmentStepAdJoin);
      return;
    case authpolicy::ERROR_PASSWORD_EXPIRED:
      ShowError(IDS_AD_PASSWORD_EXPIRED, true);
      return;
    case authpolicy::ERROR_JOIN_ACCESS_DENIED:
      ShowError(IDS_AD_USER_DENIED_TO_JOIN_DEVICE, true);
      return;
    case authpolicy::ERROR_USER_HIT_JOIN_QUOTA:
      ShowError(IDS_AD_USER_HIT_JOIN_QUOTA, true);
      return;
    case authpolicy::ERROR_OU_DOES_NOT_EXIST:
      ShowError(IDS_AD_OU_DOES_NOT_EXIST, true);
      return;
    case authpolicy::ERROR_OU_ACCESS_DENIED:
      ShowError(IDS_AD_OU_ACCESS_DENIED, true);
      return;
    case authpolicy::ERROR_SETTING_OU_FAILED:
      ShowError(IDS_AD_OU_SETTING_FAILED, true);
      return;
    case authpolicy::ERROR_KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE:
      ShowError(IDS_AD_NOT_SUPPORTED_ENCRYPTION, true);
      return;
#if !defined(ARCH_CPU_X86_64)
    // Currently, the Active Directory integration is only supported on x86_64
    // systems. (see https://crbug.com/676602)
    case authpolicy::ERROR_DBUS_FAILURE:
      ShowError(IDS_AD_BOARD_NOT_SUPPORTED, true);
      return;
#endif
    default:
      LOG(ERROR) << "Unhandled error code: " << error;
      ShowError(IDS_AD_DOMAIN_JOIN_UNKNOWN_ERROR, true);
      return;
  }
}

void EnrollmentScreenHandler::ShowAttributePromptScreen(
    const std::string& asset_id,
    const std::string& location) {
  CallJS("login.OAuthEnrollmentScreen.showAttributePromptStep", asset_id,
         location);
}

void EnrollmentScreenHandler::ShowEnrollmentSpinnerScreen() {
  ShowStep(kEnrollmentStepWorking);
}

void EnrollmentScreenHandler::SetEnterpriseDomainInfo(
    const std::string& manager,
    const std::u16string& device_type) {
  CallJS("login.OAuthEnrollmentScreen.setEnterpriseDomainInfo", manager,
         device_type);
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
      ShowError(IDS_ENTERPRISE_ENROLLMENT_AUTH_FATAL_ERROR, false);
      return;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_AUTH_ACCOUNT_ERROR, true);
      return;
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_AUTH_NETWORK_ERROR, true);
      return;
    case GoogleServiceAuthError::NUM_STATES:
      break;
  }
  NOTREACHED();
}

void EnrollmentScreenHandler::ShowOtherError(
    EnterpriseEnrollmentHelper::OtherError error) {
  switch (error) {
    case EnterpriseEnrollmentHelper::OTHER_ERROR_DOMAIN_MISMATCH:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_WRONG_USER, true);
      return;
    case EnterpriseEnrollmentHelper::OTHER_ERROR_FATAL:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_FATAL_ENROLLMENT_ERROR, true);
      return;
  }
  NOTREACHED();
}

void EnrollmentScreenHandler::Shutdown() {
  shutdown_ = true;
}

void EnrollmentScreenHandler::ShowEnrollmentStatus(
    policy::EnrollmentStatus status) {
  switch (status.status()) {
    case policy::EnrollmentStatus::SUCCESS:
      ShowEnrollmentSuccessScreen();
      return;
    case policy::EnrollmentStatus::NO_STATE_KEYS:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_NO_STATE_KEYS, false);
      return;
    case policy::EnrollmentStatus::REGISTRATION_FAILED:
      // Some special cases for generating a nicer message that's more helpful.
      switch (status.client_status()) {
        case policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
          if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
            ShowError(IDS_ENTERPRISE_ENROLLMENT_ACCOUNT_ERROR_MEETS, true);
          } else {
            ShowError(IDS_ENTERPRISE_ENROLLMENT_ACCOUNT_ERROR, true);
          }
          break;
        case policy::DM_STATUS_SERVICE_MISSING_LICENSES:
          if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
            ShowError(IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR_MEETS,
                      true);
          } else {
            ShowError(IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR, true);
          }
          break;
        case policy::DM_STATUS_SERVICE_DEPROVISIONED:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_DEPROVISIONED_ERROR, true);
          break;
        case policy::DM_STATUS_SERVICE_DOMAIN_MISMATCH:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_DOMAIN_MISMATCH_ERROR, true);
          break;
        case policy::DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE:
          ShowError(
              IDS_ENTERPRISE_ENROLLMENT_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE,
              true);
          break;
        case policy::
            DM_STATUS_SERVICE_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL:
          ShowError(
              IDS_ENTERPRISE_ENROLLMENT_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL,
              true);
          break;
        case policy::DM_STATUS_SERVICE_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED:
          if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
            ShowError(
                IDS_ENTERPRISE_ENROLLMENT_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED_MEETS,
                true);
          } else {
            ShowError(
                IDS_ENTERPRISE_ENROLLMENT_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED,
                true);
          }
          break;
        case policy::DM_STATUS_SERVICE_ILLEGAL_ACCOUNT_FOR_PACKAGED_EDU_LICENSE:
          ShowError(
              IDS_ENTERPRISE_ENROLLMENT_ILLEGAL_ACCOUNT_FOR_PACKAGED_EDU_LICENSE,
              true);
          break;
        default:
          ShowErrorMessage(
              l10n_util::GetStringFUTF8(
                  IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_FAILED,
                  policy::FormatDeviceManagementStatus(status.client_status())),
              true);
      }
      return;
    case policy::EnrollmentStatus::ROBOT_AUTH_FETCH_FAILED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ROBOT_AUTH_FETCH_FAILED, true);
      return;
    case policy::EnrollmentStatus::ROBOT_REFRESH_FETCH_FAILED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ROBOT_REFRESH_FETCH_FAILED, true);
      return;
    case policy::EnrollmentStatus::ROBOT_REFRESH_STORE_FAILED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ROBOT_REFRESH_STORE_FAILED, true);
      return;
    case policy::EnrollmentStatus::REGISTRATION_BAD_MODE:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_BAD_MODE, false);
      return;
    case policy::EnrollmentStatus::REGISTRATION_CERT_FETCH_FAILED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_CERT_FETCH_FAILED,
                true);
      return;
    case policy::EnrollmentStatus::POLICY_FETCH_FAILED:
      ShowErrorMessage(
          l10n_util::GetStringFUTF8(
              IDS_ENTERPRISE_ENROLLMENT_STATUS_POLICY_FETCH_FAILED,
              policy::FormatDeviceManagementStatus(status.client_status())),
          true);
      return;
    case policy::EnrollmentStatus::VALIDATION_FAILED:
      ShowErrorMessage(
          l10n_util::GetStringFUTF8(
              IDS_ENTERPRISE_ENROLLMENT_STATUS_VALIDATION_FAILED,
              policy::FormatValidationStatus(status.validation_status())),
          true);
      return;
    case policy::EnrollmentStatus::LOCK_ERROR:
      switch (status.lock_status()) {
        case InstallAttributes::LOCK_SUCCESS:
        case InstallAttributes::LOCK_NOT_READY:
          // LOCK_SUCCESS is in contradiction of STATUS_LOCK_ERROR.
          // LOCK_NOT_READY is transient, if retries are given up, LOCK_TIMEOUT
          // is reported instead.  This piece of code is unreached.
          LOG(FATAL) << "Invalid lock status.";
          return;
        case InstallAttributes::LOCK_TIMEOUT:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_TIMEOUT, false);
          return;
        case InstallAttributes::LOCK_BACKEND_INVALID:
        case InstallAttributes::LOCK_ALREADY_LOCKED:
        case InstallAttributes::LOCK_SET_ERROR:
        case InstallAttributes::LOCK_FINALIZE_ERROR:
        case InstallAttributes::LOCK_READBACK_ERROR:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_ERROR, false);
          return;
        case InstallAttributes::LOCK_WRONG_DOMAIN:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_WRONG_USER, true);
          return;
        case InstallAttributes::LOCK_WRONG_MODE:
          ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_WRONG_MODE, true);
          return;
      }
      NOTREACHED();
      return;
    case policy::EnrollmentStatus::STORE_ERROR:
      ShowErrorMessage(
          l10n_util::GetStringFUTF8(
              IDS_ENTERPRISE_ENROLLMENT_STATUS_STORE_ERROR,
              policy::FormatStoreStatus(status.store_status(),
                                        status.validation_status())),
          true);
      return;
    case policy::EnrollmentStatus::ATTRIBUTE_UPDATE_FAILED:
      ShowErrorForDevice(IDS_ENTERPRISE_ENROLLMENT_ATTRIBUTE_ERROR, false);
      return;
    case policy::EnrollmentStatus::NO_MACHINE_IDENTIFICATION:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_STATUS_NO_MACHINE_IDENTIFICATION,
                false);
      return;
    case policy::EnrollmentStatus::ACTIVE_DIRECTORY_POLICY_FETCH_FAILED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ERROR_ACTIVE_DIRECTORY_POLICY_FETCH,
                false);
      return;
    case policy::EnrollmentStatus::DM_TOKEN_STORE_FAILED:
      ShowError(IDS_ENTERPRISE_ENROLLMENT_ERROR_SAVE_DEVICE_CONFIGURATION,
                false);
      return;
    case policy::EnrollmentStatus::OFFLINE_POLICY_LOAD_FAILED:
    case policy::EnrollmentStatus::OFFLINE_POLICY_DECODING_FAILED:
      // OFFLINE_POLICY_LOAD_FAILED and OFFLINE_POLICY_DECODING_FAILED happen
      // only within MODE_OFFLINE_DEMO flow, which shouldn't happen here.
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

// EnrollmentScreenHandler BaseScreenHandler implementation -----

void EnrollmentScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void EnrollmentScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("oauthEnrollScreenTitle",
               IDS_ENTERPRISE_ENROLLMENT_SCREEN_TITLE);
  builder->Add("oauthEnrollNextBtn", IDS_OFFLINE_LOGIN_NEXT_BUTTON_TEXT);
  builder->Add("oauthEnrollSkip", IDS_ENTERPRISE_ENROLLMENT_SKIP);
  if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
    // Use Next text since the setup is not finished.
    builder->Add("oauthEnrollDone", IDS_EULA_NEXT_BUTTON);
  } else {
    builder->Add("oauthEnrollDone", IDS_ENTERPRISE_ENROLLMENT_DONE);
  }
  builder->Add("oauthEnrollRetry", IDS_ENTERPRISE_ENROLLMENT_RETRY);
  builder->Add("oauthEnrollManualEnrollment",
               IDS_ENTERPRISE_ENROLLMENT_ENROLL_MANUALLY);
  builder->AddF("oauthEnrollSuccess", IDS_ENTERPRISE_ENROLLMENT_SUCCESS,
                ui::GetChromeOSDeviceName());
  builder->Add("oauthEnrollSuccessTitle",
               IDS_ENTERPRISE_ENROLLMENT_SUCCESS_TITLE);
  builder->Add("oauthEnrollErrorTitle", IDS_ENTERPRISE_ENROLLMENT_ERROR_TITLE);
  builder->Add("enrollmentSuccessIllustrationTitle",
               IDS_ENTERPRISE_ENROLLMENT_SUCCESS_ILLUSTRATION_TITLE);
  builder->Add("enrollmentErrorIllustrationTitle",
               IDS_ENTERPRISE_ENROLLMENT_ERROR_ILLUSTRATION_TITLE);
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
  // Do not use AddF for this string as it will be rendered by the JS code.
  builder->Add("oauthEnrollAbeSuccessDomain",
               IDS_ENTERPRISE_ENROLLMENT_SUCCESS_DOMAIN);

  /* Active Directory strings */
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

void EnrollmentScreenHandler::GetAdditionalParameters(
    base::DictionaryValue* parameters) {
  parameters->SetKey("encryptionTypesList", GetEncryptionTypesList());
}

bool EnrollmentScreenHandler::IsOnEnrollmentScreen() const {
  return (GetCurrentScreen() == kScreenId);
}

bool EnrollmentScreenHandler::IsEnrollmentScreenHiddenByError() const {
  return (GetCurrentScreen() == ErrorScreenView::kScreenId &&
          error_screen_->GetParentScreen() == kScreenId);
}

void EnrollmentScreenHandler::OnAdConfigurationUnlocked(
    std::string unlocked_data) {
  if (unlocked_data.empty()) {
    CallJS("login.OAuthEnrollmentScreen.setAdJoinParams",
           std::string() /* machineName */, std::string() /* userName */,
           static_cast<int>(ActiveDirectoryErrorState::BAD_UNLOCK_PASSWORD),
           true /* show_unlock_password */);
    return;
  }
  active_directory_domain_join_config_.clear();
  base::Optional<base::Value> options = base::JSONReader::Read(
      unlocked_data, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  if (!options || !options->is_list()) {
    ShowError(IDS_AD_JOIN_CONFIG_NOT_PARSED, true);
    CallJS("login.OAuthEnrollmentScreen.setAdJoinParams",
           std::string() /* machineName */, std::string() /* userName */,
           static_cast<int>(ActiveDirectoryErrorState::NONE),
           false /* show_unlock_password */);
    return;
  }
  base::DictionaryValue custom;
  custom.SetKey(
      "name",
      base::Value(l10n_util::GetStringUTF8(IDS_AD_CONFIG_SELECTION_CUSTOM)));
  options->Append(std::move(custom));
  active_directory_join_type_ =
      ActiveDirectoryDomainJoinType::USING_CONFIGURATION;
  CallJS("login.OAuthEnrollmentScreen.setAdJoinConfiguration", *options);
}

void EnrollmentScreenHandler::UpdateState(NetworkError::ErrorReason reason) {
  UpdateStateInternal(reason, false);
}

// TODO(rsorokin): This function is mostly copied from SigninScreenHandler and
// should be refactored in the future.
void EnrollmentScreenHandler::UpdateStateInternal(
    NetworkError::ErrorReason reason,
    bool force_update) {
  if (!force_update && !IsOnEnrollmentScreen() &&
      !IsEnrollmentScreenHiddenByError()) {
    return;
  }

  if (!force_update && !observe_network_failure_)
    return;

  NetworkStateInformer::State state = network_state_informer_->state();
  const std::string network_path = network_state_informer_->network_path();
  const bool is_online = (state == NetworkStateInformer::ONLINE);
  const bool is_behind_captive_portal =
      (state == NetworkStateInformer::CAPTIVE_PORTAL);
  const bool is_frame_error = reason == NetworkError::ERROR_REASON_FRAME_ERROR;

  LOG(WARNING) << "EnrollmentScreenHandler::UpdateState(): "
               << "state=" << NetworkStateInformer::StatusString(state) << ", "
               << "reason=" << NetworkError::ErrorReasonString(reason);

  if (is_online || !is_behind_captive_portal)
    error_screen_->HideCaptivePortal();

  if (is_frame_error) {
    LOG(WARNING) << "Retry page load";
    // TODO(rsorokin): Too many consecutive reloads.
    CallJS("login.OAuthEnrollmentScreen.doReload");
  }

  if (!is_online || is_frame_error)
    SetupAndShowOfflineMessage(state, reason);
  else
    HideOfflineMessage(state, reason);
}

void EnrollmentScreenHandler::SetupAndShowOfflineMessage(
    NetworkStateInformer::State state,
    NetworkError::ErrorReason reason) {
  const std::string network_path = network_state_informer_->network_path();
  const bool is_behind_captive_portal = IsBehindCaptivePortal(state, reason);
  const bool is_proxy_error = IsProxyError(state, reason);
  const bool is_frame_error = reason == NetworkError::ERROR_REASON_FRAME_ERROR;

  if (is_proxy_error) {
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_PROXY,
                                 std::string());
  } else if (is_behind_captive_portal) {
    // Do not bother a user with obsessive captive portal showing. This
    // check makes captive portal being shown only once: either when error
    // screen is shown for the first time or when switching from another
    // error screen (offline, proxy).
    if (IsOnEnrollmentScreen() ||
        (error_screen_->GetErrorState() != NetworkError::ERROR_STATE_PORTAL)) {
      error_screen_->FixCaptivePortal();
    }
    const std::string network_name = GetNetworkName(network_path);
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_PORTAL,
                                 network_name);
  } else if (is_frame_error) {
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT,
                                 std::string());
  } else {
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_OFFLINE,
                                 std::string());
  }

  if (GetCurrentScreen() != ErrorScreenView::kScreenId) {
    error_screen_->SetUIState(NetworkError::UI_STATE_SIGNIN);
    error_screen_->SetParentScreen(kScreenId);
    error_screen_->SetHideCallback(base::BindOnce(
        &EnrollmentScreenHandler::DoShow, weak_ptr_factory_.GetWeakPtr()));
    error_screen_->Show(nullptr);
    histogram_helper_->OnErrorShow(error_screen_->GetErrorState());
  }
}

void EnrollmentScreenHandler::HideOfflineMessage(
    NetworkStateInformer::State state,
    NetworkError::ErrorReason reason) {
  if (IsEnrollmentScreenHiddenByError())
    error_screen_->Hide();
  histogram_helper_->OnErrorHide();
}

// EnrollmentScreenHandler, private -----------------------------
void EnrollmentScreenHandler::HandleToggleFakeEnrollment() {
  VLOG(1) << "HandleToggleFakeEnrollment";
  policy::PolicyOAuth2TokenFetcher::UseFakeTokensForTesting();
  WizardController::SkipEnrollmentPromptsForTesting();
  use_fake_login_for_testing_ = true;
}

void EnrollmentScreenHandler::HandleClose(const std::string& reason) {
  DCHECK(controller_);
  if (active_directory_join_type_ != ActiveDirectoryDomainJoinType::COUNT) {
    DCHECK(g_browser_process->platform_part()
               ->browser_policy_connector_chromeos()
               ->IsActiveDirectoryManaged());
    // Record Active Directory join type in case of successful enrollment and
    // domain join.
    base::UmaHistogramEnumeration(kActiveDirectoryJoinHistogram,
                                  active_directory_join_type_,
                                  ActiveDirectoryDomainJoinType::COUNT);
  }

  if (reason == "cancel") {
    controller_->OnCancel();
  } else if (reason == "done") {
    controller_->OnConfirmationClosed();
  } else {
    NOTREACHED();
  }
}

void EnrollmentScreenHandler::HandleCompleteLogin(const std::string& user) {
  VLOG(1) << "HandleCompleteLogin";
  observe_network_failure_ = false;

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
                            weak_ptr_factory_.GetWeakPtr(), user),
        base::BindOnce(&EnrollmentScreenHandler::OnCookieWaitTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  ContinueAuthenticationWhenCookiesAvailable(user);
}

void EnrollmentScreenHandler::ContinueAuthenticationWhenCookiesAvailable(
    const std::string& user) {
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
      base::BindOnce(&EnrollmentScreenHandler::OnGetCookiesForCompleteLogin,
                     weak_ptr_factory_.GetWeakPtr(), user));
}

void EnrollmentScreenHandler::OnGetCookiesForCompleteLogin(
    const std::string& user,
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
    VLOG(1) << "OAuth cookie empty, still waiting";
    return;
  }

  oauth_code_waiter_.reset();
  DCHECK(controller_);
  controller_->OnLoginDone(gaia::SanitizeEmail(user), auth_code);
}

void EnrollmentScreenHandler::OnCookieWaitTimeout() {
  LOG(ERROR) << "Timeout waiting for OAuth cookie";
  oauth_code_waiter_.reset();

  // If enrollment ends and the browser is being restarted, the renderers are
  // killed so we can not talk to them anymore.
  if (!shutdown_)
    ShowError(IDS_LOGIN_FATAL_ERROR_NO_AUTH_TOKEN, true);
}

void EnrollmentScreenHandler::HandleAdCompleteLogin(
    const std::string& machine_name,
    const std::string& distinguished_name,
    const std::string& encryption_types,
    const std::string& user_name,
    const std::string& password) {
  observe_network_failure_ = false;
  DCHECK(controller_);
  controller_->OnActiveDirectoryCredsProvided(
      machine_name, distinguished_name,
      TranslateEncryptionTypesString(encryption_types), user_name, password);
}

void EnrollmentScreenHandler::HandleAdUnlockConfiguration(
    const std::string& password) {
  AuthPolicyHelper::DecryptConfiguration(
      active_directory_domain_join_config_, password,
      base::BindOnce(&EnrollmentScreenHandler::OnAdConfigurationUnlocked,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentScreenHandler::HandleRetry() {
  DCHECK(controller_);
  controller_->OnRetry();
}

void EnrollmentScreenHandler::HandleFrameLoadingCompleted() {
  if (network_state_informer_->state() != NetworkStateInformer::ONLINE)
    return;

  UpdateState(NetworkError::ERROR_REASON_UPDATE);
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

void EnrollmentScreenHandler::ShowStep(const char* step) {
  CallJS("login.OAuthEnrollmentScreen.showStep", std::string(step));
}

void EnrollmentScreenHandler::ShowError(int message_id, bool retry) {
  ShowErrorMessage(l10n_util::GetStringUTF8(message_id), retry);
}

void EnrollmentScreenHandler::ShowErrorForDevice(int message_id, bool retry) {
  ShowErrorMessage(
      l10n_util::GetStringFUTF8(message_id, ui::GetChromeOSDeviceName()),
      retry);
}

void EnrollmentScreenHandler::ShowErrorMessage(const std::string& message,
                                               bool retry) {
  CallJS("login.OAuthEnrollmentScreen.showError", message, retry);
}

void EnrollmentScreenHandler::DoShow() {
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

  base::DictionaryValue screen_data;

  screen_data.SetString("webviewPartitionName", partition_name);
  signin_partition_name_ = partition_name;

  screen_data.SetString("gaiaUrl", GaiaUrls::GetInstance()->gaia_url().spec());
  screen_data.SetString("clientId",
                        GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  screen_data.SetString("enrollment_mode",
                        EnrollmentModeToUIMode(config_.mode));
  screen_data.SetBoolean("is_enrollment_enforced", config_.is_forced());
  screen_data.SetBoolean("attestationBased", config_.is_mode_attestation());
  screen_data.SetString("management_domain", config_.management_domain);

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  if (!app_locale.empty())
    screen_data.SetString("hl", app_locale);

  const bool cfm = policy::EnrollmentRequisitionManager::IsRemoraRequisition();
  screen_data.SetString("flow", cfm ? "cfm" : "enterprise");

  ShowScreenWithData(EnrollmentScreenView::kScreenId, &screen_data);
  if (first_show_) {
    first_show_ = false;
    UpdateStateInternal(NetworkError::ERROR_REASON_UPDATE, true);
  }
  histogram_helper_->OnScreenShow();
}

}  // namespace chromeos
