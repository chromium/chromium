// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_SCREEN_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_SCREEN_CLIENT_H_

#include "ash/public/cpp/login_screen_client.h"
#include "ash/public/cpp/system_tray_focus_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace base {
class ListValue;
}

namespace chromeos {
class LoginAuthRecorder;
}

// Handles method calls sent from ash to chrome. Also sends messages from chrome
// to ash.
class LoginScreenClient : public ash::LoginScreenClient {
 public:
  // Handles method calls coming from ash into chrome.
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();
    virtual void HandleAuthenticateUserWithPasswordOrPin(
        const AccountId& account_id,
        const std::string& password,
        bool authenticated_by_pin,
        base::OnceCallback<void(bool)> callback) = 0;
    virtual void HandleAuthenticateUserWithExternalBinary(
        const AccountId& account_id,
        base::OnceCallback<void(bool)> callback) = 0;
    virtual void HandleEnrollUserWithExternalBinary(
        base::OnceCallback<void(bool)> callback) = 0;
    virtual void HandleAuthenticateUserWithEasyUnlock(
        const AccountId& account_id) = 0;
    virtual void HandleAuthenticateUserWithChallengeResponse(
        const AccountId& account_id,
        base::OnceCallback<void(bool)> callback) = 0;
    virtual void HandleHardlockPod(const AccountId& account_id) = 0;
    virtual void HandleOnFocusPod(const AccountId& account_id) = 0;
    virtual void HandleOnNoPodFocused() = 0;
    // Handles request to focus a lock screen app window. Returns whether the
    // focus has been handed over to a lock screen app. For example, this might
    // fail if a hander for lock screen apps focus has not been set.
    virtual bool HandleFocusLockScreenApps(bool reverse) = 0;
    virtual void HandleFocusOobeDialog() = 0;
    virtual void HandleLaunchPublicSession(const AccountId& account_id,
                                           const std::string& locale,
                                           const std::string& input_method) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Handles methods related to parent access coming from ash into chrome.
  class ParentAccessDelegate {
   public:
    virtual ~ParentAccessDelegate();

    virtual bool ValidateParentAccessCode(const std::string& access_code) = 0;
  };

  LoginScreenClient();
  ~LoginScreenClient() override;
  static bool HasInstance();
  static LoginScreenClient* Get();

  // Set the object which will handle calls coming from ash.
  void SetDelegate(Delegate* delegate);

  chromeos::LoginAuthRecorder* auth_recorder();

  void AddSystemTrayFocusObserver(ash::SystemTrayFocusObserver* observer);
  void RemoveSystemTrayFocusObserver(ash::SystemTrayFocusObserver* observer);

  // ash::LoginScreenClient:
  void AuthenticateUserWithPasswordOrPin(
      const AccountId& account_id,
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) override;
  void AuthenticateUserWithExternalBinary(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void EnrollUserWithExternalBinary(
      base::OnceCallback<void(bool)> callback) override;
  void AuthenticateUserWithEasyUnlock(const AccountId& account_id) override;
  void AuthenticateUserWithChallengeResponse(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  bool ValidateParentAccessCode(const AccountId& account_id,
                                const std::string& access_code,
                                base::Time validation_time) override;
  void HardlockPod(const AccountId& account_id) override;
  void OnFocusPod(const AccountId& account_id) override;
  void OnNoPodFocused() override;
  void LoadWallpaper(const AccountId& account_id) override;
  void SignOutUser() override;
  void CancelAddUser() override;
  void LoginAsGuest() override;
  void OnMaxIncorrectPasswordAttempted(const AccountId& account_id) override;
  void FocusLockScreenApps(bool reverse) override;
  void FocusOobeDialog() override;
  void ShowGaiaSignin(bool can_close,
                      const AccountId& prefilled_account) override;
  void OnRemoveUserWarningShown() override;
  void RemoveUser(const AccountId& account_id) override;
  void LaunchPublicSession(const AccountId& account_id,
                           const std::string& locale,
                           const std::string& input_method) override;
  void RequestPublicSessionKeyboardLayouts(const AccountId& account_id,
                                           const std::string& locale) override;
  void ShowFeedback() override;
  void ShowResetScreen() override;
  void ShowAccountAccessHelpApp() override;
  void ShowParentAccessHelpApp() override;
  void ShowLockScreenNotificationSettings() override;
  void OnFocusLeavingSystemTray(bool reverse) override;
  void OnUserActivity() override;

 private:
  void SetPublicSessionKeyboardLayout(
      const AccountId& account_id,
      const std::string& locale,
      std::unique_ptr<base::ListValue> keyboard_layouts);

  Delegate* delegate_ = nullptr;

  // Captures authentication related user metrics for login screen.
  std::unique_ptr<chromeos::LoginAuthRecorder> auth_recorder_;

  base::ObserverList<ash::SystemTrayFocusObserver>::Unchecked
      system_tray_focus_observers_;

  base::WeakPtrFactory<LoginScreenClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginScreenClient);
};

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_SCREEN_CLIENT_H_
