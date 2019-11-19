// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_LOGIN_PERFORMER_H_
#define CHROMEOS_LOGIN_AUTH_LOGIN_PERFORMER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/authenticator.h"
#include "chromeos/login/auth/extended_authenticator.h"
#include "chromeos/login/auth/user_context.h"
#include "google_apis/gaia/google_service_auth_error.h"

class AccountId;

namespace base {
class TaskRunner;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace content {
class BrowserContext;
}

namespace chromeos {

// This class encapsulates sign in operations.
// Sign in is performed in a way that offline auth is executed first.
// Once offline auth is OK - user homedir is mounted, UI is launched.
// At this point LoginPerformer |delegate_| is destroyed and it releases
// LP instance ownership. LP waits for online login result.
// If auth is succeeded, cookie fetcher is executed, LP instance deletes itself.
//
// If |delegate_| is not NULL it will handle error messages, password input.
class COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) LoginPerformer
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
    virtual void WhiteListCheckFailed(const std::string& email) = 0;
    virtual void PolicyLoadFailed() = 0;
    virtual void SetAuthFlowOffline(bool offline) = 0;
  };

  LoginPerformer(scoped_refptr<base::TaskRunner> task_runner,
                 Delegate* delegate);
  ~LoginPerformer() override;

  // Performs a login for |user_context|.
  // If auth_mode is |kExternal|, there are no further auth checks, |kInternal|
  // will perform auth checks.
  void PerformLogin(const UserContext& user_context,
                    AuthorizationMode auth_mode);

  // Performs supervised user login with a given |user_context|.
  void LoginAsSupervisedUser(const UserContext& user_context);

  // Performs actions to prepare guest mode login.
  void LoginOffTheRecord();

  // Performs public session login with a given |user_context|.
  void LoginAsPublicSession(const UserContext& user_context);

  // Performs a login into the kiosk mode account with |app_account_id|.
  void LoginAsKioskAccount(const AccountId& app_account_id,
                           bool use_guest_mount);

  // Performs a login into the ARC kiosk mode account with |arc_app_account_id|.
  void LoginAsArcKioskAccount(const AccountId& arc_app_account_id);

  // Performs a login into the Web kiosk mode account with |web_app_account_id|.
  void LoginAsWebKioskAccount(const AccountId& web_app_account_id);

  // AuthStatusConsumer implementation:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;
  void OnOffTheRecordAuthSuccess() override;
  void OnPasswordChangeDetected() override;
  void OnOldEncryptionDetected(const UserContext& user_context,
                               bool has_incomplete_migration) override;

  // Migrates cryptohome using |old_password| specified.
  void RecoverEncryptedData(const std::string& old_password);

  // Reinitializes cryptohome with the new password.
  void ResyncEncryptedData();

  // Returns latest auth error.
  const GoogleServiceAuthError& error() const {
    return last_login_failure_.error();
  }

  // True if password change has been detected.
  bool password_changed() { return password_changed_; }

  // Number of times we've been called with OnPasswordChangeDetected().
  // If user enters incorrect old password, same LoginPerformer instance will
  // be called so callback count makes it possible to distinguish initial
  // "password changed detected" event from further attempts to enter old
  // password for cryptohome migration (when > 1).
  int password_changed_callback_count() {
    return password_changed_callback_count_;
  }

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  AuthorizationMode auth_mode() const { return auth_mode_; }

  // Check if user is allowed to sign in on device. |wildcard_match| will
  // contain additional information whether this user is explicitly listed or
  // not (may be relevant for external-based sign-in).
  virtual bool IsUserWhitelisted(const AccountId& account_id,
                                 bool* wildcard_match) = 0;

 protected:
  // Platform-dependant methods to be implemented by concrete class.

  // Run trusted check for a platform. If trusted check have to be performed
  // asynchronously, |false| will be returned, and either delegate's
  // PolicyLoadFailed() or |callback| will be called upon actual check.
  virtual bool RunTrustedCheck(const base::Closure& callback) = 0;

  // This method should run addional online check if user can sign in on device.
  // Either |success_callback| or |failure_callback| should be called upon this
  // check.
  virtual void RunOnlineWhitelistCheck(
      const AccountId& account_id,
      bool wildcard_match,
      const std::string& refresh_token,
      const base::Closure& success_callback,
      const base::Closure& failure_callback) = 0;

  // Supervised users-related methods.

  // Check if supervised users are allowed on this device.
  virtual bool AreSupervisedUsersAllowed() = 0;

  // Check which authenticator should be used for supervised user.
  virtual bool UseExtendedAuthenticatorForSupervisedUser(
      const UserContext& user_context) = 0;

  // Probably transform supervised user's authentication key.
  virtual UserContext TransformSupervisedKey(const UserContext& context) = 0;

  // Set up sign-in flow for supervised user.
  virtual void SetupSupervisedUserFlow(const AccountId& account_id) = 0;

  // Set up sign-in flow for Easy Unlock.
  virtual void SetupEasyUnlockUserFlow(const AccountId& account_id) = 0;

  // Run policy check for |account_id|. If something is wrong, delegate's
  // PolicyLoadFailed is called.
  virtual bool CheckPolicyForUser(const AccountId& account_id) = 0;

  // Look up browser context to use during signin.
  virtual content::BrowserContext* GetSigninContext() = 0;

  // Gets the SharedURLLoaderFactory used for sign in.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetSigninURLLoaderFactory() = 0;

  // Create authenticator implementation.
  virtual scoped_refptr<Authenticator> CreateAuthenticator() = 0;

  void set_authenticator(scoped_refptr<Authenticator> authenticator);

  // Notifications receiver.
  Delegate* delegate_;

 private:
  // Starts login completion of externally authenticated user.
  void StartLoginCompletion();

  // Starts authentication.
  void StartAuthentication();
  void NotifyWhitelistCheckFailure();

  // Makes sure that authenticator is created.
  void EnsureAuthenticator();
  void EnsureExtendedAuthenticator();

  // Actual implementation of LoginAsSupervisedUser that is run after trusted
  // values check.
  void TrustedLoginAsSupervisedUser(const UserContext& user_context);

  // Actual implementantion of PeformLogin that is run after trusted values
  // check.
  void DoPerformLogin(const UserContext& user_context,
                      AuthorizationMode auth_mode);

  scoped_refptr<base::TaskRunner> task_runner_;

  // Used for logging in.
  scoped_refptr<Authenticator> authenticator_;

  // Used for logging in.
  scoped_refptr<ExtendedAuthenticator> extended_authenticator_;

  // Represents last login failure that was encountered when communicating to
  // sign-in server. AuthFailure.LoginFailureNone() by default.
  AuthFailure last_login_failure_;

  // User credentials for the current login attempt.
  UserContext user_context_;

  // True if password change has been detected.
  // Once correct password is entered homedir migration is executed.
  bool password_changed_ = false;
  int password_changed_callback_count_ = 0;

  // Authorization mode type.
  AuthorizationMode auth_mode_ = AuthorizationMode::kInternal;

  base::WeakPtrFactory<LoginPerformer> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(LoginPerformer);
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_LOGIN_PERFORMER_H_
