// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"

#include <utility>

#include "ash/public/cpp/child_accounts/parent_access_controller.h"
#include "ash/public/cpp/login/login_utils.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "ash/webui/settings/public/constants/setting.mojom-shared.h"
#include "base/check_is_test.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/help_app_launcher.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/login_auth_recorder.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_display_host_webui.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_dialogs.h"
#include "chrome/browser/ui/webui/ash/login/l10n_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_names.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace {
using ash::SupervisedAction;

LoginScreenClientImpl* g_login_screen_client_instance = nullptr;
}  // namespace

LoginScreenClientImpl::Delegate::Delegate() = default;
LoginScreenClientImpl::Delegate::~Delegate() = default;

LoginScreenClientImpl::ParentAccessDelegate::~ParentAccessDelegate() = default;

LoginScreenClientImpl::LoginScreenClientImpl()
    : auth_recorder_(std::make_unique<ash::LoginAuthRecorder>()) {
  // Register this object as the client interface implementation.
  ash::LoginScreen::Get()->SetClient(this);

  DCHECK(!g_login_screen_client_instance);
  g_login_screen_client_instance = this;

  if (user_manager::UserManager::IsInitialized()) {
    user_manager::UserManager::Get()->AddObserver(this);
  } else {
    CHECK_IS_TEST();
  }
}

LoginScreenClientImpl::~LoginScreenClientImpl() {
  if (user_manager::UserManager::IsInitialized()) {
    user_manager::UserManager::Get()->RemoveObserver(this);
  } else {
    CHECK_IS_TEST();
  }
  ash::LoginScreen::Get()->SetClient(nullptr);
  DCHECK_EQ(this, g_login_screen_client_instance);
  g_login_screen_client_instance = nullptr;
}

// static
bool LoginScreenClientImpl::HasInstance() {
  return !!g_login_screen_client_instance;
}

// static
LoginScreenClientImpl* LoginScreenClientImpl::Get() {
  DCHECK(g_login_screen_client_instance);
  return g_login_screen_client_instance;
}

void LoginScreenClientImpl::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void LoginScreenClientImpl::AddSystemTrayObserver(
    ash::SystemTrayObserver* observer) {
  system_tray_observers_.AddObserver(observer);
}

void LoginScreenClientImpl::RemoveSystemTrayObserver(
    ash::SystemTrayObserver* observer) {
  system_tray_observers_.RemoveObserver(observer);
}

void LoginScreenClientImpl::AddLoginScreenShownObserver(
    LoginScreenShownObserver* observer) {
  login_screen_shown_observers_.AddObserver(observer);
}

void LoginScreenClientImpl::RemoveLoginScreenShownObserver(
    LoginScreenShownObserver* observer) {
  login_screen_shown_observers_.RemoveObserver(observer);
}

ash::LoginAuthRecorder* LoginScreenClientImpl::auth_recorder() {
  return auth_recorder_.get();
}

void LoginScreenClientImpl::AuthenticateUserWithPasswordOrPin(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    base::OnceCallback<void(bool)> callback) {
  if (delegate_) {
    delegate_->HandleAuthenticateUserWithPasswordOrPin(
        account_id, password, authenticated_by_pin, std::move(callback));
    auto auth_method = authenticated_by_pin
                           ? ash::LoginAuthRecorder::AuthMethod::kPin
                           : ash::LoginAuthRecorder::AuthMethod::kPassword;
    auth_recorder_->RecordAuthMethod(auth_method);
  } else {
    LOG(ERROR) << "Failed AuthenticateUserWithPasswordOrPin; no delegate";
    std::move(callback).Run(false);
  }
}

void LoginScreenClientImpl::AuthenticateUserWithEasyUnlock(
    const AccountId& account_id) {
  if (delegate_) {
    delegate_->HandleAuthenticateUserWithEasyUnlock(account_id);
    auth_recorder_->RecordAuthMethod(
        ash::LoginAuthRecorder::AuthMethod::kSmartlock);
  }
}

void LoginScreenClientImpl::AuthenticateUserWithChallengeResponse(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  if (delegate_) {
    delegate_->HandleAuthenticateUserWithChallengeResponse(account_id,
                                                           std::move(callback));
    auth_recorder_->RecordAuthMethod(
        ash::LoginAuthRecorder::AuthMethod::kChallengeResponse);
  }
}

ash::ParentCodeValidationResult LoginScreenClientImpl::ValidateParentAccessCode(
    const AccountId& account_id,
    const std::string& access_code,
    base::Time validation_time) {
  return ash::parent_access::ParentAccessService::Get()
      .ValidateParentAccessCode(account_id, access_code, validation_time);
}

void LoginScreenClientImpl::OnFocusPod(const AccountId& account_id) {
  if (delegate_) {
    delegate_->HandleOnFocusPod(account_id);
  }
}

void LoginScreenClientImpl::FocusLockScreenApps(bool reverse) {
  // If delegate is not set, or it fails to handle focus request, call
  // |HandleFocusLeavingLockScreenApps| so the lock screen service can
  // give focus to the next window in the tab order.
  if (!delegate_ || !delegate_->HandleFocusLockScreenApps(reverse)) {
    ash::LoginScreen::Get()->GetModel()->HandleFocusLeavingLockScreenApps(
        reverse);
  }
}

void LoginScreenClientImpl::FocusOobeDialog() {
  if (delegate_) {
    delegate_->HandleFocusOobeDialog();
  }
}

void LoginScreenClientImpl::ShowGaiaSignin(const AccountId& prefilled_account) {
  MakePreAuthenticationChecks(
      prefilled_account,
      base::BindOnce(&LoginScreenClientImpl::ShowGaiaSigninInternal,
                     weak_ptr_factory_.GetWeakPtr(), prefilled_account));
}

void LoginScreenClientImpl::StartUserRecovery(
    const AccountId& account_to_recover) {
  CHECK(!account_to_recover.empty());
  MakePreAuthenticationChecks(
      account_to_recover,
      base::BindOnce(&LoginScreenClientImpl::StartUserRecoveryInternal,
                     weak_ptr_factory_.GetWeakPtr(), account_to_recover));
}

void LoginScreenClientImpl::MakePreAuthenticationChecks(
    const AccountId& account_id,
    base::OnceClosure continuation) {
  if (time_show_gaia_signin_initiated_.is_null()) {
    time_show_gaia_signin_initiated_ = base::TimeTicks::Now();
  }
  // Check trusted status as a workaround to ensure that device owner id is
  // ready. Device owner ID is necessary for IsApprovalRequired checks.
  auto continuation_split = base::SplitOnceCallback(std::move(continuation));
  const ash::CrosSettingsProvider::TrustedStatus status =
      ash::CrosSettings::Get()->PrepareTrustedValues(
          base::BindOnce(&LoginScreenClientImpl::MakePreAuthenticationChecks,
                         weak_ptr_factory_.GetWeakPtr(), account_id,
                         std::move(continuation_split.second)));
  switch (status) {
    case ash::CrosSettingsProvider::TRUSTED:
      // Owner account ID is available. Record time spent waiting for owner
      // account ID and continue showing Gaia Signin.
      base::UmaHistogramTimes(
          "Ash.Login.ShowGaiaSignin.WaitTime",
          base::TimeTicks::Now() - time_show_gaia_signin_initiated_);
      time_show_gaia_signin_initiated_ = base::TimeTicks();
      break;
    case ash::CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      // Do nothing. This function will be called again when the values are
      // ready.
      return;
    case ash::CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      base::UmaHistogramBoolean("Ash.Login.ShowGaiaSignin.PermanentlyUntrusted",
                                true);
      time_show_gaia_signin_initiated_ = base::TimeTicks();
      return;
  }

  auto supervised_action = account_id.empty() ? SupervisedAction::kAddUser
                                              : SupervisedAction::kReauth;
  if (ash::parent_access::ParentAccessService::Get().IsApprovalRequired(
          supervised_action)) {
    // Show the client native parent access widget and processed to GAIA signin
    // flow in |OnParentAccessValidation| when validation success.
    // On login screen we want to validate parent access code for the
    // device owner. Device owner might be different than the account that
    // requires reauth, so we are passing an empty |account_id|.
    ash::ParentAccessController::Get()->ShowWidget(
        AccountId(),
        base::BindOnce(&LoginScreenClientImpl::OnParentAccessValidation,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(continuation_split.first)),
        supervised_action, false /* extra_dimmer */, base::Time::Now());
  } else {
    std::move(continuation_split.first).Run();
  }
}

void LoginScreenClientImpl::ShowOsInstallScreen() {
  if (ash::LoginDisplayHost::default_host()) {
    ash::LoginDisplayHost::default_host()->ShowOsInstallScreen();
  }
}

void LoginScreenClientImpl::OnRemoveUserWarningShown() {
  ProfileMetrics::LogProfileDeleteUser(
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER_SHOW_WARNING);
}

void LoginScreenClientImpl::RemoveUser(const AccountId& account_id) {
  ProfileMetrics::LogProfileDeleteUser(
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  user_manager::UserManager::Get()->RemoveUser(
      account_id, user_manager::UserRemovalReason::LOCAL_USER_INITIATED);
  if (ash::LoginDisplayHost::default_host()) {
    ash::LoginDisplayHost::default_host()->UpdateAddUserButtonStatus();
  }
}

void LoginScreenClientImpl::LaunchPublicSession(
    const AccountId& account_id,
    const std::string& locale,
    const std::string& input_method) {
  if (delegate_) {
    delegate_->HandleLaunchPublicSession(account_id, locale, input_method);
  }
}

void LoginScreenClientImpl::RequestPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale) {
  ash::GetKeyboardLayoutsForLocale(
      base::BindOnce(&LoginScreenClientImpl::SetPublicSessionKeyboardLayout,
                     weak_ptr_factory_.GetWeakPtr(), account_id, locale),
      locale, ash::input_method::InputMethodManager::Get());
}

void LoginScreenClientImpl::HandleAccelerator(
    ash::LoginAcceleratorAction action) {
  if (ash::LoginDisplayHost::default_host()) {
    ash::LoginDisplayHost::default_host()->HandleAccelerator(action);
  }
}

void LoginScreenClientImpl::ShowAccountAccessHelpApp(
    gfx::NativeWindow parent_window) {
  base::MakeRefCounted<ash::HelpAppLauncher>(parent_window)
      ->ShowHelpTopic(ash::HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
}

void LoginScreenClientImpl::ShowParentAccessHelpApp() {
  // Don't pass in a parent window so that the size of the help dialog is not
  // bounded by its parent window.
  base::MakeRefCounted<ash::HelpAppLauncher>(/*parent_window=*/nullptr)
      ->ShowHelpTopic(ash::HelpAppLauncher::HELP_PARENT_ACCESS_CODE);
}

void LoginScreenClientImpl::ShowLockScreenNotificationSettings() {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetActiveUserProfile(),
      std::string(chromeos::settings::mojom::kSecurityAndSignInSubpagePathV2) +
          "?settingId=" +
          base::NumberToString(static_cast<int>(
              chromeos::settings::mojom::Setting::kLockScreenNotification)));
}

void LoginScreenClientImpl::OnFocusLeavingSystemTray(bool reverse) {
  for (ash::SystemTrayObserver& observer : system_tray_observers_) {
    observer.OnFocusLeavingSystemTray(reverse);
  }
}

void LoginScreenClientImpl::OnSystemTrayBubbleShown() {
  for (ash::SystemTrayObserver& observer : system_tray_observers_) {
    observer.OnSystemTrayBubbleShown();
  }
}

void LoginScreenClientImpl::OnLoginScreenShown() {
  for (LoginScreenShownObserver& observer : login_screen_shown_observers_) {
    observer.OnLoginScreenShown();
  }
}

void LoginScreenClientImpl::CancelAddUser() {
  ash::UserAddingScreen::Get()->Cancel();
}

void LoginScreenClientImpl::LoginAsGuest() {
  DCHECK(!ash::ScreenLocker::default_screen_locker());
  if (ash::LoginDisplayHost::default_host()) {
    ash::LoginDisplayHost::default_host()->GetExistingUserController()->Login(
        ash::UserContext(user_manager::UserType::kGuest,
                         user_manager::GuestAccountId()),
        ash::SigninSpecifics());
  }
}

void LoginScreenClientImpl::ShowGuestTosScreen() {
  // EULA is already accepted on enterprise managed devices.
  // Unmanaged guests on managed devices should login directly without seeing
  // the ToS screen. Managed guest sessions are handled separately.
  if (ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    LoginAsGuest();
    return;
  }

  ash::LoginDisplayHost::default_host()->ShowGuestTosScreen();
}

void LoginScreenClientImpl::OnMaxIncorrectPasswordAttempted(
    const AccountId& account_id) {
  RecordReauthReason(account_id, ash::ReauthReason::kIncorrectPasswordEntered);
}

void LoginScreenClientImpl::SetPublicSessionKeyboardLayout(
    const AccountId& account_id,
    const std::string& locale,
    base::Value::List keyboard_layouts) {
  std::vector<ash::InputMethodItem> result;

  for (const auto& i : keyboard_layouts) {
    if (!i.is_dict()) {
      continue;
    }
    const base::Value::Dict& dict = i.GetDict();

    ash::InputMethodItem input_method_item;
    const std::string* ime_id = dict.FindString("value");
    if (ime_id) {
      input_method_item.ime_id = *ime_id;
    }

    const std::string* title = dict.FindString("title");
    if (title) {
      input_method_item.title = *title;
    }

    input_method_item.selected = dict.FindBool("selected").value_or(false);
    result.push_back(std::move(input_method_item));
  }
  ash::LoginScreen::Get()->GetModel()->SetPublicSessionKeyboardLayouts(
      account_id, locale, result);
}

views::Widget* LoginScreenClientImpl::GetLoginWindowWidget() {
  if (ash::LoginDisplayHost::default_host()) {
    return ash::LoginDisplayHost::default_host()->GetLoginWindowWidget();
  }
  return nullptr;
}

void LoginScreenClientImpl::OnUserImageChanged(const user_manager::User& user) {
  ash::LoginScreen::Get()->GetModel()->SetAvatarForUser(
      user.GetAccountId(), ash::BuildAshUserAvatarForUser(user));
}

void LoginScreenClientImpl::OnParentAccessValidation(
    base::OnceClosure continuation,
    bool success) {
  if (success) {
    std::move(continuation).Run();
  }
}

void LoginScreenClientImpl::ShowGaiaSigninInternal(
    const AccountId& prefilled_account) {
  // It is possible that the call will come during the session start and after
  // the LoginDisplayHost destruction. Ignore such calls.
  if (ash::LoginDisplayHost::default_host()) {
    ash::LoginDisplayHost::default_host()->ShowGaiaDialog(prefilled_account);
  } else if (session_manager::SessionManager::Get()->session_state() ==
             session_manager::SessionState::LOCKED) {
    ash::LockScreenStartReauthDialog::Show();
  } else {
    // TODO(b/332715260): In general this shouldn't happen, however there might
    // be transition states when pending calls still arrive. It should be safe
    // to remove the DumpWithoutCrashing if the number of reports will be low.
    base::debug::DumpWithoutCrashing();
    LOG(WARNING) << __func__ << ": ignoring the call, session state: "
                 << static_cast<int>(session_manager::SessionManager::Get()
                                         ->session_state());
  }
}

void LoginScreenClientImpl::StartUserRecoveryInternal(
    const AccountId& account_to_recover) {
  CHECK(ash::LoginDisplayHost::default_host())
      << "Recovery is not supported on the lock screen";
  ash::LoginDisplayHost::default_host()->StartUserRecovery(account_to_recover);
}
