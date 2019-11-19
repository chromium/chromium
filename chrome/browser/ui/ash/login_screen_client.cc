// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login_screen_client.h"

#include <utility>

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_types.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/login_auth_recorder.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/common/webui_url_constants.h"
#include "components/user_manager/remove_user_delegate.h"
#include "components/user_manager/user_names.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {
LoginScreenClient* g_login_screen_client_instance = nullptr;
}  // namespace

LoginScreenClient::Delegate::Delegate() = default;
LoginScreenClient::Delegate::~Delegate() = default;

LoginScreenClient::ParentAccessDelegate::~ParentAccessDelegate() = default;

LoginScreenClient::LoginScreenClient()
    : auth_recorder_(std::make_unique<chromeos::LoginAuthRecorder>()) {
  // Register this object as the client interface implementation.
  ash::LoginScreen::Get()->SetClient(this);

  DCHECK(!g_login_screen_client_instance);
  g_login_screen_client_instance = this;
}

LoginScreenClient::~LoginScreenClient() {
  ash::LoginScreen::Get()->SetClient(nullptr);
  DCHECK_EQ(this, g_login_screen_client_instance);
  g_login_screen_client_instance = nullptr;
}

// static
bool LoginScreenClient::HasInstance() {
  return !!g_login_screen_client_instance;
}

// static
LoginScreenClient* LoginScreenClient::Get() {
  DCHECK(g_login_screen_client_instance);
  return g_login_screen_client_instance;
}

void LoginScreenClient::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void LoginScreenClient::AddSystemTrayFocusObserver(
    ash::SystemTrayFocusObserver* observer) {
  system_tray_focus_observers_.AddObserver(observer);
}

void LoginScreenClient::RemoveSystemTrayFocusObserver(
    ash::SystemTrayFocusObserver* observer) {
  system_tray_focus_observers_.RemoveObserver(observer);
}

chromeos::LoginAuthRecorder* LoginScreenClient::auth_recorder() {
  return auth_recorder_.get();
}

void LoginScreenClient::AuthenticateUserWithPasswordOrPin(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    base::OnceCallback<void(bool)> callback) {
  if (delegate_) {
    delegate_->HandleAuthenticateUserWithPasswordOrPin(
        account_id, password, authenticated_by_pin, std::move(callback));
    auth_recorder_->RecordAuthMethod(
        authenticated_by_pin
            ? chromeos::LoginAuthRecorder::AuthMethod::kPin
            : chromeos::LoginAuthRecorder::AuthMethod::kPassword);
  } else {
    LOG(ERROR) << "Failed AuthenticateUserWithPasswordOrPin; no delegate";
    std::move(callback).Run(false);
  }
}

void LoginScreenClient::AuthenticateUserWithExternalBinary(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  if (!delegate_)
    LOG(FATAL) << "Failed AuthenticateUserWithExternalBinary; no delegate";

  delegate_->HandleAuthenticateUserWithExternalBinary(account_id,
                                                      std::move(callback));
  // TODO: Record auth method attempt here
  NOTIMPLEMENTED() << "Missing UMA recording for external binary auth";
}

void LoginScreenClient::EnrollUserWithExternalBinary(
    base::OnceCallback<void(bool)> callback) {
  if (!delegate_)
    LOG(FATAL) << "Failed EnrollUserWithExternalBinary; no delegate";

  delegate_->HandleEnrollUserWithExternalBinary(std::move(callback));

  // TODO: Record enrollment attempt here
  NOTIMPLEMENTED() << "Missing UMA recording for external binary enrollment";
}

void LoginScreenClient::AuthenticateUserWithEasyUnlock(
    const AccountId& account_id) {
  if (delegate_) {
    delegate_->HandleAuthenticateUserWithEasyUnlock(account_id);
    auth_recorder_->RecordAuthMethod(
        chromeos::LoginAuthRecorder::AuthMethod::kSmartlock);
  }
}

void LoginScreenClient::AuthenticateUserWithChallengeResponse(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  if (delegate_) {
    delegate_->HandleAuthenticateUserWithChallengeResponse(account_id,
                                                           std::move(callback));
    auth_recorder_->RecordAuthMethod(
        chromeos::LoginAuthRecorder::AuthMethod::kChallengeResponse);
  }
}

bool LoginScreenClient::ValidateParentAccessCode(const AccountId& account_id,
                                                 const std::string& access_code,
                                                 base::Time validation_time) {
  return chromeos::parent_access::ParentAccessService::Get()
      .ValidateParentAccessCode(account_id, access_code, validation_time);
}

void LoginScreenClient::HardlockPod(const AccountId& account_id) {
  if (delegate_)
    delegate_->HandleHardlockPod(account_id);
}

void LoginScreenClient::OnFocusPod(const AccountId& account_id) {
  if (delegate_)
    delegate_->HandleOnFocusPod(account_id);
}

void LoginScreenClient::OnNoPodFocused() {
  if (delegate_)
    delegate_->HandleOnNoPodFocused();
}

void LoginScreenClient::FocusLockScreenApps(bool reverse) {
  // If delegate is not set, or it fails to handle focus request, call
  // |HandleFocusLeavingLockScreenApps| so the lock screen service can
  // give focus to the next window in the tab order.
  if (!delegate_ || !delegate_->HandleFocusLockScreenApps(reverse)) {
    ash::LoginScreen::Get()->GetModel()->HandleFocusLeavingLockScreenApps(
        reverse);
  }
}

void LoginScreenClient::FocusOobeDialog() {
  if (delegate_)
    delegate_->HandleFocusOobeDialog();
}

void LoginScreenClient::ShowGaiaSignin(bool can_close,
                                       const AccountId& prefilled_account) {
  if (chromeos::LoginDisplayHost::default_host()) {
    chromeos::LoginDisplayHost::default_host()->ShowGaiaDialog(
        can_close, prefilled_account);
  }
}

void LoginScreenClient::OnRemoveUserWarningShown() {
  ProfileMetrics::LogProfileDeleteUser(
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER_SHOW_WARNING);
}

void LoginScreenClient::RemoveUser(const AccountId& account_id) {
  ProfileMetrics::LogProfileDeleteUser(
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  user_manager::UserManager::Get()->RemoveUser(account_id,
                                               nullptr /*delegate*/);
  if (chromeos::LoginDisplayHost::default_host())
    chromeos::LoginDisplayHost::default_host()->UpdateAddUserButtonStatus();
}

void LoginScreenClient::LaunchPublicSession(const AccountId& account_id,
                                            const std::string& locale,
                                            const std::string& input_method) {
  if (delegate_)
    delegate_->HandleLaunchPublicSession(account_id, locale, input_method);
}

void LoginScreenClient::RequestPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale) {
  chromeos::GetKeyboardLayoutsForLocale(
      base::BindRepeating(&LoginScreenClient::SetPublicSessionKeyboardLayout,
                          weak_ptr_factory_.GetWeakPtr(), account_id, locale),
      locale);
}

void LoginScreenClient::ShowFeedback() {
  if (chromeos::LoginDisplayHost::default_host())
    chromeos::LoginDisplayHost::default_host()->ShowFeedback();
}

void LoginScreenClient::ShowResetScreen() {
  chromeos::LoginDisplayHost::default_host()->ShowResetScreen();
}

void LoginScreenClient::ShowAccountAccessHelpApp() {
  scoped_refptr<chromeos::HelpAppLauncher>(
      new chromeos::HelpAppLauncher(nullptr))
      ->ShowHelpTopic(chromeos::HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
}

void LoginScreenClient::ShowParentAccessHelpApp() {
  scoped_refptr<chromeos::HelpAppLauncher>(
      new chromeos::HelpAppLauncher(nullptr))
      ->ShowHelpTopic(chromeos::HelpAppLauncher::HELP_PARENT_ACCESS_CODE);
}

void LoginScreenClient::ShowLockScreenNotificationSettings() {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ProfileManager::GetActiveUserProfile(), chrome::kLockScreenSubPage);
}

void LoginScreenClient::OnFocusLeavingSystemTray(bool reverse) {
  for (ash::SystemTrayFocusObserver& observer : system_tray_focus_observers_)
    observer.OnFocusLeavingSystemTray(reverse);
}

void LoginScreenClient::LoadWallpaper(const AccountId& account_id) {
  WallpaperControllerClient::Get()->ShowUserWallpaper(account_id);
}

void LoginScreenClient::SignOutUser() {
  chromeos::ScreenLocker::default_screen_locker()->Signout();
}

void LoginScreenClient::CancelAddUser() {
  chromeos::UserAddingScreen::Get()->Cancel();
}

void LoginScreenClient::LoginAsGuest() {
  DCHECK(!chromeos::ScreenLocker::default_screen_locker());
  if (chromeos::LoginDisplayHost::default_host()) {
    chromeos::LoginDisplayHost::default_host()
        ->GetExistingUserController()
        ->Login(chromeos::UserContext(user_manager::USER_TYPE_GUEST,
                                      user_manager::GuestAccountId()),
                chromeos::SigninSpecifics());
  }
}

void LoginScreenClient::OnMaxIncorrectPasswordAttempted(
    const AccountId& account_id) {
  RecordReauthReason(account_id,
                     chromeos::ReauthReason::INCORRECT_PASSWORD_ENTERED);
}

void LoginScreenClient::SetPublicSessionKeyboardLayout(
    const AccountId& account_id,
    const std::string& locale,
    std::unique_ptr<base::ListValue> keyboard_layouts) {
  std::vector<ash::InputMethodItem> result;

  for (const auto& i : *keyboard_layouts) {
    const base::DictionaryValue* dictionary;
    if (!i.GetAsDictionary(&dictionary))
      continue;

    ash::InputMethodItem input_method_item;
    std::string ime_id;
    dictionary->GetString("value", &ime_id);
    input_method_item.ime_id = ime_id;

    std::string title;
    dictionary->GetString("title", &title);
    input_method_item.title = title;

    bool selected;
    dictionary->GetBoolean("selected", &selected);
    input_method_item.selected = selected;
    result.push_back(std::move(input_method_item));
  }
  ash::LoginScreen::Get()->GetModel()->SetPublicSessionKeyboardLayouts(
      account_id, locale, result);
}

void LoginScreenClient::OnUserActivity() {
  if (chromeos::LoginDisplayHost::default_host()) {
    chromeos::LoginDisplayHost::default_host()
        ->GetExistingUserController()
        ->ResetAutoLoginTimer();
  }
}
