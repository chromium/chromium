// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_DISPLAY_HOST_MOJO_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_DISPLAY_HOST_MOJO_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/challenge_response_auth_keys_loader.h"
#include "chrome/browser/ash/login/screens/user_selection_screen.h"
#include "chrome/browser/ash/login/security_token_pin_dialog_host_login_impl.h"
#include "chrome/browser/ui/ash/login/login_display_host_common.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chrome/browser/ui/ash/login/oobe_ui_dialog_delegate.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"
#include "components/user_manager/user.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class View;
}  // namespace views

namespace ash {
class ExistingUserController;
class MojoSystemInfoDispatcher;
class OobeUIDialogDelegate;
class WizardController;

// A LoginDisplayHost instance that sends requests to the views-based signin
// screen.
class LoginDisplayHostMojo : public LoginDisplayHostCommon,
                             public ::LoginScreenClientImpl::Delegate,
                             public AuthStatusConsumer,
                             public OobeUI::Observer,
                             public views::ViewObserver,
                             public ui::UserActivityObserver {
 public:
  explicit LoginDisplayHostMojo(DisplayedScreen displayed_screen);

  LoginDisplayHostMojo(const LoginDisplayHostMojo&) = delete;
  LoginDisplayHostMojo& operator=(const LoginDisplayHostMojo&) = delete;

  ~LoginDisplayHostMojo() override;

  static LoginDisplayHostMojo* Get();

  // Called when the gaia dialog is destroyed.
  void OnDialogDestroyed(const OobeUIDialogDelegate* dialog);

  void SetUsers(const user_manager::UserList& users);

  UserSelectionScreen* user_selection_screen() {
    return user_selection_screen_.get();
  }

  // LoginDisplayHost:
  ExistingUserController* GetExistingUserController() override;
  gfx::NativeWindow GetNativeWindow() const override;
  views::Widget* GetLoginWindowWidget() const override;
  OobeUI* GetOobeUI() const override;
  content::WebContents* GetOobeWebContents() const override;
  WebUILoginView* GetWebUILoginView() const override;
  void OnFinalize() override;
  void StartWizard(OobeScreenId first_screen) override;
  WizardController* GetWizardController() override;
  void OnStartUserAdding() override;
  void CancelUserAdding() override;
  void OnStartSignInScreen() override;
  void OnStartAppLaunch() override;
  void OnBrowserCreated() override;
  void ShowGaiaDialog(const AccountId& prefilled_account) override;
  void StartUserRecovery(const AccountId& account_to_recover) override;
  void ShowOsInstallScreen() override;
  void ShowGuestTosScreen() override;
  void ShowRemoteActivityNotificationScreen() override;
  void HideOobeDialog(bool saml_page_closed = false) override;
  void SetShelfButtonsEnabled(bool enabled) override;
  void UpdateOobeDialogState(OobeDialogState state) override;
  void OnCancelPasswordChangedFlow() override;
  void HandleDisplayCaptivePortal() override;
  void UpdateAddUserButtonStatus() override;
  void RequestSystemInfoUpdate() override;
  bool HasUserPods() override;
  void UseAlternativeAuthentication(std::unique_ptr<UserContext> user_context,
                                    bool online_password_mismatch) override;
  void RunLocalAuthentication(
      std::unique_ptr<UserContext> user_context) override;
  void AddObserver(LoginDisplayHost::Observer* observer) override;
  void RemoveObserver(LoginDisplayHost::Observer* observer) override;
  SigninUI* GetSigninUI() final;
  bool IsWizardControllerCreated() const final;
  bool GetKeyboardRemappedPrefValue(const std::string& pref_name,
                                    int* value) const final;
  bool IsWebUIStarted() const final;

  // LoginDisplayHostCommon:
  bool HandleAccelerator(LoginAcceleratorAction action) final;

  // LoginScreenClientImpl::Delegate:
  void HandleAuthenticateUserWithPasswordOrPin(
      const AccountId& account_id,
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) override;
  void HandleAuthenticateUserWithEasyUnlock(
      const AccountId& account_id) override;
  void HandleAuthenticateUserWithChallengeResponse(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void HandleOnFocusPod(const AccountId& account_id) override;
  bool HandleFocusLockScreenApps(bool reverse) override;
  void HandleFocusOobeDialog() override;
  void HandleLaunchPublicSession(const AccountId& account_id,
                                 const std::string& locale,
                                 const std::string& input_method) override;

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;
  void OnPasswordChangeDetectedFor(const AccountId& account) override;
  void OnOldEncryptionDetected(std::unique_ptr<UserContext> user_context,
                               bool has_incomplete_migration) override;

  // OobeUI::Observer:
  void OnCurrentScreenChanged(OobeScreenId current_screen,
                              OobeScreenId new_screen) override;
  void OnDestroyingOobeUI() override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  bool IsOobeUIDialogVisible() const override;

  OobeUIDialogDelegate* EnsureDialogForTest();

 private:
  // Ensure GetOobeUI() is not nullptr.
  void EnsureOobeDialogLoaded();

  // Callback to be invoked when the `challenge_response_auth_keys_loader_`
  // completes building the currently available challenge-response keys. Used
  // only during the challenge-response authentication.
  void OnChallengeResponseKeysPrepared(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> on_auth_complete_callback,
      std::vector<ChallengeResponseKey> challenge_response_keys);

  // Helper methods to show and hide the dialog.
  void ShowDialog();
  void ShowFullScreen();
  void HideDialog();

  // Common part for ShowGaiaDialog/StartUserRecovery.
  void ShowGaiaDialogImpl(const AccountId& prefilled_account);

  // Adds this as a `OobeUI::Observer` if it has not already been added as one.
  void ObserveOobeUI();

  // Removes this as a `OobeUI::Observer` if it has been added as an observer.
  void StopObservingOobeUI();

  // Create ExistingUserController and link it to LoginDisplayHostMojo so we can
  // consume auth status events.
  void CreateExistingUserController();

  // Result callback for local authentication dialog.
  void OnLocalAuthenticationCompleted(
      bool success,
      std::unique_ptr<UserContext> user_context);

  // Sets an extra flag that can hide/unhide offline login link if the offline
  // login timer has expired for a focused user.
  void MaybeUpdateOfflineLoginLinkVisibility(const AccountId& account_id);

  // ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

  void OnDeviceSettingsChanged();

  // Starts `AuthHub` in login mode.
  void ScheduleStartAuthHubInLoginMode();
  void StartAuthHubInLoginMode(bool is_cryptohome_available);

  // Checks the auth factors availability and updates the user pod.
  void UpdateAuthFactorsAvailability(const user_manager::User* user);
  void OnAuthSessionStarted(bool user_exists,
                            std::unique_ptr<ash::UserContext> user_context,
                            std::optional<ash::AuthenticationError> error);

  base::ObserverList<LoginDisplayHost::Observer> observers_;

  // State associated with a pending authentication attempt.
  struct AuthState {
    AuthState(AccountId account_id, base::OnceCallback<void(bool)> callback);
    ~AuthState();

    // Account that is being authenticated.
    AccountId account_id;
    // Callback that should be executed the authentication result is available.
    base::OnceCallback<void(bool)> callback;
  };
  std::unique_ptr<AuthState> pending_auth_state_;

  std::unique_ptr<UserSelectionScreen> user_selection_screen_;

  base::CallbackListSubscription allow_new_user_subscription_;

  std::unique_ptr<ExistingUserController> existing_user_controller_;

  AuthPerformer auth_performer_;

  // Called after host deletion.
  std::vector<base::OnceClosure> completion_callbacks_;
  raw_ptr<OobeUIDialogDelegate> dialog_ = nullptr;  // Not owned.
  std::unique_ptr<WizardController> wizard_controller_;

  // Whether or not there are users that are visible in the views login screen.
  bool has_user_pods_ = false;

  // The account id of the user pod that's being focused.
  AccountId focused_pod_account_id_;

  // Fetches system information and sends it to the UI over mojo.
  std::unique_ptr<MojoSystemInfoDispatcher> system_info_updater_;

  // Prevents repeated calls to OnStartSigninScreen, which can happen when a
  // user cancels the Powerwash dialog in the login screen. Set to true on the
  // first OnStartSigninScreen and remains true afterward.
  bool signin_screen_started_ = false;

  ChallengeResponseAuthKeysLoader challenge_response_auth_keys_loader_;

  SecurityTokenPinDialogHostLoginImpl
      security_token_pin_dialog_host_login_impl_;

  // Set if this has been added as a `OobeUI::Observer`.
  bool added_as_oobe_observer_ = false;

  bool initialized_ = false;

  // Set if Gaia dialog is shown with prefilled email.
  std::optional<AccountId> gaia_reauth_account_id_;

  base::ScopedObservation<views::View, views::ViewObserver> scoped_observation_{
      this};

  base::ScopedObservation<ui::UserActivityDetector, ui::UserActivityObserver>
      scoped_activity_observation_{this};

  base::WeakPtrFactory<LoginDisplayHostMojo> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_DISPLAY_HOST_MOJO_H_
