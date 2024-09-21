// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_display_host_common.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_launch_state.h"
#include "chrome/browser/ash/attestation/attestation_ca_client.h"
#include "chrome/browser/ash/auth/cryptohome_pin_engine.h"
#include "chrome/browser/ash/login/choobe_flow_controller.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/lock_screen_utils.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/oobe_cros_events_metrics.h"
#include "chrome/browser/ash/login/oobe_metrics_helper.h"
#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/screens/app_launch_splash_screen.h"
#include "chrome/browser/ash/login/screens/encryption_migration_screen.h"
#include "chrome/browser/ash/login/screens/gaia_screen.h"
#include "chrome/browser/ash/login/screens/osauth/recovery_eligibility_screen.h"
#include "chrome/browser/ash/login/screens/pin_setup_screen.h"
#include "chrome/browser/ash/login/screens/reset_screen.h"
#include "chrome/browser/ash/login/screens/saml_confirm_password_screen.h"
#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/nearby/quick_start_connectivity_service_factory.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/profiles/signin_profile_handler.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/ui/ash/login/login_feedback.h"
#include "chrome/browser/ui/ash/login/signin_ui.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/ash/diagnostics_dialog/diagnostics_dialog.h"
#include "chrome/browser/ui/webui/ash/login/family_link_notice_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/locale_switch_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/management_transition_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/offline_login_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/saml_confirm_password_handler.h"
#include "chrome/browser/ui/webui/ash/login/signin_fatal_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_allowlist_check_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/attestation/attestation_flow_adaptive.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/language_preferences/language_preferences.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/common/features/feature_session_type.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// The delay of triggering initialization of the device policy subsystem
// after the login screen is initialized. This makes sure that device policy
// network requests are made while the system is idle waiting for user input.
constexpr int64_t kPolicyServiceInitializationDelayMilliseconds = 100;

void PushFrontImIfNotExists(const std::string& input_method_id,
                            std::vector<std::string>* input_method_ids) {
  if (input_method_id.empty()) {
    return;
  }

  if (!base::Contains(*input_method_ids, input_method_id)) {
    input_method_ids->insert(input_method_ids->begin(), input_method_id);
  }
}

void SetGaiaInputMethods(const AccountId& account_id) {
  input_method::InputMethodManager* imm =
      input_method::InputMethodManager::Get();

  scoped_refptr<input_method::InputMethodManager::State> gaia_ime_state =
      imm->GetActiveIMEState()->Clone();
  imm->SetState(gaia_ime_state);
  gaia_ime_state->SetUIStyle(input_method::InputMethodManager::UIStyle::kLogin);

  // Set Least Recently Used input method for the user.
  if (account_id.is_valid()) {
    lock_screen_utils::SetUserInputMethod(account_id, gaia_ime_state.get(),
                                          true /*honor_device_policy*/);
  } else {
    lock_screen_utils::EnforceDevicePolicyInputMethods(std::string());
    std::vector<std::string> input_method_ids;
    if (gaia_ime_state->GetAllowedInputMethodIds().empty()) {
      input_method_ids =
          imm->GetInputMethodUtil()->GetHardwareLoginInputMethodIds();
    } else {
      input_method_ids = gaia_ime_state->GetAllowedInputMethodIds();
    }
    const std::string owner_input_method_id =
        lock_screen_utils::GetUserLastInputMethodId(
            user_manager::UserManager::Get()->GetOwnerAccountId());
    const std::string system_input_method_id =
        g_browser_process->local_state()->GetString(
            language_prefs::kPreferredKeyboardLayout);

    PushFrontImIfNotExists(owner_input_method_id, &input_method_ids);
    PushFrontImIfNotExists(system_input_method_id, &input_method_ids);

    gaia_ime_state->EnableLoginLayouts(
        g_browser_process->GetApplicationLocale(), input_method_ids);

    if (!system_input_method_id.empty()) {
      gaia_ime_state->ChangeInputMethod(system_input_method_id,
                                        false /* show_message */);
    } else if (!owner_input_method_id.empty()) {
      gaia_ime_state->ChangeInputMethod(owner_input_method_id,
                                        false /* show_message */);
    }
  }
}

int ErrorToMessageId(SigninError error) {
  switch (error) {
    case SigninError::kCaptivePortalError:
      NOTREACHED_IN_MIGRATION();
      return 0;
    case SigninError::kGoogleAccountNotAllowed:
      return IDS_LOGIN_ERROR_GOOGLE_ACCOUNT_NOT_ALLOWED;
    case SigninError::kOwnerRequired:
      return IDS_LOGIN_ERROR_OWNER_REQUIRED;
    case SigninError::kTpmUpdateRequired:
      return IDS_LOGIN_ERROR_TPM_UPDATE_REQUIRED;
    case SigninError::kKnownUserFailedNetworkNotConnected:
      return IDS_LOGIN_ERROR_AUTHENTICATING;
    case SigninError::kNewUserFailedNetworkNotConnected:
      return IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED;
    case SigninError::kNewUserFailedNetworkConnected:
      return IDS_LOGIN_ERROR_AUTHENTICATING_NEW;
    case SigninError::kKnownUserFailedNetworkConnected:
      return IDS_LOGIN_ERROR_AUTHENTICATING;
    case SigninError::kOwnerKeyLost:
      return IDS_LOGIN_ERROR_OWNER_KEY_LOST;
    case SigninError::kChallengeResponseAuthMultipleClientCerts:
      return IDS_CHALLENGE_RESPONSE_AUTH_MULTIPLE_CLIENT_CERTS_ERROR;
    case SigninError::kChallengeResponseAuthInvalidClientCert:
      return IDS_CHALLENGE_RESPONSE_AUTH_INVALID_CLIENT_CERT_ERROR;
    case SigninError::kCookieWaitTimeout:
      return IDS_LOGIN_FATAL_ERROR_NO_AUTH_TOKEN;
    case SigninError::kFailedToFetchSamlRedirect:
      return IDS_FAILED_TO_FETCH_SAML_REDIRECT;
  }
}

bool IsAuthError(SigninError error) {
  return error == SigninError::kCaptivePortalError ||
         error == SigninError::kKnownUserFailedNetworkNotConnected ||
         error == SigninError::kNewUserFailedNetworkNotConnected ||
         error == SigninError::kNewUserFailedNetworkConnected ||
         error == SigninError::kKnownUserFailedNetworkConnected;
}

class AccessibilityManagerWrapper
    : public quick_start::TargetDeviceBootstrapController::
          AccessibilityManagerWrapper {
 public:
  AccessibilityManagerWrapper() = default;
  AccessibilityManagerWrapper(AccessibilityManagerWrapper&) = delete;
  AccessibilityManagerWrapper& operator=(AccessibilityManagerWrapper&) = delete;
  ~AccessibilityManagerWrapper() override = default;

  bool AllowQRCodeUX() const override {
    return ash::AccessibilityManager::Get()->AllowQRCodeUX();
  }
};

std::unique_ptr<quick_start::SecondDeviceAuthBroker>
CreateSecondDeviceAuthBroker() {
  std::unique_ptr<attestation::ServerProxy> server_proxy(
      new attestation::AttestationCAClient());
  std::unique_ptr<attestation::AttestationFlow> attestation_flow =
      std::make_unique<attestation::AttestationFlowAdaptive>(
          std::move(server_proxy));

  // TODO(b:286850431) - Fix device id generation.
  const std::string device_id =
      GenerateSigninScopedDeviceId(/*for_ephemeral=*/false);
  auto* signin_profile = ProfileHelper::GetSigninProfile();
  return std::make_unique<quick_start::SecondDeviceAuthBroker>(
      device_id, signin_profile->GetURLLoaderFactory(),
      std::move(attestation_flow));
}

}  // namespace

LoginDisplayHostCommon::LoginDisplayHostCommon()
    : keep_alive_(KeepAliveOrigin::LOGIN_DISPLAY_HOST_WEBUI,
                  KeepAliveRestartOption::DISABLED),
      login_ui_pref_controller_(std::make_unique<LoginUIPrefController>()),
      wizard_context_(std::make_unique<WizardContext>()),
      oobe_metrics_helper_(std::make_unique<OobeMetricsHelper>()) {
  if (features::IsOobeCrosEventsEnabled()) {
    oobe_cros_events_metrics_ =
        std::make_unique<OobeCrosEventsMetrics>(oobe_metrics_helper_.get());
  }
  // Close the login screen on app termination (for the case where shutdown
  // occurs before login completes).
  app_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
          &LoginDisplayHostCommon::OnAppTerminating, base::Unretained(this)));
  BrowserList::AddObserver(this);
}

LoginDisplayHostCommon::~LoginDisplayHostCommon() = default;

void LoginDisplayHostCommon::BeforeSessionStart() {
  session_starting_ = true;
}

bool LoginDisplayHostCommon::LoginDisplayHostCommon::IsFinalizing() {
  return is_finalizing_;
}

void LoginDisplayHostCommon::Finalize(base::OnceClosure completion_callback) {
  LOG(WARNING) << "Finalize";
  LoginDisplayHost::Finalize(std::move(completion_callback));

  // If finalize is called twice the LoginDisplayHost instance will be deleted
  // multiple times.
  CHECK(!is_finalizing_);
  is_finalizing_ = true;

  OnFinalize();
}

void LoginDisplayHostCommon::FinalizeImmediately() {
  CHECK(!is_finalizing_);
  CHECK(!shutting_down_);
  is_finalizing_ = true;
  shutting_down_ = true;
  OnFinalize();
  Cleanup();
  delete this;
}

void LoginDisplayHostCommon::StartUserAdding(
    base::OnceClosure completion_callback) {
  LoginDisplayHost::StartUserAdding(std::move(completion_callback));
  OnStartUserAdding();
}

void LoginDisplayHostCommon::StartSignInScreen() {
  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetUsers();

  // Fix for users who updated device and thus never passed register screen.
  // If we already have users, we assume that it is not a second part of
  // OOBE. See http://crosbug.com/6289
  if (!StartupUtils::IsDeviceRegistered() && !users.empty()) {
    VLOG(1) << "Mark device registered because there are remembered users: "
            << users.size();
    StartupUtils::MarkDeviceRegistered(base::OnceClosure());
  }

  // Initiate device policy fetching.
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  connector->ScheduleServiceInitialization(
      kPolicyServiceInitializationDelayMilliseconds);

  // Run UI-specific logic.
  OnStartSignInScreen();

  // Enable status area after starting sign-in screen, as it may depend on the
  // UI being visible.
  SystemTrayClientImpl::Get()->SetPrimaryTrayVisible(/*visible=*/true);
}

void LoginDisplayHostCommon::StartKiosk(const KioskAppId& kiosk_app_id,
                                        bool is_auto_launch) {
  VLOG(1) << "Login >> start kiosk of type "
          << static_cast<int>(kiosk_app_id.type);

  SetKioskLaunchStateCrashKey(KioskLaunchState::kAttemptToLaunch);

  SystemTrayClientImpl::Get()->SetPrimaryTrayVisible(/*visible=*/false);

  // Wait for the `CrosSettings` to become either trusted or permanently
  // untrusted.
  const CrosSettingsProvider::TrustedStatus status =
      CrosSettings::Get()->PrepareTrustedValues(base::BindOnce(
          &LoginDisplayHostCommon::StartKiosk, weak_factory_.GetWeakPtr(),
          kiosk_app_id, is_auto_launch));
  if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED) {
    return;
  }

  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    // If the `CrosSettings` are permanently untrusted, refuse to launch a
    // single-app kiosk mode session.
    LOG(ERROR) << "Login >> Refusing to launch single-app kiosk mode.";
    SystemTrayClientImpl::Get()->SetPrimaryTrayVisible(/*visible=*/true);
    return;
  }

  if (system::DeviceDisablingManager::IsDeviceDisabledDuringNormalOperation()) {
    // If the device is disabled, bail out. A device disabled screen will be
    // shown by the DeviceDisablingManager.
    return;
  }

  // Prevent a race condition when user launches a kiosk app from the apps
  // menu while another login is in progress. E.g. UI shelf is not disabled on
  // slower devices.
  // A race can happen between manual launch kiosk and one of guest session,
  // MGS (manual or autolaunched) or autolaunched kiosk.
  // Currently needs to use both ExistingUserController and UserManager because
  // these sessions aren't consistent with setting various login states in time.
  // TODO(b/291293540): Check why ExistingUserController is not updated by
  // autolaunch kiosk.
  const auto& existing_user_controller =
      CHECK_DEREF(GetExistingUserController());
  const bool is_login_detected_existing_user_controller =
      existing_user_controller.IsSigninInProgress() ||
      existing_user_controller.IsUserSigninCompleted();
  const bool is_login_detected_user_manager =
      user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->IsUserLoggedIn();
  if (is_login_detected_existing_user_controller ||
      is_login_detected_user_manager) {
    LOG(ERROR) << "Cancel kiosk launch. Another user login is completed or in "
                  "progress.";
    return;
  }

  SetKioskLaunchStateCrashKey(KioskLaunchState::kStartLaunch);

  OnStartAppLaunch();

  int auto_launch_delay = -1;
  if (is_auto_launch) {
    if (!CrosSettings::Get()->GetInteger(
            kAccountsPrefDeviceLocalAccountAutoLoginDelay,
            &auto_launch_delay)) {
      auto_launch_delay = 0;
    }
    DCHECK_EQ(0, auto_launch_delay)
        << "Kiosks do not support non-zero auto-login delays";
  }

  extensions::SetCurrentFeatureSessionType(
      is_auto_launch && auto_launch_delay == 0
          ? extensions::mojom::FeatureSessionType::kAutolaunchedKiosk
          : extensions::mojom::FeatureSessionType::kKiosk);

  KioskController::Get().StartSession(
      kiosk_app_id, is_auto_launch, this,
      GetWizardController()->GetScreen<AppLaunchSplashScreen>());
}

void LoginDisplayHostCommon::CompleteLogin(const UserContext& user_context) {
  if (GetExistingUserController()) {
    GetExistingUserController()->CompleteLogin(user_context);
  } else {
    LOG(WARNING) << "LoginDisplayHostCommon::CompleteLogin - Failure : "
                 << "ExistingUserController not available.";
  }
}

void LoginDisplayHostCommon::OnGaiaScreenReady() {
  if (GetExistingUserController()) {
    GetExistingUserController()->OnGaiaScreenReady();
  } else {
    // Used to debug crbug.com/902315. Feel free to remove after that is fixed.
    LOG(ERROR) << "OnGaiaScreenReady: there is no existing user controller";
  }
}

void LoginDisplayHostCommon::SetDisplayEmail(const std::string& email) {
  if (GetExistingUserController()) {
    GetExistingUserController()->SetDisplayEmail(email);
  }
}

void LoginDisplayHostCommon::ShowAllowlistCheckFailedError() {
  StartWizard(UserAllowlistCheckScreenView::kScreenId);
}

void LoginDisplayHostCommon::UpdateWallpaper(
    const AccountId& prefilled_account) {
  auto* wallpaper_controller = ash::WallpaperController::Get();
  if (prefilled_account.is_valid()) {
    wallpaper_controller->ShowUserWallpaper(prefilled_account);
    return;
  }
  wallpaper_controller->ShowSigninWallpaper();
}

bool LoginDisplayHostCommon::IsUserAllowlisted(
    const AccountId& account_id,
    const std::optional<user_manager::UserType>& user_type) {
  if (!GetExistingUserController()) {
    return true;
  }
  return GetExistingUserController()->IsUserAllowlisted(account_id, user_type);
}

void LoginDisplayHostCommon::CancelPasswordChangedFlow() {
  if (GetExistingUserController()) {
    GetExistingUserController()->CancelPasswordChangedFlow();
  }

  OnCancelPasswordChangedFlow();
}

bool LoginDisplayHostCommon::HandleAccelerator(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kShowFeedback) {
    login_feedback_ = std::make_unique<LoginFeedback>(
        ProfileHelper::Get()->GetSigninProfile());
    login_feedback_->Request(std::string());
    return true;
  }

  if (action == LoginAcceleratorAction::kLaunchDiagnostics) {
    // Don't handle this action if device is disabled.
    if (system::DeviceDisablingManager::
            IsDeviceDisabledDuringNormalOperation()) {
      return false;
    }
    DiagnosticsDialog::ShowDialog();
    return true;
  }

  if (KioskController::Get().HandleAccelerator(action)) {
    return true;
  }

  // This path should only handle screen-specific accelerators, so we do not
  // need to create WebUI here.
  if (IsWizardControllerCreated() &&
      GetWizardController()->HandleAccelerator(action)) {
    return true;
  }

  // We check the reset screen accelerator after checking with the
  // WizardController because this accelerator is also handled by the
  // reset screen itself - triggering the rollback option. In such case we
  // return in the previous statement.
  if (action == LoginAcceleratorAction::kShowResetScreen) {
    ResetScreen::CheckIfPowerwashAllowed(
        base::BindOnce(&LoginDisplayHostCommon::OnPowerwashAllowedCallback,
                       weak_factory_.GetWeakPtr()));
    return true;
  }

  if (action == LoginAcceleratorAction::kCancelScreenAction) {
    if (!GetOobeUI() || !GetLoginWindowWidget() ||
        !GetLoginWindowWidget()->IsVisible()) {
      return false;
    }
    GetOobeUI()->GetCoreOobe()->ForwardCancel();
    return true;
  }

  return false;
}

void LoginDisplayHostCommon::SetScreenAfterManagedTos(OobeScreenId screen_id) {
  // If user stopped onboarding flow on TermsOfServiceScreen make sure that
  // next screen will be FamilyLinkNoticeView::kScreenId.
  if (screen_id == TermsOfServiceScreenView::kScreenId) {
    screen_id = FamilyLinkNoticeView::kScreenId;
  }
  wizard_context_->screen_after_managed_tos = screen_id;
}

void LoginDisplayHostCommon::OnPowerwashAllowedCallback(
    bool is_reset_allowed,
    std::optional<tpm_firmware_update::Mode> tpm_firmware_update_mode) {
  if (!is_reset_allowed) {
    return;
  }
  if (tpm_firmware_update_mode.has_value()) {
    // Force the TPM firmware update option to be enabled.
    g_browser_process->local_state()->SetInteger(
        ::prefs::kFactoryResetTPMFirmwareUpdateMode,
        static_cast<int>(tpm_firmware_update_mode.value()));
  }
  StartWizard(ResetView::kScreenId);
}

void LoginDisplayHostCommon::StartUserOnboarding() {
  oobe_metrics_helper_->RecordOnboardingStart(
      g_browser_process->local_state()->GetTime(prefs::kOobeStartTime));
  StartWizard(LocaleSwitchView::kScreenId);
}

void LoginDisplayHostCommon::ResumeUserOnboarding(const PrefService& prefs,
                                                  OobeScreenId screen_id) {
  oobe_metrics_helper_->RecordOnboardingResume(screen_id);
  SetScreenAfterManagedTos(screen_id);

  if (features::IsOobeChoobeEnabled()) {
    if (ChoobeFlowController::ShouldResumeChoobe(prefs)) {
      oobe_metrics_helper_->RecordChoobeResume();
      GetWizardController()->CreateChoobeFlowController();
      GetWizardController()->choobe_flow_controller()->ResumeChoobe(prefs);
    }
  }

  // Try to show TermsOfServiceScreen first
  StartWizard(TermsOfServiceScreenView::kScreenId);
}

void LoginDisplayHostCommon::StartManagementTransition() {
  StartWizard(ManagementTransitionScreenView::kScreenId);
}

void LoginDisplayHostCommon::ShowTosForExistingUser() {
  SetScreenAfterManagedTos(ash::OOBE_SCREEN_UNKNOWN);
  StartUserOnboarding();
}

void LoginDisplayHostCommon::ShowNewTermsForFlexUsers() {
  SetScreenAfterManagedTos(ConsolidatedConsentScreenView::kScreenId);

  wizard_context_->is_cloud_ready_update_flow = true;
  StartWizard(TermsOfServiceScreenView::kScreenId);
}

void LoginDisplayHostCommon::SetAuthSessionForOnboarding(
    const UserContext& user_context) {
  wizard_context_->extra_factors_token = AuthSessionStorage::Get()->Store(
      std::make_unique<UserContext>(user_context));
}

void LoginDisplayHostCommon::ClearOnboardingAuthSession() {
  if (wizard_context_->extra_factors_token.has_value()) {
    AuthSessionStorage::Get()->Invalidate(
        wizard_context_->extra_factors_token.value(), base::DoNothing());
    wizard_context_->extra_factors_token = std::nullopt;
  }
}

void LoginDisplayHostCommon::StartEncryptionMigration(
    std::unique_ptr<UserContext> user_context,
    EncryptionMigrationMode migration_mode,
    base::OnceCallback<void(std::unique_ptr<UserContext>)> on_skip_migration) {
  StartWizard(EncryptionMigrationScreenView::kScreenId);

  EncryptionMigrationScreen* migration_screen =
      GetWizardController()->GetScreen<EncryptionMigrationScreen>();

  DCHECK(migration_screen);
  migration_screen->SetUserContext(std::move(user_context));
  migration_screen->SetMode(migration_mode);
  migration_screen->SetSkipMigrationCallback(std::move(on_skip_migration));
  migration_screen->SetupInitialView();
}

void LoginDisplayHostCommon::ShowSigninError(SigninError error,
                                             const std::string& details) {
  VLOG(1) << "Show error, error_id: " << static_cast<int>(error);

  if (error == SigninError::kKnownUserFailedNetworkNotConnected ||
      error == SigninError::kKnownUserFailedNetworkConnected) {
    if (!IsOobeUIDialogVisible()) {
      // Handled by Views UI.
      return;
    }
    OfflineLoginScreen* offline_login_screen =
        GetWizardController()->GetScreen<OfflineLoginScreen>();
    if (GetWizardController()->current_screen() == offline_login_screen) {
      offline_login_screen->ShowPasswordMismatchMessage();
      return;
    }
  }

  std::string error_text;
  switch (error) {
    case SigninError::kCaptivePortalError:
      error_text = l10n_util::GetStringFUTF8(
          IDS_LOGIN_ERROR_CAPTIVE_PORTAL,
          GetExistingUserController()->GetConnectedNetworkName());
      break;
    default:
      error_text = l10n_util::GetStringUTF8(ErrorToMessageId(error));
      break;
  }

  std::string keyboard_hint;

  // Only display hints about keyboard layout if the error is authentication-
  // related.
  if (IsAuthError(error)) {
    input_method::InputMethodManager* ime_manager =
        input_method::InputMethodManager::Get();
    // Display a hint to switch keyboards if there are other enabled input
    // methods.
    if (ime_manager->GetActiveIMEState()->GetNumEnabledInputMethods() > 1) {
      keyboard_hint =
          l10n_util::GetStringUTF8(IDS_LOGIN_ERROR_KEYBOARD_SWITCH_HINT);
    }
  }

  std::string help_link_text = l10n_util::GetStringUTF8(IDS_LEARN_MORE);

  GetWizardController()->GetScreen<SignInFatalErrorScreen>()->SetCustomError(
      error_text, keyboard_hint, details, help_link_text);
  StartWizard(SignInFatalErrorView::kScreenId);
}

void LoginDisplayHostCommon::SAMLConfirmPassword(
    ::login::StringList scraped_passwords,
    std::unique_ptr<UserContext> user_context) {
  GetWizardController()
      ->GetScreen<SamlConfirmPasswordScreen>()
      ->SetContextAndPasswords(std::move(user_context),
                               std::move(scraped_passwords));
  StartWizard(SamlConfirmPasswordView::kScreenId);
}

WizardContext* LoginDisplayHostCommon::GetWizardContextForTesting() {
  return GetWizardContext();
}

void LoginDisplayHostCommon::OnBrowserAdded(Browser* browser) {
  VLOG(4) << "OnBrowserAdded " << session_starting_;
  // Browsers created before session start (windows opened by extensions, for
  // example) are ignored.
  if (session_starting_) {
    // OnBrowserAdded is called when the browser is created, but not shown yet.
    // Lock window has to be closed at this point so that a browser window
    // exists and the window can acquire input focus.
    OnBrowserCreated();
    app_terminating_subscription_ = {};
    BrowserList::RemoveObserver(this);
  }
}

WizardContext* LoginDisplayHostCommon::GetWizardContext() {
  return wizard_context_.get();
}

OobeMetricsHelper* LoginDisplayHostCommon::GetOobeMetricsHelper() {
  return oobe_metrics_helper_.get();
}

void LoginDisplayHostCommon::OnCancelPasswordChangedFlow() {
  LoginDisplayHost::default_host()->StartSignInScreen();
}

void LoginDisplayHostCommon::ShutdownDisplayHost() {
  LOG(WARNING) << "ShutdownDisplayHost";
  if (shutting_down_) {
    return;
  }
  shutting_down_ = true;

  Cleanup();
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void LoginDisplayHostCommon::OnStartSignInScreenCommon() {
  kiosk_app_menu_controller_.ConfigureKioskCallbacks();
  kiosk_app_menu_controller_.SendKioskApps();
}

void LoginDisplayHostCommon::ShowGaiaDialogCommon(
    const AccountId& prefilled_account) {
  if (GetExistingUserController()->IsSigninInProgress()) {
    return;
  }
  SetGaiaInputMethods(prefilled_account);

  if (!prefilled_account.is_valid()) {
    StartWizard(UserCreationView::kScreenId);
  } else {
    wizard_context_->gaia_config.prefilled_account = prefilled_account;
    StartWizard(GaiaView::kScreenId);
  }
}

void LoginDisplayHostCommon::AddWizardCreatedObserverForTests(
    base::RepeatingClosure on_created) {
  DCHECK(!on_wizard_controller_created_for_tests_);
  on_wizard_controller_created_for_tests_ = std::move(on_created);
}

base::WeakPtr<quick_start::TargetDeviceBootstrapController>
LoginDisplayHostCommon::GetQuickStartBootstrapController() {
  CHECK(wizard_context_->quick_start_enabled);
  if (!bootstrap_controller_) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    CHECK(profile);

    quick_start::QuickStartConnectivityService* service =
        quick_start::QuickStartConnectivityServiceFactory::GetForProfile(
            profile);
    CHECK(service);

    bootstrap_controller_ =
        std::make_unique<ash::quick_start::TargetDeviceBootstrapController>(
            CreateSecondDeviceAuthBroker(),
            std::make_unique<AccessibilityManagerWrapper>(), service);
  }
  return bootstrap_controller_->GetAsWeakPtrForClient();
}

void LoginDisplayHostCommon::NotifyWizardCreated() {
  if (on_wizard_controller_created_for_tests_) {
    on_wizard_controller_created_for_tests_.Run();
  }
}

void LoginDisplayHostCommon::Cleanup() {
  if (wizard_context_->quick_start_enabled) {
    bootstrap_controller_.reset();
  }

  SigninProfileHandler::Get()->ClearSigninProfile(base::DoNothing());
  app_terminating_subscription_ = {};
  BrowserList::RemoveObserver(this);
  login_ui_pref_controller_.reset();

  // Cancel kiosk session start since kiosk holds a pointer to `this` during
  // the start procedure.
  KioskController::Get().CancelSessionStart();
}

void LoginDisplayHostCommon::OnAppTerminating() {
  ShutdownDisplayHost();
}

}  // namespace ash
