// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_DISPLAY_HOST_COMMON_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_DISPLAY_HOST_COMMON_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/login_accelerators.h"
#include "base/callback_list.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/tpm/tpm_firmware_update.h"
#include "chrome/browser/ui/ash/login/kiosk_app_menu_controller.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_ui_pref_controller.h"
#include "chrome/browser/ui/ash/login/signin_ui.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/user_manager/user_type.h"

class AccountId;

namespace ash {

class LoginFeedback;
class OobeMetricsHelper;
class OobeCrosEventsMetrics;

// LoginDisplayHostCommon contains code which is not specific to a particular UI
// implementation - the goal is to reduce code duplication between
// LoginDisplayHostMojo and LoginDisplayHostWebUI.
class LoginDisplayHostCommon : public LoginDisplayHost,
                               public BrowserListObserver,
                               public SigninUI {
 public:
  LoginDisplayHostCommon();

  LoginDisplayHostCommon(const LoginDisplayHostCommon&) = delete;
  LoginDisplayHostCommon& operator=(const LoginDisplayHostCommon&) = delete;

  ~LoginDisplayHostCommon() override;

  // LoginDisplayHost:
  void BeforeSessionStart() final;
  bool IsFinalizing() final;
  void Finalize(base::OnceClosure completion_callback) final;
  void FinalizeImmediately() final;
  void StartUserAdding(base::OnceClosure completion_callback) final;
  void StartSignInScreen() final;
  void StartKiosk(const KioskAppId& kiosk_app_id, bool is_auto_launch) final;
  void CompleteLogin(const UserContext& user_context) final;
  void OnGaiaScreenReady() final;
  void SetDisplayEmail(const std::string& email) final;
  void ShowAllowlistCheckFailedError() final;
  void UpdateWallpaper(const AccountId& prefilled_account) final;
  bool IsUserAllowlisted(
      const AccountId& account_id,
      const std::optional<user_manager::UserType>& user_type) final;
  void CancelPasswordChangedFlow() final;
  void AddWizardCreatedObserverForTests(
      base::RepeatingClosure on_created) final;
  base::WeakPtr<quick_start::TargetDeviceBootstrapController>
  GetQuickStartBootstrapController() final;
  // Most of the accelerators are handled in a same way, but not all.
  bool HandleAccelerator(LoginAcceleratorAction action) override;

  // SigninUI:
  void SetAuthSessionForOnboarding(const UserContext& user_context) final;
  void ClearOnboardingAuthSession() final;
  void StartUserOnboarding() final;
  void ResumeUserOnboarding(const PrefService& prefs,
                            OobeScreenId screen_id) final;
  void StartManagementTransition() final;
  void ShowTosForExistingUser() final;
  void ShowNewTermsForFlexUsers() final;
  void StartEncryptionMigration(
      std::unique_ptr<UserContext> user_context,
      EncryptionMigrationMode migration_mode,
      base::OnceCallback<void(std::unique_ptr<UserContext>)> on_skip_migration)
      final;
  void ShowSigninError(SigninError error, const std::string& details) final;
  void SAMLConfirmPassword(::login::StringList scraped_passwords,
                           std::unique_ptr<UserContext> user_context) final;
  WizardContext* GetWizardContextForTesting() final;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

  WizardContext* GetWizardContext() override;

  OobeMetricsHelper* GetOobeMetricsHelper() override;

 protected:
  virtual void OnStartSignInScreen() = 0;
  virtual void OnStartAppLaunch() = 0;
  virtual void OnBrowserCreated() = 0;
  virtual void OnStartUserAdding() = 0;
  virtual void OnFinalize() = 0;
  virtual void OnCancelPasswordChangedFlow() = 0;

  // This function needed to isolate error messages on the Views and WebUI side.
  virtual bool IsOobeUIDialogVisible() const = 0;

  // Marks display host for deletion.
  void ShutdownDisplayHost();

  // Common code for OnStartSignInScreen() call above.
  void OnStartSignInScreenCommon();

  // Common code for ShowGaiaDialog() call above.
  void ShowGaiaDialogCommon(const AccountId& prefilled_account);

  // Triggers |on_wizard_controller_created_for_tests_| callback.
  void NotifyWizardCreated();

 private:
  void Cleanup();
  // Set screen, from which WC flow will continue after attempt to show
  // TermsOfServiceScreen.
  void SetScreenAfterManagedTos(OobeScreenId screen_id);

  void OnPowerwashAllowedCallback(
      bool is_reset_allowed,
      std::optional<tpm_firmware_update::Mode> tpm_firmware_update_mode);

  void OnAppTerminating();

  // True if session start is in progress.
  bool session_starting_ = false;

  // Has ShutdownDisplayHost() already been called?  Used to avoid posting our
  // own deletion to the message loop twice if the user logs out while we're
  // still in the process of cleaning up after login (http://crbug.com/134463).
  bool shutting_down_ = false;

  // Used to make sure Finalize() is not called twice.
  bool is_finalizing_ = false;

  // Make sure chrome won't exit while we are at login/oobe screen.
  ScopedKeepAlive keep_alive_;

  KioskAppMenuController kiosk_app_menu_controller_;

  std::unique_ptr<LoginFeedback> login_feedback_;

  std::unique_ptr<LoginUIPrefController> login_ui_pref_controller_;

  std::unique_ptr<WizardContext> wizard_context_;

  // Callback to be executed when WebUI is started.
  base::RepeatingClosure on_wizard_controller_created_for_tests_;

  std::unique_ptr<ash::quick_start::TargetDeviceBootstrapController>
      bootstrap_controller_;

  base::CallbackListSubscription app_terminating_subscription_;

  std::unique_ptr<OobeMetricsHelper> oobe_metrics_helper_;

  std::unique_ptr<OobeCrosEventsMetrics> oobe_cros_events_metrics_;

  base::WeakPtrFactory<LoginDisplayHostCommon> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_LOGIN_DISPLAY_HOST_COMMON_H_
