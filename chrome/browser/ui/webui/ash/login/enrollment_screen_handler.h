// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENROLLMENT_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENROLLMENT_SCREEN_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "net/cookies/canonical_cookie.h"

namespace ash {

class CookieWaiter;
class HelpAppLauncher;

// Possible error states of the Active Directory screen. Must be in the same
// order as ActiveDirectoryErrorState ( in enterprise_enrollment.js ) values.
enum class ActiveDirectoryErrorState {
  NONE = 0,
  MACHINE_NAME_INVALID = 1,
  MACHINE_NAME_TOO_LONG = 2,
  BAD_USERNAME = 3,
  BAD_AUTH_PASSWORD = 4,
  BAD_UNLOCK_PASSWORD = 5,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ActiveDirectoryDomainJoinType {
  // Configuration is not set on the domain.
  WITHOUT_CONFIGURATION = 0,
  // Configuration is set but was not unlocked during domain join.
  NOT_USING_CONFIGURATION = 1,
  // Configuration is set and was unlocked during domain join.
  USING_CONFIGURATION = 2,
  // Number of elements in the enum. Should be last.
  COUNT,
};

// WebUIMessageHandler implementation which handles events occurring on the
// page, such as the user pressing the signin button.
class EnrollmentScreenHandler : public BaseScreenHandler,
                                public EnrollmentScreenView {
 public:
  using TView = EnrollmentScreenView;

  EnrollmentScreenHandler();

  EnrollmentScreenHandler(const EnrollmentScreenHandler&) = delete;
  EnrollmentScreenHandler& operator=(const EnrollmentScreenHandler&) = delete;

  ~EnrollmentScreenHandler() override;

  // Implements EnrollmentScreenView:
  void SetEnrollmentConfig(const policy::EnrollmentConfig& config) override;
  void SetEnrollmentController(Controller* controller) override;

  void SetEnterpriseDomainInfo(const std::string& manager,
                               const std::u16string& device_type) override;
  void SetFlowType(FlowType flow_type) override;
  void SetGaiaButtonsType(GaiaButtonsType buttons_type) override;
  void Show() override;
  void Hide() override;
  void ShowSigninScreen() override;
  void ReloadSigninScreen() override;
  void ResetEnrollmentScreen() override;
  void ShowSkipConfirmationDialog() override;
  void ShowUserError(const std::string& email) override;
  void ShowEnrollmentDuringTrialNotAllowedError() override;
  void ShowActiveDirectoryScreen(const std::string& domain_join_config,
                                 const std::string& machine_name,
                                 const std::string& username,
                                 authpolicy::ErrorType error) override;
  void ShowAttributePromptScreen(const std::string& asset_id,
                                 const std::string& location) override;
  void ShowEnrollmentSuccessScreen() override;
  void ShowEnrollmentWorkingScreen() override;
  void ShowEnrollmentTPMCheckingScreen() override;
  void ShowAuthError(const GoogleServiceAuthError& error) override;
  void ShowEnrollmentStatus(policy::EnrollmentStatus status) override;
  void ShowOtherError(
      EnterpriseEnrollmentHelper::OtherError error_code) override;
  void Shutdown() override;

  // Implements BaseScreenHandler:
  void InitAfterJavascriptAllowed() override;
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;
  void GetAdditionalParameters(base::Value::Dict* parameters) override;

  void ContinueAuthenticationWhenCookiesAvailable(const std::string& user,
                                                  int license_type);
  void OnCookieWaitTimeout();

 private:
  // Handlers for WebUI messages.
  void HandleToggleFakeEnrollment();
  void HandleClose(const std::string& reason);
  void HandleCompleteLogin(const std::string& user, int license_type);
  void OnGetCookiesForCompleteLogin(
      const std::string& user,
      int license_type,
      const net::CookieAccessResultList& cookies,
      const net::CookieAccessResultList& excluded_cookies);
  void HandleAdCompleteLogin(const std::string& machine_name,
                             const std::string& distinguished_name,
                             const std::string& encryption_types,
                             const std::string& user_name,
                             const std::string& password);
  void HandleAdUnlockConfiguration(const std::string& password);
  void HandleIdentifierEntered(const std::string& email);
  void HandleRetry();
  void HandleFrameLoadingCompleted();
  void HandleDeviceAttributesProvided(const std::string& asset_id,
                                      const std::string& location);
  void HandleOnLearnMore();

  // Shows a given enrollment step.
  void ShowStep(const std::string& step);

  // Display the given i18n resource as error message.
  void ShowError(int message_id, bool retry);

  // Display the given i18n resource as an error message, with the $1
  // substitution parameter replaced with the device's product name.
  void ShowErrorForDevice(int message_id, bool retry);

  // Display the given string as error message.
  void ShowErrorMessage(const std::string& message, bool retry);

  // Display the given i18n string as a progress message.
  void ShowWorking(int message_id);

  // Shows the screen. Asynchronous for oauth-based enrollment.
  void DoShow();

  // Shows oauth-based enrollment screen using the given sign-in partition name.
  void DoShowWithPartition(const std::string& partition_name);

  // Shows the screen with the given data dictionary.
  void DoShowWithData(base::Value::Dict screen_data);

  // Screen data to be passed to web ui for attestation enrollment.
  base::Value::Dict ScreenDataForAttestationEnrollment();

  // Screen data to be passed to web ui for gaia oauth-based enrollment.
  base::Value::Dict ScreenDataForOAuthEnrollment();

  // Screen data to be passed to web ui for all enrollment modes.
  base::Value::Dict ScreenDataCommon();

  // Returns true if current visible screen is the enrollment sign-in page.
  bool IsOnEnrollmentScreen();

  // Called after configuration seed was unlocked.
  void OnAdConfigurationUnlocked(std::string unlocked_data);

  // Keeps the controller for this view.
  raw_ptr<Controller, ExperimentalAsh> controller_ = nullptr;

  bool show_on_init_ = false;

  // The enrollment configuration.
  policy::EnrollmentConfig config_;

  // GAIA flow type parameter that is set to authenticator.
  FlowType flow_type_;

  GaiaButtonsType gaia_buttons_type_;

  // Active Directory configuration in the form of encrypted binary data.
  std::string active_directory_domain_join_config_;

  ActiveDirectoryDomainJoinType active_directory_join_type_ =
      ActiveDirectoryDomainJoinType::COUNT;

  // True if screen was not shown yet.
  bool first_show_ = true;

  // Set true when chrome is being restarted to pick up enrollment changes. The
  // renderer processes will be destroyed and can no longer be talked to.
  bool shutdown_ = false;

  std::string signin_partition_name_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  std::unique_ptr<CookieWaiter> oauth_code_waiter_;

  bool use_fake_login_for_testing_ = false;

  base::WeakPtrFactory<EnrollmentScreenHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENROLLMENT_SCREEN_HANDLER_H_
