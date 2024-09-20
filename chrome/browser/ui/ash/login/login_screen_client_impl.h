// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_CLIENT_IMPL_H_

#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_screen_client.h"
#include "ash/system/tray/system_tray_observer.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/experiences/login/login_screen_shown_observer.h"
#include "components/user_manager/user_manager.h"

namespace ash {
enum class ParentCodeValidationResult;
class LoginAuthRecorder;
}  // namespace ash

// Handles method calls sent from ash to chrome. Also sends messages from chrome
// to ash.
class LoginScreenClientImpl : public ash::LoginScreenClient,
                              public user_manager::UserManager::Observer {
 public:
  // Handles method calls coming from ash into chrome.
  class Delegate {
   public:
    Delegate();

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate();
    virtual void HandleAuthenticateUserWithPasswordOrPin(
        const AccountId& account_id,
        const std::string& password,
        bool authenticated_by_pin,
        base::OnceCallback<void(bool)> callback) = 0;
    virtual void HandleAuthenticateUserWithEasyUnlock(
        const AccountId& account_id) = 0;
    virtual void HandleAuthenticateUserWithChallengeResponse(
        const AccountId& account_id,
        base::OnceCallback<void(bool)> callback) = 0;
    virtual void HandleOnFocusPod(const AccountId& account_id) = 0;
    // Handles request to focus a lock screen app window. Returns whether the
    // focus has been handed over to a lock screen app. For example, this might
    // fail if a hander for lock screen apps focus has not been set.
    virtual bool HandleFocusLockScreenApps(bool reverse) = 0;
    virtual void HandleFocusOobeDialog() = 0;
    virtual void HandleLaunchPublicSession(const AccountId& account_id,
                                           const std::string& locale,
                                           const std::string& input_method) = 0;
  };

  // Handles methods related to parent access coming from ash into chrome.
  class ParentAccessDelegate {
   public:
    virtual ~ParentAccessDelegate();

    virtual bool ValidateParentAccessCode(const std::string& access_code) = 0;
  };

  LoginScreenClientImpl();

  LoginScreenClientImpl(const LoginScreenClientImpl&) = delete;
  LoginScreenClientImpl& operator=(const LoginScreenClientImpl&) = delete;

  ~LoginScreenClientImpl() override;
  static bool HasInstance();
  static LoginScreenClientImpl* Get();

  // Set the object which will handle calls coming from ash.
  void SetDelegate(Delegate* delegate);

  ash::LoginAuthRecorder* auth_recorder();

  void AddSystemTrayObserver(ash::SystemTrayObserver* observer);
  void RemoveSystemTrayObserver(ash::SystemTrayObserver* observer);

  void AddLoginScreenShownObserver(LoginScreenShownObserver* observer);
  void RemoveLoginScreenShownObserver(LoginScreenShownObserver* observer);

  // ash::LoginScreenClient:
  void AuthenticateUserWithPasswordOrPin(
      const AccountId& account_id,
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) override;
  void AuthenticateUserWithEasyUnlock(const AccountId& account_id) override;
  void AuthenticateUserWithChallengeResponse(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  ash::ParentCodeValidationResult ValidateParentAccessCode(
      const AccountId& account_id,
      const std::string& access_code,
      base::Time validation_time) override;
  void OnFocusPod(const AccountId& account_id) override;
  void CancelAddUser() override;
  void ShowGuestTosScreen() override;
  void OnMaxIncorrectPasswordAttempted(const AccountId& account_id) override;
  void FocusLockScreenApps(bool reverse) override;
  void FocusOobeDialog() override;
  void ShowGaiaSignin(const AccountId& prefilled_account) override;
  void StartUserRecovery(const AccountId& account_to_recover) override;
  void ShowOsInstallScreen() override;
  void OnRemoveUserWarningShown() override;
  void RemoveUser(const AccountId& account_id) override;
  void LaunchPublicSession(const AccountId& account_id,
                           const std::string& locale,
                           const std::string& input_method) override;
  void RequestPublicSessionKeyboardLayouts(const AccountId& account_id,
                                           const std::string& locale) override;
  void HandleAccelerator(ash::LoginAcceleratorAction action) override;
  void ShowAccountAccessHelpApp(gfx::NativeWindow parent_window) override;
  void ShowParentAccessHelpApp() override;
  void ShowLockScreenNotificationSettings() override;
  void OnFocusLeavingSystemTray(bool reverse) override;
  void OnSystemTrayBubbleShown() override;
  void OnLoginScreenShown() override;
  views::Widget* GetLoginWindowWidget() override;

  // user_manager::UserManager::Observer:
  void OnUserImageChanged(const user_manager::User& user) override;

 private:
  void LoginAsGuest();
  void SetPublicSessionKeyboardLayout(const AccountId& account_id,
                                      const std::string& locale,
                                      base::Value::List keyboard_layouts);

  void MakePreAuthenticationChecks(const AccountId& account_id,
                                   base::OnceClosure continuation);

  // Called when the parent access code was validated with result equals
  // |success|.
  void OnParentAccessValidation(base::OnceClosure continuation, bool success);

  void ShowGaiaSigninInternal(const AccountId& prefilled_account);
  void StartUserRecoveryInternal(const AccountId& account_to_recover);

  raw_ptr<Delegate> delegate_ = nullptr;

  // Captures authentication related user metrics for login screen.
  std::unique_ptr<ash::LoginAuthRecorder> auth_recorder_;

  base::ObserverList<ash::SystemTrayObserver>::Unchecked system_tray_observers_;

  base::ObserverList<LoginScreenShownObserver> login_screen_shown_observers_;

  base::TimeTicks time_show_gaia_signin_initiated_;

  base::WeakPtrFactory<LoginScreenClientImpl> weak_ptr_factory_{this};
};

namespace base {

template <>
struct ScopedObservationTraits<LoginScreenClientImpl, ash::SystemTrayObserver> {
  static void AddObserver(LoginScreenClientImpl* source,
                          ash::SystemTrayObserver* observer) {
    source->AddSystemTrayObserver(observer);
  }
  static void RemoveObserver(LoginScreenClientImpl* source,
                             ash::SystemTrayObserver* observer) {
    source->RemoveSystemTrayObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_SCREEN_CLIENT_IMPL_H_
