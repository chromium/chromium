// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"

#include <stddef.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "ash/public/cpp/login_constants.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "ash/public/mojom/tray_action.mojom.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/chromeos/login/error_screens_histogram_helper.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/hwid_checker.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/lock_screen_utils.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_backend.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/screens/gaia_screen.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/chromeos/login/ui/login_display_webui.h"
#include "chrome/browser/chromeos/login/ui/login_feedback.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/system_clock.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/ash/ime_controller_client.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/session_manager/core/session_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/feedback_private/feedback_private_delegate.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"

namespace {

// Max number of users to show.
const size_t kMaxUsers = 18;

// Timeout to delay first notification about offline state for a
// current network.
constexpr base::TimeDelta kOfflineTimeout = base::TimeDelta::FromSeconds(1);

// Timeout to delay first notification about offline state when authenticating
// to a proxy.
constexpr base::TimeDelta kProxyAuthTimeout = base::TimeDelta::FromSeconds(5);

// Timeout used to prevent infinite connecting to a flaky network.
constexpr base::TimeDelta kConnectingTimeout = base::TimeDelta::FromSeconds(60);

// Max number of Gaia Reload to Show Proxy Auth Dialog.
const int kMaxGaiaReloadForProxyAuthDialog = 3;

// Type of the login screen UI that is currently presented to user.
const char kSourceGaiaSignin[] = "gaia-signin";
const char kSourceAccountPicker[] = "account-picker";

class CallOnReturn {
 public:
  explicit CallOnReturn(const base::Closure& callback)
      : callback_(callback), call_scheduled_(false) {}

  ~CallOnReturn() {
    if (call_scheduled_ && !callback_.is_null())
      callback_.Run();
  }

  void CancelScheduledCall() { call_scheduled_ = false; }
  void ScheduleCall() { call_scheduled_ = true; }

 private:
  base::Closure callback_;
  bool call_scheduled_;

  DISALLOW_COPY_AND_ASSIGN(CallOnReturn);
};

}  // namespace

namespace chromeos {

namespace {

bool IsOnline(NetworkStateInformer::State state,
              NetworkError::ErrorReason reason) {
  return state == NetworkStateInformer::ONLINE &&
         reason != NetworkError::ERROR_REASON_PORTAL_DETECTED &&
         reason != NetworkError::ERROR_REASON_LOADING_TIMEOUT;
}

bool IsBehindCaptivePortal(NetworkStateInformer::State state,
                           NetworkError::ErrorReason reason) {
  return state == NetworkStateInformer::CAPTIVE_PORTAL ||
         reason == NetworkError::ERROR_REASON_PORTAL_DETECTED;
}

bool IsProxyError(NetworkStateInformer::State state,
                  NetworkError::ErrorReason reason,
                  net::Error frame_error) {
  return state == NetworkStateInformer::PROXY_AUTH_REQUIRED ||
         reason == NetworkError::ERROR_REASON_PROXY_AUTH_CANCELLED ||
         reason == NetworkError::ERROR_REASON_PROXY_CONNECTION_FAILED ||
         (reason == NetworkError::ERROR_REASON_FRAME_ERROR &&
          (frame_error == net::ERR_PROXY_CONNECTION_FAILED ||
           frame_error == net::ERR_TUNNEL_CONNECTION_FAILED));
}

bool IsSigninScreen(const OobeScreenId screen) {
  return screen == GaiaView::kScreenId ||
         screen == OobeScreen::SCREEN_ACCOUNT_PICKER;
}

bool IsSigninScreenError(NetworkError::ErrorState error_state) {
  return error_state == NetworkError::ERROR_STATE_PORTAL ||
         error_state == NetworkError::ERROR_STATE_OFFLINE ||
         error_state == NetworkError::ERROR_STATE_PROXY ||
         error_state == NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT;
}

// Returns network name by service path.
std::string GetNetworkName(const std::string& service_path) {
  const NetworkState* network = NetworkHandler::Get()->network_state_handler()->
      GetNetworkState(service_path);
  if (!network)
    return std::string();
  return network->name();
}

}  // namespace

// SigninScreenHandler implementation ------------------------------------------

SigninScreenHandler::SigninScreenHandler(
    JSCallsContainer* js_calls_container,
    const scoped_refptr<NetworkStateInformer>& network_state_informer,
    ErrorScreen* error_screen,
    CoreOobeView* core_oobe_view,
    GaiaScreenHandler* gaia_screen_handler)
    : BaseWebUIHandler(js_calls_container),
      network_state_informer_(network_state_informer),
      error_screen_(error_screen),
      core_oobe_view_(core_oobe_view),
      proxy_auth_dialog_reload_times_(kMaxGaiaReloadForProxyAuthDialog),
      gaia_screen_handler_(gaia_screen_handler),
      histogram_helper_(new ErrorScreensHistogramHelper("Signin")) {
  DCHECK(network_state_informer_.get());
  DCHECK(error_screen_);
  DCHECK(core_oobe_view_);
  DCHECK(js_calls_container);
  gaia_screen_handler_->set_signin_screen_handler(this);
  network_state_informer_->AddObserver(this);

  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_NEEDED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_CANCELLED,
                 content::NotificationService::AllSources());

  chromeos::PowerManagerClient::Get()->AddObserver(this);

  chromeos::input_method::ImeKeyboard* keyboard =
      chromeos::input_method::InputMethodManager::Get()->GetImeKeyboard();
  if (keyboard)
    keyboard->AddObserver(this);

  ash::TabletMode* tablet_mode = ash::TabletMode::Get();
  tablet_mode->AddObserver(this);
  OnTabletModeToggled(tablet_mode->InTabletMode());

  WallpaperControllerClient::Get()->AddObserver(this);
}

SigninScreenHandler::~SigninScreenHandler() {
  if (auto* wallpaper_controller_client = WallpaperControllerClient::Get())
    wallpaper_controller_client->RemoveObserver(this);
  // Ash maybe released before us.
  if (ash::TabletMode::Get())
    ash::TabletMode::Get()->RemoveObserver(this);
  OobeUI* oobe_ui = GetOobeUI();
  if (oobe_ui && oobe_ui_observer_added_)
    oobe_ui->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  chromeos::input_method::ImeKeyboard* keyboard =
      chromeos::input_method::InputMethodManager::Get()->GetImeKeyboard();
  if (keyboard)
    keyboard->RemoveObserver(this);
  if (ImeControllerClient::Get())  // Can be null in tests.
    ImeControllerClient::Get()->SetImesManagedByPolicy(false);
  weak_factory_.InvalidateWeakPtrs();
  if (delegate_)
    delegate_->SetWebUIHandler(nullptr);
  network_state_informer_->RemoveObserver(this);
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(nullptr);
  proximity_auth::ScreenlockBridge::Get()->SetFocusedUser(EmptyAccountId());
}

void SigninScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  // Format numbers to be used on the pin keyboard.
  for (int j = 0; j <= 9; j++) {
    builder->Add("pinKeyboard" + base::NumberToString(j),
                 base::FormatNumber(int64_t{j}));
  }

  builder->Add("passwordHint", IDS_LOGIN_POD_EMPTY_PASSWORD_TEXT);
  builder->Add("pinKeyboardPlaceholderPin",
               IDS_PIN_KEYBOARD_HINT_TEXT_PIN);
  builder->Add("pinKeyboardPlaceholderPinPassword",
               IDS_PIN_KEYBOARD_HINT_TEXT_PIN_PASSWORD);
  builder->Add("pinKeyboardDeleteAccessibleName",
               IDS_PIN_KEYBOARD_DELETE_ACCESSIBLE_NAME);
  builder->Add("fingerprintHint", IDS_FINGERPRINT_HINT_TEXT);
  builder->Add("fingerprintIconMessage", IDS_FINGERPRINT_ICON_MESSAGE);
  builder->Add("fingerprintSigningin", IDS_FINGERPRINT_LOGIN_TEXT);
  builder->Add("fingerprintSigninFailed", IDS_FINGERPRINT_LOGIN_FAILED_TEXT);
  builder->Add("signingIn", IDS_LOGIN_POD_SIGNING_IN);
  builder->Add("podMenuButtonAccessibleName",
               IDS_LOGIN_POD_MENU_BUTTON_ACCESSIBLE_NAME);
  builder->Add("podMenuRemoveItemAccessibleName",
               IDS_LOGIN_POD_MENU_REMOVE_ITEM_ACCESSIBLE_NAME);
  builder->Add("passwordFieldAccessibleName",
               IDS_LOGIN_POD_PASSWORD_FIELD_ACCESSIBLE_NAME);
  builder->Add("submitButtonAccessibleName",
               IDS_LOGIN_POD_SUBMIT_BUTTON_ACCESSIBLE_NAME);
  builder->Add("signedIn", IDS_SCREEN_LOCK_ACTIVE_USER);
  builder->Add("offlineLogin", IDS_OFFLINE_LOGIN_HTML);
  builder->Add("ownerUserPattern", IDS_LOGIN_POD_OWNER_USER);
  builder->Add("removeUser", IDS_LOGIN_POD_REMOVE_USER);

  builder->Add("disabledAddUserTooltip",
               webui::IsEnterpriseManaged()
                   ? IDS_DISABLED_ADD_USER_TOOLTIP_ENTERPRISE
                   : IDS_DISABLED_ADD_USER_TOOLTIP);

  builder->Add("supervisedUserExpiredTokenWarning",
               IDS_SUPERVISED_USER_EXPIRED_TOKEN_WARNING);
  builder->Add("signinBannerText", IDS_LOGIN_USER_ADDING_BANNER);

  // Multi-profiles related strings.
  builder->Add("multiProfilesRestrictedPolicyTitle",
               IDS_MULTI_PROFILES_RESTRICTED_POLICY_TITLE);
  builder->Add("multiProfilesNotAllowedPolicyMsg",
               IDS_MULTI_PROFILES_NOT_ALLOWED_POLICY_MSG);
  builder->Add("multiProfilesPrimaryOnlyPolicyMsg",
               IDS_MULTI_PROFILES_PRIMARY_ONLY_POLICY_MSG);
  builder->Add("multiProfilesOwnerPrimaryOnlyMsg",
               IDS_MULTI_PROFILES_OWNER_PRIMARY_ONLY_MSG);

  // Used by SAML password dialog.
  builder->Add("nextButtonText", IDS_OFFLINE_LOGIN_NEXT_BUTTON_TEXT);

  builder->Add("publicAccountInfoFormat", IDS_LOGIN_PUBLIC_ACCOUNT_INFO_FORMAT);
  builder->Add("publicAccountReminder",
               IDS_LOGIN_PUBLIC_ACCOUNT_SIGNOUT_REMINDER);
  builder->Add("publicSessionLanguageAndInput",
               IDS_LOGIN_PUBLIC_SESSION_LANGUAGE_AND_INPUT);
  builder->Add("publicAccountEnter", IDS_LOGIN_PUBLIC_ACCOUNT_ENTER);
  builder->Add("publicAccountEnterAccessibleName",
               IDS_LOGIN_PUBLIC_ACCOUNT_ENTER_ACCESSIBLE_NAME);
  builder->Add("publicAccountMonitoringWarning",
               IDS_LOGIN_PUBLIC_ACCOUNT_MONITORING_WARNING);
  builder->Add("publicAccountLearnMore", IDS_LEARN_MORE);
  builder->Add("publicAccountMonitoringInfo",
               IDS_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO);
  builder->Add("publicAccountMonitoringInfoItem1",
               IDS_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO_ITEM_1);
  builder->Add("publicAccountMonitoringInfoItem2",
               IDS_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO_ITEM_2);
  builder->Add("publicAccountMonitoringInfoItem3",
               IDS_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO_ITEM_3);
  builder->Add("publicAccountMonitoringInfoItem4",
               IDS_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO_ITEM_4);
  builder->Add("publicSessionSelectLanguage", IDS_LANGUAGE_SELECTION_SELECT);
  builder->Add("publicSessionSelectKeyboard", IDS_KEYBOARD_SELECTION_SELECT);
  builder->Add("removeUserWarningTextNonSyncNoStats", base::string16());
  builder->Add("removeUserWarningTextNonSyncCalculating", base::string16());
  builder->Add("removeUserWarningTextHistory", base::string16());
  builder->Add("removeUserWarningTextPasswords", base::string16());
  builder->Add("removeUserWarningTextBookmarks", base::string16());
  builder->Add("removeUserWarningTextAutofill", base::string16());
  builder->Add("removeUserWarningTextCalculating", base::string16());
  builder->Add("removeUserWarningTextSyncNoStats", base::string16());
  builder->Add("removeUserWarningTextSyncCalculating", base::string16());
  builder->AddF("removeLegacySupervisedUserWarningText",
               IDS_LOGIN_POD_LEGACY_SUPERVISED_USER_REMOVE_WARNING,
               base::UTF8ToUTF16(
                   chrome::kLegacySupervisedUserManagementDisplayURL));
  builder->Add("removeNonOwnerUserWarningText",
               IDS_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING);
  builder->Add("removeUserWarningButtonTitle",
               IDS_LOGIN_POD_USER_REMOVE_WARNING_BUTTON);
  builder->Add("samlNotice", IDS_LOGIN_SAML_NOTICE);
  builder->Add("samlNoticeWithVideo", IDS_LOGIN_SAML_NOTICE_WITH_VIDEO);
  builder->AddF("confirmPasswordTitle", IDS_LOGIN_CONFIRM_PASSWORD_TITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("manualPasswordTitle", IDS_LOGIN_MANUAL_PASSWORD_TITLE);
  builder->Add("manualPasswordInputLabel",
               IDS_LOGIN_MANUAL_PASSWORD_INPUT_LABEL);
  builder->Add("manualPasswordMismatch",
               IDS_LOGIN_MANUAL_PASSWORD_MISMATCH);
  builder->Add("confirmPasswordLabel", IDS_LOGIN_CONFIRM_PASSWORD_LABEL);
  builder->Add("confirmPasswordIncorrectPassword",
               IDS_LOGIN_CONFIRM_PASSWORD_INCORRECT_PASSWORD);
  builder->Add("accountSetupCancelDialogTitle",
               IDS_LOGIN_ACCOUNT_SETUP_CANCEL_DIALOG_TITLE);
  builder->Add("accountSetupCancelDialogNo",
               IDS_LOGIN_ACCOUNT_SETUP_CANCEL_DIALOG_NO);
  builder->Add("accountSetupCancelDialogYes",
               IDS_LOGIN_ACCOUNT_SETUP_CANCEL_DIALOG_YES);

  builder->Add("fatalEnrollmentError",
               IDS_ENTERPRISE_ENROLLMENT_AUTH_FATAL_ERROR);
  builder->Add("insecureURLEnrollmentError",
               IDS_ENTERPRISE_ENROLLMENT_AUTH_INSECURE_URL_ERROR);
}

void SigninScreenHandler::RegisterMessages() {
  AddCallback("authenticateUser", &SigninScreenHandler::HandleAuthenticateUser);
  AddCallback("completeOfflineAuthentication",
              &SigninScreenHandler::HandleCompleteOfflineAuthentication);
  AddCallback("launchIncognito", &SigninScreenHandler::HandleLaunchIncognito);
  AddCallback("launchSAMLPublicSession",
              &SigninScreenHandler::HandleLaunchSAMLPublicSession);
  AddRawCallback("offlineLogin", &SigninScreenHandler::HandleOfflineLogin);
  AddCallback("toggleEnrollmentScreen",
              &SigninScreenHandler::HandleToggleEnrollmentScreen);
  AddCallback("toggleKioskEnableScreen",
              &SigninScreenHandler::HandleToggleKioskEnableScreen);
  AddCallback("accountPickerReady",
              &SigninScreenHandler::HandleAccountPickerReady);
  AddCallback("openInternetDetailDialog",
              &SigninScreenHandler::HandleOpenInternetDetailDialog);
  AddCallback("loginVisible", &SigninScreenHandler::HandleLoginVisible);
  AddCallback("cancelUserAdding", &SigninScreenHandler::HandleCancelUserAdding);
  AddCallback("loginUIStateChanged",
              &SigninScreenHandler::HandleLoginUIStateChanged);
  AddCallback("showLoadingTimeoutError",
              &SigninScreenHandler::HandleShowLoadingTimeoutError);
  AddCallback("focusPod", &SigninScreenHandler::HandleFocusPod);
  AddCallback("noPodFocused", &SigninScreenHandler::HandleNoPodFocused);
  AddCallback("getPublicSessionKeyboardLayouts",
              &SigninScreenHandler::HandleGetPublicSessionKeyboardLayouts);
  AddCallback("getTabletModeState",
              &SigninScreenHandler::HandleGetTabletModeState);
  AddCallback("getDemoModeState", &SigninScreenHandler::HandleGetDemoModeState);
  AddCallback("logRemoveUserWarningShown",
              &SigninScreenHandler::HandleLogRemoveUserWarningShown);
  AddCallback("firstIncorrectPasswordAttempt",
              &SigninScreenHandler::HandleFirstIncorrectPasswordAttempt);
  AddCallback("maxIncorrectPasswordAttempts",
              &SigninScreenHandler::HandleMaxIncorrectPasswordAttempts);
  AddCallback("sendFeedback", &SigninScreenHandler::HandleSendFeedback);
}

void SigninScreenHandler::Show(bool oobe_ui) {
  CHECK(delegate_);

  // Just initialize internal fields from context and call ShowImpl().
  oobe_ui_ = oobe_ui;

  ShowImpl();
  histogram_helper_->OnScreenShow();
}

void SigninScreenHandler::SetDelegate(SigninScreenHandlerDelegate* delegate) {
  delegate_ = delegate;
  if (delegate_)
    delegate_->SetWebUIHandler(this);
}

void SigninScreenHandler::OnNetworkReady() {
  VLOG(1) << "OnNetworkReady() call.";
  gaia_screen_handler_->MaybePreloadAuthExtension();
}

void SigninScreenHandler::UpdateState(NetworkError::ErrorReason reason) {
  // ERROR_REASON_FRAME_ERROR is an explicit signal from GAIA frame so it shoud
  // force network error UI update.
  bool force_update = reason == NetworkError::ERROR_REASON_FRAME_ERROR;
  UpdateStateInternal(reason, force_update);
}

void SigninScreenHandler::SetOfflineTimeoutForTesting(
    base::TimeDelta offline_timeout) {
  is_offline_timeout_for_test_set_ = true;
  offline_timeout_for_test_ = offline_timeout;
}

bool SigninScreenHandler::GetKeyboardRemappedPrefValue(
    const std::string& pref_name,
    int* value) {
  return focused_pod_account_id_ && focused_pod_account_id_->is_valid() &&
         user_manager::known_user::GetIntegerPref(*focused_pod_account_id_,
                                                  pref_name, value);
}

// SigninScreenHandler, private: -----------------------------------------------

void SigninScreenHandler::ShowImpl() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  if (!ime_state_.get())
    ime_state_ = input_method::InputMethodManager::Get()->GetActiveIMEState();

  if (!oobe_ui_observer_added_) {
    oobe_ui_observer_added_ = true;
    GetOobeUI()->AddObserver(this);
  }

  if (oobe_ui_) {
    // Shows new user sign-in for OOBE.
    gaia_screen_handler_->OnShowAddUser();
  } else {
    // Populates account picker. Animation is turned off for now until we
    // figure out how to make it fast enough. This will call LoadUsers.
    delegate_->HandleGetUsers();

    // Reset Caps Lock state when login screen is shown.
    input_method::InputMethodManager::Get()
        ->GetImeKeyboard()
        ->SetCapsLockEnabled(false);

    UpdateUIState(UI_STATE_ACCOUNT_PICKER);
  }
}

void SigninScreenHandler::UpdateUIState(UIState ui_state) {
  switch (ui_state) {
    case UI_STATE_GAIA_SIGNIN:
      ui_state_ = UI_STATE_GAIA_SIGNIN;
      break;
    case UI_STATE_ACCOUNT_PICKER:
      ui_state_ = UI_STATE_ACCOUNT_PICKER;
      ShowScreen(OobeScreen::SCREEN_ACCOUNT_PICKER);
      break;
    default:
      NOTREACHED();
      break;
  }
}

// TODO(antrim@): split this method into small parts.
// TODO(antrim@): move this logic to GaiaScreenHandler.
void SigninScreenHandler::UpdateStateInternal(NetworkError::ErrorReason reason,
                                              bool force_update) {
  // Do nothing once user has signed in or sign in is in progress.
  // TODO(antrim): We will end up here when processing network state
  // notification but no ShowSigninScreen() was called so delegate_ will be
  // nullptr. Network state processing logic does not belong here.
  if (delegate_ &&
      (delegate_->IsUserSigninCompleted() || delegate_->IsSigninInProgress())) {
    return;
  }

  NetworkStateInformer::State state = network_state_informer_->state();
  const std::string network_path = network_state_informer_->network_path();
  const std::string network_name = GetNetworkName(network_path);

  // Skip "update" notification about OFFLINE state from
  // NetworkStateInformer if previous notification already was
  // delayed.
  if ((state == NetworkStateInformer::OFFLINE ||
       network_state_ignored_until_proxy_auth_) &&
      !force_update && !update_state_closure_.IsCancelled()) {
    return;
  }

  update_state_closure_.Cancel();

  if ((state == NetworkStateInformer::OFFLINE && !force_update) ||
      network_state_ignored_until_proxy_auth_) {
    update_state_closure_.Reset(
        base::Bind(&SigninScreenHandler::UpdateStateInternal,
                   weak_factory_.GetWeakPtr(),
                   reason,
                   true));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, update_state_closure_.callback(),
        is_offline_timeout_for_test_set_ ? offline_timeout_for_test_
                                         : kOfflineTimeout);
    return;
  }

  // Don't show or hide error screen if we're in connecting state.
  if (state == NetworkStateInformer::CONNECTING && !force_update) {
    if (connecting_closure_.IsCancelled()) {
      // First notification about CONNECTING state.
      connecting_closure_.Reset(
          base::Bind(&SigninScreenHandler::UpdateStateInternal,
                     weak_factory_.GetWeakPtr(),
                     reason,
                     true));
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, connecting_closure_.callback(), kConnectingTimeout);
    }
    return;
  }
  connecting_closure_.Cancel();

  const bool is_online = IsOnline(state, reason);
  const bool is_behind_captive_portal = IsBehindCaptivePortal(state, reason);
  const bool is_gaia_loading_timeout =
      (reason == NetworkError::ERROR_REASON_LOADING_TIMEOUT);
  const bool is_gaia_error =
      FrameError() != net::OK && FrameError() != net::ERR_NETWORK_CHANGED;
  const bool is_gaia_signin = IsGaiaVisible() || IsGaiaHiddenByError();
  const bool offline_login_active =
      gaia_screen_handler_->IsOfflineLoginActive();
  const bool error_screen_should_overlay =
      !offline_login_active && IsGaiaVisible();
  const bool from_not_online_to_online_transition =
      is_online && last_network_state_ != NetworkStateInformer::ONLINE;
  last_network_state_ = state;
  proxy_auth_dialog_need_reload_ =
      (reason == NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED) &&
      (state == NetworkStateInformer::PROXY_AUTH_REQUIRED) &&
      (proxy_auth_dialog_reload_times_ > 0);

  CallOnReturn reload_gaia(base::Bind(
      &SigninScreenHandler::ReloadGaia, weak_factory_.GetWeakPtr(), true));

  if (is_online || !is_behind_captive_portal)
    error_screen_->HideCaptivePortal();

  // Hide offline message (if needed) and return if current screen is
  // not a Gaia frame.
  if (!is_gaia_signin) {
    if (!IsSigninScreenHiddenByError())
      HideOfflineMessage(state, reason);
    return;
  }

  // Use the online login page if the user has not used the machine for awhile.
  if (offline_login_active)
    gaia_screen_handler_->MonitorOfflineIdle(is_online);

  // Reload frame if network state is changed from {!ONLINE} -> ONLINE state.
  if (reason == NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED &&
      from_not_online_to_online_transition) {
    // Schedules a immediate retry.
    LOG(WARNING) << "Retry frame load since network has been changed.";
    gaia_reload_reason_ = reason;
    reload_gaia.ScheduleCall();
  }

  if (reason == NetworkError::ERROR_REASON_PROXY_CONFIG_CHANGED &&
      error_screen_should_overlay) {
    // Schedules a immediate retry.
    LOG(WARNING) << "Retry frameload since proxy settings has been changed.";
    gaia_reload_reason_ = reason;
    reload_gaia.ScheduleCall();
  }

  if (reason == NetworkError::ERROR_REASON_FRAME_ERROR &&
      reason != gaia_reload_reason_ &&
      !IsProxyError(state, reason, FrameError())) {
    LOG(WARNING) << "Retry frame load due to reason: "
                 << NetworkError::ErrorReasonString(reason);
    gaia_reload_reason_ = reason;
    reload_gaia.ScheduleCall();
  }

  if (is_gaia_loading_timeout) {
    LOG(WARNING) << "Retry frame load due to loading timeout.";
    reload_gaia.ScheduleCall();
  }

  if (proxy_auth_dialog_need_reload_) {
    --proxy_auth_dialog_reload_times_;
    LOG(WARNING) << "Retry frame load to show proxy auth dialog";
    reload_gaia.ScheduleCall();
  }

  if ((!is_online || is_gaia_loading_timeout || is_gaia_error) &&
      !offline_login_active) {
    SetupAndShowOfflineMessage(state, reason);
  } else {
    HideOfflineMessage(state, reason);

    // Cancel scheduled GAIA reload (if any) to prevent double reloads.
    reload_gaia.CancelScheduledCall();
  }
}

void SigninScreenHandler::SetupAndShowOfflineMessage(
    NetworkStateInformer::State state,
    NetworkError::ErrorReason reason) {
  const std::string network_path = network_state_informer_->network_path();
  const bool is_behind_captive_portal = IsBehindCaptivePortal(state, reason);
  const bool is_proxy_error = IsProxyError(state, reason, FrameError());
  const bool is_gaia_loading_timeout =
      (reason == NetworkError::ERROR_REASON_LOADING_TIMEOUT);

  if (is_proxy_error) {
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_PROXY,
                                 std::string());
  } else if (is_behind_captive_portal) {
    // Do not bother a user with obsessive captive portal showing. This
    // check makes captive portal being shown only once: either when error
    // screen is shown for the first time or when switching from another
    // error screen (offline, proxy).
    if (IsGaiaVisible() ||
        (error_screen_->GetErrorState() != NetworkError::ERROR_STATE_PORTAL)) {
      LoginDisplayHost::default_host()->HandleDisplayCaptivePortal();
    }
    const std::string network_name = GetNetworkName(network_path);
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_PORTAL,
                                 network_name);
  } else if (is_gaia_loading_timeout) {
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT,
                                 std::string());
  } else {
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_OFFLINE,
                                 std::string());
  }

  bool guest_signin_allowed = false;
  bool offline_login_allowed = false;
  if (IsSigninScreenError(error_screen_->GetErrorState())) {
    guest_signin_allowed =
        user_manager::UserManager::Get()->IsGuestSessionAllowed();
    offline_login_allowed = error_screen_->GetErrorState() !=
                            NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT;
  }
  error_screen_->AllowGuestSignin(guest_signin_allowed);
  error_screen_->AllowOfflineLogin(offline_login_allowed);

  if (GetCurrentScreen() != ErrorScreenView::kScreenId) {
    error_screen_->SetUIState(NetworkError::UI_STATE_SIGNIN);
    error_screen_->SetParentScreen(GaiaView::kScreenId);
    error_screen_->Show(nullptr);
    histogram_helper_->OnErrorShow(error_screen_->GetErrorState());
  }
}

void SigninScreenHandler::HideOfflineMessage(NetworkStateInformer::State state,
                                             NetworkError::ErrorReason reason) {
  if (!IsSigninScreenHiddenByError())
    return;

  gaia_reload_reason_ = NetworkError::ERROR_REASON_NONE;

  error_screen_->Hide();
  histogram_helper_->OnErrorHide();

  // Forces a reload for Gaia screen on hiding error message.
  if (IsGaiaVisible() || IsGaiaHiddenByError())
    ReloadGaia(reason == NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED);
}

void SigninScreenHandler::ReloadGaia(bool force_reload) {
  gaia_screen_handler_->ReloadGaia(force_reload);
}

void SigninScreenHandler::Initialize() {
  // Preload PIN keyboard if any of the users can authenticate via PIN.
  if (user_manager::UserManager::IsInitialized()) {
    for (user_manager::User* user :
         user_manager::UserManager::Get()->GetUnlockUsers()) {
      quick_unlock::PinBackend::GetInstance()->CanAuthenticate(
          user->GetAccountId(),
          base::BindOnce(&SigninScreenHandler::PreloadPinKeyboard,
                         weak_factory_.GetWeakPtr()));
    }
  }

  // |delegate_| is null when we are preloading the lock screen.
  if (delegate_ && show_on_init_) {
    show_on_init_ = false;
    ShowImpl();
  }
}

void SigninScreenHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kUsersLastInputMethod);
}

void SigninScreenHandler::OnCurrentScreenChanged(OobeScreenId current_screen,
                                                 OobeScreenId new_screen) {
  if (new_screen == OobeScreen::SCREEN_ACCOUNT_PICKER) {
    // Restore active IME state if returning to user pod row screen.
    input_method::InputMethodManager::Get()->SetState(ime_state_);
  }
}

void SigninScreenHandler::OnWallpaperColorsChanged() {
  // Updates the color of the scrollable container on account picker screen,
  // based on wallpaper color extraction results.
  auto colors = WallpaperControllerClient::Get()->GetWallpaperColors();
  SkColor dark_muted_color =
      colors[static_cast<int>(ash::ColorProfileType::DARK_MUTED)];
  if (dark_muted_color == ash::kInvalidWallpaperColor)
    dark_muted_color = ash::login_constants::kDefaultBaseColor;

  dark_muted_color = SkColorSetA(dark_muted_color, 0xFF);
  SkColor base_color = color_utils::GetResultingPaintColor(
      SkColorSetA(ash::login_constants::kDefaultBaseColor,
                  ash::login_constants::kTranslucentColorDarkenAlpha),
      dark_muted_color);
  SkColor scroll_color =
      SkColorSetA(base_color, ash::login_constants::kScrollTranslucentAlpha);
  CallJS("login.AccountPickerScreen.setOverlayColors",
         color_utils::SkColorToRgbaString(dark_muted_color),
         color_utils::SkColorToRgbaString(scroll_color));
}

void SigninScreenHandler::OnWallpaperBlurChanged() {
  const bool show_pod_background =
      !WallpaperControllerClient::Get()->IsWallpaperBlurred();
  CallJS("login.AccountPickerScreen.togglePodBackground", show_pod_background);
}

void SigninScreenHandler::ClearAndEnablePassword() {
  core_oobe_view_->ResetSignInUI(false);
}

void SigninScreenHandler::ClearUserPodPassword() {
  core_oobe_view_->ClearUserPodPassword();
}

void SigninScreenHandler::RefocusCurrentPod() {
  core_oobe_view_->RefocusCurrentPod();
}

void SigninScreenHandler::UpdatePinKeyboardState(const AccountId& account_id) {
  quick_unlock::PinBackend::GetInstance()->CanAuthenticate(
      account_id, base::BindOnce(&SigninScreenHandler::SetPinEnabledForUser,
                                 weak_factory_.GetWeakPtr(), account_id));
}

void SigninScreenHandler::SetPinEnabledForUser(const AccountId& account_id,
                                               bool is_enabled) {
  CallJS("login.AccountPickerScreen.setPinEnabledForUser", account_id,
         is_enabled);
}

void SigninScreenHandler::PreloadPinKeyboard(bool should_preload) {
  if (should_preload)
    CallJS("cr.ui.Oobe.preloadPinKeyboard");
}

void SigninScreenHandler::OnUserImageChanged(const user_manager::User& user) {
  if (page_is_ready()) {
    CallJS("login.AccountPickerScreen.updateUserImage", user.GetAccountId());
  }
}

void SigninScreenHandler::OnPreferencesChanged() {
  // Make sure that one of the login UI is fully functional now, otherwise
  // preferences update would be picked up next time it will be shown.
  if (!webui_visible_) {
    LOG(WARNING) << "Login UI is not active - postponed prefs change.";
    preferences_changed_delayed_ = true;
    return;
  }

  preferences_changed_delayed_ = false;

  if (!delegate_)
    return;

  // Send the updated user list to the UI.
  delegate_->HandleGetUsers();
  if (GetCurrentScreen() == OobeScreen::SCREEN_ACCOUNT_PICKER &&
      delegate_->ShowUsersHasChanged() &&
      !delegate_->IsShowUsers()) {
    // We are at the account picker screen and the POD setting has changed
    // to be disabled. We need to show the add user page.
    gaia_screen_handler_->HandleShowAddUser(nullptr);
    return;
  }

  if (delegate_->AllowNewUserChanged() || ui_state_ == UI_STATE_UNKNOWN) {
    // We need to reload GAIA if UI_STATE_UNKNOWN or the allow new user setting
    // has changed so that reloaded GAIA shows/hides the option to create a new
    // account.
    GaiaScreen* gaia_screen = GaiaScreen::Get(
        WizardController::default_controller()->screen_manager());
    gaia_screen->LoadOnline(EmptyAccountId());
  }
}

void SigninScreenHandler::ResetSigninScreenHandlerDelegate() {
  SetDelegate(nullptr);
}

void SigninScreenHandler::ShowError(int login_attempts,
                                    const std::string& error_text,
                                    const std::string& help_link_text,
                                    HelpAppLauncher::HelpTopic help_topic_id) {
  core_oobe_view_->ShowSignInError(login_attempts, error_text, help_link_text,
                                   help_topic_id);
}

void SigninScreenHandler::ShowSigninUI(const std::string& email) {
  core_oobe_view_->ShowSignInUI(email);
}

void SigninScreenHandler::ShowAllowlistCheckFailedError() {
  gaia_screen_handler_->ShowAllowlistCheckFailedError();
}

void SigninScreenHandler::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_AUTH_NEEDED: {
      network_state_ignored_until_proxy_auth_ = true;
      break;
    }
    case chrome::NOTIFICATION_AUTH_SUPPLIED: {
      if (IsGaiaHiddenByError()) {
        // Start listening to network state notifications immediately, hoping
        // that the network will switch to ONLINE soon.
        update_state_closure_.Cancel();
        ReenableNetworkStateUpdatesAfterProxyAuth();
      } else {
        // Gaia is not hidden behind an error yet. Discard last cached network
        // state notification and wait for |kProxyAuthTimeout| before
        // considering network update notifications again (hoping the network
        // will become ONLINE by then).
        update_state_closure_.Cancel();
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                &SigninScreenHandler::ReenableNetworkStateUpdatesAfterProxyAuth,
                weak_factory_.GetWeakPtr()),
            kProxyAuthTimeout);
      }
      break;
    }
    case chrome::NOTIFICATION_AUTH_CANCELLED: {
      update_state_closure_.Cancel();
      ReenableNetworkStateUpdatesAfterProxyAuth();
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

void SigninScreenHandler::ReenableNetworkStateUpdatesAfterProxyAuth() {
  network_state_ignored_until_proxy_auth_ = false;
}

void SigninScreenHandler::SuspendDone(const base::TimeDelta& sleep_duration) {
  for (user_manager::User* user :
       user_manager::UserManager::Get()->GetUnlockUsers()) {
    UpdatePinKeyboardState(user->GetAccountId());
  }
}

void SigninScreenHandler::OnTabletModeStarted() {
  OnTabletModeToggled(true);
}

void SigninScreenHandler::OnTabletModeEnded() {
  OnTabletModeToggled(false);
}

void SigninScreenHandler::OnTabletModeToggled(bool enabled) {
  CallJS("login.AccountPickerScreen.setTabletModeState", enabled);
}

bool SigninScreenHandler::ShouldLoadGaia() const {
  // Fetching of the extension is not started before account picker page is
  // loaded because it can affect the loading speed.
  // Do not load the extension for the screen locker, see crosbug.com/25018.
  return !ScreenLocker::default_screen_locker() &&
         is_account_picker_showing_first_time_;
}

void SigninScreenHandler::HandleAuthenticateUser(const AccountId& account_id,
                                                 const std::string& password,
                                                 bool authenticated_by_pin) {
  AuthenticateExistingUser(account_id, password, authenticated_by_pin);
}

void SigninScreenHandler::AuthenticateExistingUser(const AccountId& account_id,
                                                   const std::string& password,
                                                   bool authenticated_by_pin) {
  if (!delegate_)
    return;
  DCHECK_EQ(account_id.GetUserEmail(),
            gaia::SanitizeEmail(account_id.GetUserEmail()));

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  UserContext user_context;
  if (!user) {
    LOG(ERROR) << "AuthenticateExistingUser: User not found! account type="
               << AccountId::AccountTypeToString(account_id.GetAccountType());
    const user_manager::UserType user_type =
        (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY)
            ? user_manager::USER_TYPE_ACTIVE_DIRECTORY
            : user_manager::UserType::USER_TYPE_REGULAR;
    user_context = UserContext(user_type, account_id);
  } else {
    user_context = UserContext(*user);
  }
  user_context.SetKey(Key(password));
  user_context.SetPasswordKey(Key(password));
  user_context.SetIsUsingPin(authenticated_by_pin);
  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY) {
    if (user_context.GetUserType() !=
        user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY) {
      LOG(FATAL) << "Incorrect Active Directory user type "
                 << user_context.GetUserType();
    }
    user_context.SetIsUsingOAuth(false);
  }

  delegate_->Login(user_context, SigninSpecifics());

  UpdatePinKeyboardState(account_id);
}

void SigninScreenHandler::HandleCompleteOfflineAuthentication(
    const std::string& email,
    const std::string& password) {
  const std::string sanitized_email = gaia::SanitizeEmail(email);
  const AccountId account_id = user_manager::known_user::GetAccountId(
      sanitized_email, std::string() /* id */, AccountType::UNKNOWN);
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user) {
    LOG(ERROR)
        << "HandleCompleteOfflineAuthentication: User not found! account type="
        << AccountId::AccountTypeToString(account_id.GetAccountType());
    LoginDisplayHost::default_host()->GetLoginDisplay()->ShowError(
        IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED, 1,
        HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
    return;
  }

  AuthenticateExistingUser(account_id, password,
                           false /* authenticated_by_pin */);
}

void SigninScreenHandler::HandleLaunchIncognito() {
  UserContext context(user_manager::USER_TYPE_GUEST, EmptyAccountId());
  if (delegate_)
    delegate_->Login(context, SigninSpecifics());
}

void SigninScreenHandler::HandleLaunchSAMLPublicSession(
    const std::string& email) {
  if (!delegate_)
    return;

  const AccountId account_id = user_manager::known_user::GetAccountId(
      email, std::string() /* id */, AccountType::UNKNOWN);

  UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT, account_id);
  delegate_->Login(context, SigninSpecifics());
}

void SigninScreenHandler::HandleOfflineLogin(const base::ListValue* args) {
  if (!delegate_) {
    NOTREACHED();
    return;
  }
  std::string email;
  args->GetString(0, &email);

  GaiaScreen* gaia_screen =
      GaiaScreen::Get(WizardController::default_controller()->screen_manager());
  gaia_screen->LoadOffline(AccountId::FromUserEmail(email));
  HideOfflineMessage(NetworkStateInformer::OFFLINE,
                     NetworkError::ERROR_REASON_NONE);
  LoginDisplayHost::default_host()->StartWizard(GaiaView::kScreenId);

  UpdateUIState(UI_STATE_GAIA_SIGNIN);
}

void SigninScreenHandler::HandleToggleEnrollmentScreen() {
  if (delegate_)
    delegate_->ShowEnterpriseEnrollmentScreen();
}

void SigninScreenHandler::HandleToggleKioskEnableScreen() {
  if (delegate_ && !webui::IsEnterpriseManaged() &&
      KioskAppManager::IsConsumerKioskEnabled() &&
      LoginDisplayHost::default_host()) {
    delegate_->ShowKioskEnableScreen();
  }
}

void SigninScreenHandler::HandleToggleKioskAutolaunchScreen() {
  if (delegate_ && !webui::IsEnterpriseManaged())
    delegate_->ShowKioskAutolaunchScreen();
}

void SigninScreenHandler::LoadUsers(const user_manager::UserList& users,
                                    const base::ListValue& users_list) {
  CallJS("login.AccountPickerScreen.loadUsers", users_list);

  // Enable pin for any users who can use it.
  // TODO(jdufault): Cache pin state in BrowserProcess::local_state() so we
  // don't need to query cryptohome every time we show login. See
  // https://crbug.com/721938.
  for (user_manager::User* user : users)
    UpdatePinKeyboardState(user->GetAccountId());
}

void SigninScreenHandler::HandleAccountPickerReady() {
  VLOG(0) << "Login WebUI >> AccountPickerReady";

  if (delegate_ && !ScreenLocker::default_screen_locker() &&
      !chromeos::IsMachineHWIDCorrect() &&
      !oobe_ui_) {
    delegate_->ShowWrongHWIDScreen();
    return;
  }

  is_account_picker_showing_first_time_ = true;

  // The wallpaper may have been set before the instance is initialized, so make
  // sure the colors and blur state are updated.
  OnWallpaperColorsChanged();
  OnWallpaperBlurChanged();

  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  if (session_manager->session_state() == session_manager::SessionState::OOBE) {
    // This updates post-OOBE shelf UI. Changes the color of shelf buttons and
    // displays additional buttons that should only be shown in the login screen
    session_manager->SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);
  }

  if (delegate_)
    delegate_->OnSigninScreenReady();
}

void SigninScreenHandler::HandleOpenInternetDetailDialog() {
  // Empty string opens the internet detail dialog for the default network.
  InternetDetailDialog::ShowDialog("");
}

void SigninScreenHandler::HandleLoginVisible(const std::string& source) {
  VLOG(1) << "Login WebUI >> loginVisible, src: " << source << ", "
          << "webui_visible_: " << webui_visible_;
  if (!webui_visible_) {
    // There might be multiple messages from OOBE UI so send notifications after
    // the first one only.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
    TRACE_EVENT_NESTABLE_ASYNC_END0("ui", "ShowLoginWebUI",
                                    LoginDisplayHostWebUI::kShowLoginWebUIid);
  }
  webui_visible_ = true;
  if (preferences_changed_delayed_)
    OnPreferencesChanged();
}

void SigninScreenHandler::HandleCancelUserAdding() {
  if (delegate_)
    delegate_->CancelUserAdding();
}

void SigninScreenHandler::HandleLoginUIStateChanged(const std::string& source,
                                                    bool active) {
  VLOG(0) << "Login WebUI >> active: " << active << ", "
            << "source: " << source;

  if (!KioskAppManager::Get()->GetAutoLaunchApp().empty() &&
      KioskAppManager::Get()->IsAutoLaunchRequested()) {
    VLOG(0) << "Showing auto-launch warning";
    // On slow devices, the wallpaper animation is not shown initially, so we
    // must explicitly load the wallpaper. This is also the case for the
    // account-picker and gaia-signin UI states.
    LoginDisplayHost::default_host()->LoadSigninWallpaper();
    HandleToggleKioskAutolaunchScreen();
    return;
  }

  if (source == kSourceGaiaSignin) {
    ui_state_ = UI_STATE_GAIA_SIGNIN;
  } else if (source == kSourceAccountPicker) {
    ui_state_ = UI_STATE_ACCOUNT_PICKER;
  } else {
    NOTREACHED();
    return;
  }
}

void SigninScreenHandler::HandleShowLoadingTimeoutError() {
  UpdateState(NetworkError::ERROR_REASON_LOADING_TIMEOUT);
}

void SigninScreenHandler::HandleFocusPod(const AccountId& account_id,
                                         bool is_large_pod) {
  proximity_auth::ScreenlockBridge::Get()->SetFocusedUser(account_id);
  const bool is_same_pod_focused =
      focused_pod_account_id_ && *focused_pod_account_id_ == account_id;

  if (delegate_ && !is_same_pod_focused)
    delegate_->CheckUserStatus(account_id);

  focused_pod_account_id_ = std::make_unique<AccountId>(account_id);

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  // |user| may be nullptr in kiosk mode or unit tests.
  if (user && user->is_logged_in() && !user->is_active()) {
    SessionControllerClientImpl::DoSwitchActiveUser(account_id);
    return;
  }

  if (LoginDisplayHost::default_host() && is_large_pod)
    LoginDisplayHost::default_host()->LoadWallpaper(account_id);

  if (is_same_pod_focused)
    return;

  // TODO(https://crbug.com/1071779): Migrate KioskTest to Views Account picker.
  // DCHECK_EQ(session_manager::SessionManager::Get()->session_state(),
  //           session_manager::SessionState::LOGIN_SECONDARY);
  lock_screen_utils::SetUserInputMethod(account_id, ime_state_.get(),
                                        false /*honor_device_policy*/);
  lock_screen_utils::SetKeyboardSettings(account_id);

  bool use_24hour_clock = false;
  if (user_manager::known_user::GetBooleanPref(
          account_id, prefs::kUse24HourClock, &use_24hour_clock)) {
    g_browser_process->platform_part()
        ->GetSystemClock()
        ->SetLastFocusedPodHourClockType(use_24hour_clock ? base::k24HourClock
                                                          : base::k12HourClock);
  }
}

void SigninScreenHandler::HandleNoPodFocused() {
  focused_pod_account_id_.reset();
}

void SigninScreenHandler::HandleGetPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale) {
  GetKeyboardLayoutsForLocale(
      base::Bind(&SigninScreenHandler::SendPublicSessionKeyboardLayouts,
                 weak_factory_.GetWeakPtr(), account_id, locale),
      locale);
}

void SigninScreenHandler::SendPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale,
    std::unique_ptr<base::ListValue> keyboard_layouts) {
  CallJS("login.AccountPickerScreen.setPublicSessionKeyboardLayouts",
         account_id, locale, *keyboard_layouts);
}

void SigninScreenHandler::HandleGetTabletModeState() {
  CallJS("login.AccountPickerScreen.setTabletModeState",
         ash::TabletMode::Get()->InTabletMode());
}

void SigninScreenHandler::HandleGetDemoModeState() {
  CallJS("login.AccountPickerScreen.setDemoModeState",
         DemoSession::IsDeviceInDemoMode());
}

void SigninScreenHandler::HandleLogRemoveUserWarningShown() {
  ProfileMetrics::LogProfileDeleteUser(
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER_SHOW_WARNING);
}

void SigninScreenHandler::HandleFirstIncorrectPasswordAttempt(
    const AccountId& account_id) {
  // TODO(ginkage): Fix this case once crbug.com/469987 is ready.
  /*
    if (user_manager::known_user::IsUsingSAML(email))
      RecordReauthReason(email, ReauthReason::INCORRECT_SAML_PASSWORD_ENTERED);
  */
}

void SigninScreenHandler::HandleMaxIncorrectPasswordAttempts(
    const AccountId& account_id) {
  RecordReauthReason(account_id, ReauthReason::INCORRECT_PASSWORD_ENTERED);
}

void SigninScreenHandler::HandleSendFeedback() {
  login_feedback_ =
      std::make_unique<LoginFeedback>(Profile::FromWebUI(web_ui()));
  login_feedback_->Request(
      std::string(), base::BindOnce(&SigninScreenHandler::OnFeedbackFinished,
                                    weak_factory_.GetWeakPtr()));
}

bool SigninScreenHandler::AllAllowlistedUsersPresent() {
  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = false;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  if (allow_new_user)
    return false;
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::UserList& users = user_manager->GetUsers();
  if (!delegate_ || users.size() > kMaxUsers) {
    return false;
  }

  bool allow_family_link = false;
  cros_settings->GetBoolean(kAccountsPrefFamilyLinkAccountsAllowed,
                            &allow_family_link);
  if (allow_family_link)
    return false;

  const base::ListValue* allowlist = nullptr;
  if (!cros_settings->GetList(kAccountsPrefUsers, &allowlist) || !allowlist)
    return false;
  for (size_t i = 0; i < allowlist->GetSize(); ++i) {
    std::string allowlisted_user;
    // NB: Wildcards in the allowlist are also detected as not present here.
    if (!allowlist->GetString(i, &allowlisted_user) ||
        !user_manager->IsKnownUser(
            AccountId::FromUserEmail(allowlisted_user))) {
      return false;
    }
  }
  return true;
}

bool SigninScreenHandler::IsGaiaVisible() const {
  return IsSigninScreen(GetCurrentScreen()) &&
      ui_state_ == UI_STATE_GAIA_SIGNIN;
}

bool SigninScreenHandler::IsGaiaHiddenByError() const {
  return IsSigninScreenHiddenByError() &&
      ui_state_ == UI_STATE_GAIA_SIGNIN;
}

bool SigninScreenHandler::IsSigninScreenHiddenByError() const {
  return (GetCurrentScreen() == ErrorScreenView::kScreenId) &&
         (IsSigninScreen(error_screen_->GetParentScreen()));
}

net::Error SigninScreenHandler::FrameError() const {
  return gaia_screen_handler_->frame_error();
}

void SigninScreenHandler::OnCapsLockChanged(bool enabled) {
  if (page_is_ready())
    CallJS("login.AccountPickerScreen.setCapsLockState", enabled);
}

void SigninScreenHandler::OnFeedbackFinished() {
  login_feedback_.reset();
}

}  // namespace chromeos
