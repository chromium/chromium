// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_LOGIN_PERFORMER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_LOGIN_PERFORMER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/authenticator.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/google_service_auth_error.h"

class AccountId;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

// This class encapsulates sign in operations.
// Sign in is performed in a way that offline auth is executed first.
// Once offline auth is OK - user homedir is mounted, UI is launched.
// At this point LoginPerformer |delegate_| is destroyed and it releases
// LP instance ownership. LP waits for online login result.
// If auth is succeeded, cookie fetcher is executed, LP instance deletes itself.
//
// If |delegate_| is not NULL it will handle error messages, password input.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH) LoginPerformer
    : public AuthStatusConsumer {
 public:
  enum class AuthorizationMode {
    // Authorization performed internally by Chrome.
    kInternal,
    // Authorization performed by an external service (e.g., Gaia, or Active
    // Directory).
    kExternal
  };

  // Delegate class to get notifications from the LoginPerformer.
  class Delegate : public AuthStatusConsumer {
   public:
    ~Delegate() override {}
    virtual void AllowlistCheckFailed(const std::string& email) = 0;
    virtual void PolicyLoadFailed() = 0;
    // Sends AuthSuccess metrics to LoginUnlockThroughputRecorder if needed.
    virtual void ReportOnAuthSuccessMetrics() {}
  };

  explicit LoginPerformer(Delegate* delegate,
                          AuthEventsRecorder* metrics_recorder);

  LoginPerformer(const LoginPerformer&) = delete;
  LoginPerformer& operator=(const LoginPerformer&) = delete;

  ~LoginPerformer() override;

  // Performs a login for |user_context|.
  // If auth_mode is |kExternal|, there are no further auth checks, |kInternal|
  // will perform auth checks.
  void PerformLogin(const UserContext& user_context,
                    AuthorizationMode auth_mode);

  // Performs actions to prepare guest mode login.
  void LoginOffTheRecord();

  // Performs public session login with a given |user_context|.
  void LoginAsPublicSession(const UserContext& user_context);

  // Performs a login into the kiosk mode account with |app_account_id|.
  void LoginAsKioskAccount(const AccountId& app_account_id);

  // Performs a login into the Web kiosk mode account with |web_app_account_id|.
  void LoginAsWebKioskAccount(const AccountId& web_app_account_id);

  // Performs a login into the IWA kiosk mode account with |iwa_account_id|.
  void LoginAsIwaKioskAccount(const AccountId& iwa_account_id);

  // Performs final stages of the login for user already authenticated via
  // `AuthSession`.
  void LoginAuthenticated(std::unique_ptr<UserContext> user_context);

  // AuthStatusConsumer implementation:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;
  void OnOffTheRecordAuthSuccess() override;
  void OnOnlinePasswordUnusable(std::unique_ptr<UserContext>, bool) override;
  void OnOldEncryptionDetected(std::unique_ptr<UserContext>,
                               bool has_incomplete_migration) override;
  void OnLocalAuthenticationRequired(
      std::unique_ptr<UserContext> user_context) override;

  // Returns latest auth error.
  const GoogleServiceAuthError& error() const {
    return last_login_failure_.error();
  }

  // True if password change has been detected.
  bool password_changed() { return password_changed_; }

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  AuthorizationMode auth_mode() const { return auth_mode_; }

  // Check if user is allowed to sign in on device. |wildcard_match| will
  // contain additional information whether this user is explicitly listed or
  // not (may be relevant for external-based sign-in). |user_type| will be used
  // to check if the user is allowed because of the user type, pass
  // std::nullopt if user type is not known.
  virtual bool IsUserAllowlisted(
      const AccountId& account_id,
      bool* wildcard_match,
      const std::optional<user_manager::UserType>& user_type) = 0;

  virtual void LoadAndApplyEarlyPrefs(std::unique_ptr<UserContext> context,
                                      AuthOperationCallback callback) = 0;

 protected:
  // Platform-dependant methods to be implemented by concrete class.

  // Run trusted check for a platform. If trusted check have to be performed
  // asynchronously, |false| will be returned, and either delegate's
  // PolicyLoadFailed() or |callback| will be called upon actual check.
  virtual bool RunTrustedCheck(base::OnceClosure callback) = 0;

  // This method should run addional online check if user can sign in on device.
  // Either |success_callback| or |failure_callback| should be called upon this
  // check.
  virtual void RunOnlineAllowlistCheck(const AccountId& account_id,
                                       bool wildcard_match,
                                       const std::string& refresh_token,
                                       base::OnceClosure success_callback,
                                       base::OnceClosure failure_callback) = 0;

  // Run policy check for |account_id|. If something is wrong, delegate's
  // PolicyLoadFailed is called.
  virtual bool CheckPolicyForUser(const AccountId& account_id) = 0;

  // Gets the SharedURLLoaderFactory used for sign in.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetSigninURLLoaderFactory() = 0;

  // Create authenticator implementation.
  virtual scoped_refptr<Authenticator> CreateAuthenticator() = 0;

  void set_authenticator(scoped_refptr<Authenticator> authenticator);

  // Notifications receiver.
  raw_ptr<Delegate> delegate_;

 private:
  // Starts login completion of externally authenticated user.
  void StartLoginCompletion();

  // Starts authentication.
  void StartAuthentication();

  // Makes sure that authenticator is created.
  void EnsureAuthenticator();

  // Actual implementantion of PeformLogin that is run after trusted values
  // check.
  void DoPerformLogin(const UserContext& user_context,
                      AuthorizationMode auth_mode);

  // Notifications to delegate
  void NotifyAuthFailure(const AuthFailure& error);
  void NotifyAuthSuccess(const UserContext& user_context);
  void NotifyOffTheRecordAuthSuccess();
  void NotifyOnlinePasswordUnusable(std::unique_ptr<UserContext> user_context,
                                    bool online_password_mismatch);
  void NotifyOldEncryptionDetected(std::unique_ptr<UserContext> user_context,
                                   bool has_incomplete_migration);
  void NotifyLocalAuthenticationRequired(
      std::unique_ptr<UserContext> user_context);
  void NotifyAllowlistCheckFailure();

  // Callback passed to `LoadAndApplyEarlyPrefs`.
  void OnEarlyPrefsApplied(std::unique_ptr<UserContext> context,
                           std::optional<AuthenticationError> error);

  // Used for logging in.
  scoped_refptr<Authenticator> authenticator_;

  // Used for metric reporting.
  const raw_ptr<AuthEventsRecorder> auth_events_recorder_;

  // Represents last login failure that was encountered when communicating to
  // sign-in server. AuthFailure.LoginFailureNone() by default.
  AuthFailure last_login_failure_;

  // User credentials for the current login attempt.
  UserContext user_context_;

  // True if password change has been detected.
  // Once correct password is entered homedir migration is executed.
  bool password_changed_ = false;

  // Authorization mode type.
  AuthorizationMode auth_mode_ = AuthorizationMode::kInternal;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LoginPerformer> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_LOGIN_PERFORMER_H_
