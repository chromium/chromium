// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_delegate.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_siwg_controller_interface.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_web_content_interface.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_interface.h"

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

namespace actor_login {

class ActorLoginCredentialFiller;
class ActorLoginDelegateClient;
class ActorLoginGetCredentialsHelper;
class ActorLoginMetricsHelper;

// Delegate implementation, scoped to `WebContents` as its functionality is
// intrinsically tied to a specific browser tab.
class ActorLoginDelegateImpl
    : public ActorLoginDelegate,
      public ActorLoginWebContentInterface,
      public base::SupportsUserData::Data,
      public password_manager::PasswordManagerInterface::Observer {
 public:
  ~ActorLoginDelegateImpl() override;

  // Not copyable or movable.
  ActorLoginDelegateImpl(const ActorLoginDelegateImpl&) = delete;
  ActorLoginDelegateImpl& operator=(const ActorLoginDelegateImpl&) = delete;

  // Retrieve the instance stored in `user_data`.
  static ActorLoginDelegateImpl* FromUserData(
      base::SupportsUserData* user_data);

  // Create and store the instance in `user_data`.
  static ActorLoginDelegateImpl* CreateForUserData(
      base::SupportsUserData* user_data,
      ActorLoginDelegateClient* actor_login_delegate_client);

  // Removes the instance stored in `user_data` for testing purposes.
  static void RemoveFromUserDataForTesting(base::SupportsUserData* user_data);

  // `ActorLoginDelegate` implementation:
  void GetCredentials(
      bool has_sign_in_with_google_button,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      CredentialsOrErrorReply callback) override;
  void AttemptLogin(
      const Credential& credential,
      bool should_store_permission,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger,
      base::TimeTicks attempt_login_tool_start_time,
      LoginStatusResultOrErrorReply done_callback,
      base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate) override;

  // password_manager::PasswordManagerInterface::Observer implementation:
  void OnLoginSuccessful(
      const password_manager::PasswordForm& pending_form) override;

  // ActorLoginWebContentInterface implementation:
  void OnPrimaryPageChanged() override;
  void OnContextDestroyed() override;

#if defined(UNIT_TEST)
  // TODO(crbug.com/508169237): Utilize `WebContentsTester` instead.
  ActorLoginSiwgControllerInterface* siwg_controller() {
    return siwg_controller_.get();
  }
#endif

 private:
  explicit ActorLoginDelegateImpl(
      ActorLoginDelegateClient* actor_login_delegate_client);

  // Private helper methods for handling task completion. They should be
  // invoked asynchronously.
  void OnGetCredentialsCompleted(CredentialsOrErrorReply callback,
                                 CredentialsOrError result,
                                 bool conflicting_permissions);
  void OnAttemptLoginCompleted(
      base::expected<LoginStatusResult, ActorLoginError> result);

  void OnFederatedLoginCompletedPostButtonClick(bool success);

  // Called when `OnAttemptLoginCompleted` is invoked with a result for
  // a federated credential login.
  void ProcessFederatedResult(
      base::expected<LoginStatusResult, ActorLoginError> result);

  // Called when `OnAttemptLoginCompleted` is invoked with a filling
  // result for a password credential login.
  void ProcessPasswordResult(
      base::expected<LoginStatusResult, ActorLoginError> result);

  void RemoveFederatedEmbedderLoginRequest();

  bool ShouldCleanUpConflictingPermissions(
      const password_manager::PasswordForm& form) const;

  // Calls the permissions cleaning service to clean up conflicting permissions.
  // If the login attempt was performed with a password credential,
  // `signon_realm`, is used to identify it, so that we don't clean the
  // permission granted after disambiguation.
  void ClearConflictingPermissions();

  // Reset any pending state from a previous invocation. Most fields are reset
  // when the corresponding request finishes, or the login succeeds or failed.
  // However, the password manager cannot be fully relied upon to call the
  // delegate back with the login result, so just in case, reset the fields
  // which depend on it when a new `GetCredentials` request comes in.
  void ResetState();

  // Helper methods for recording metrics.
  void RecordGetCredentialsMetricsAndResetHelper(
      const CredentialsOrError& result);
  void RecordAttemptLoginMetrics(const Credential& credential);

  // Store the pending callback. A non-null callback indicates an active
  // request.
  LoginStatusResultOrErrorReply pending_attempt_login_done_callback_;

  // TODO(crbug.com/460025687): Use raw_ptr.
  base::WeakPtr<ActionSequenceDelegate> action_sequence_delegate_;

  // Helper for `GetCredentials`. Scoped to one `GetCredentials` request.
  std::unique_ptr<ActorLoginGetCredentialsHelper> get_credentials_helper_;

  raw_ptr<ActorLoginDelegateClient> actor_login_delegate_client_ = nullptr;

  // Can be null on Android when using third-party password manager.
  raw_ptr<password_manager::PasswordManagerClient> password_manager_client_ =
      nullptr;

  // Helper for recording Actor.Login metrics. The helper is created at the
  // beginning of a `GetCredentials` or `AttemptLogin` request, and it's
  // reset (recording metrics) when the request is completed. If credentials
  // are found, it's kept alive until an `AttemptLogin` request is made or
  // until the flow is considered finished.
  std::unique_ptr<ActorLoginMetricsHelper> metrics_helper_;

  // Fills credentials into a form. Scoped to one `AttemptLogin` request.
  std::unique_ptr<ActorLoginCredentialFiller> credential_filler_;

  // Handles FedCM login. For prototyping purposes this uses heuristics to find
  // and click the SiwG button. After the prototype, the click will be done
  // through `ExecutionEngine`.
  // Scoped to one `AttemptLogin` request.
  std::unique_ptr<ActorLoginSiwgControllerInterface> siwg_controller_;

  // Stores the credential with which the latest `AttemptLogin` request was
  // made. This is used to clean up the permission after the login attempt.
  std::unique_ptr<Credential> last_attempted_credential_;

  // Set to true whenever we find conflicting permissions in the
  // `GetCredentials` step. Reset when the login process completes. If the login
  // is successful the conflicting permissions will be cleaned up.
  // TODO(crbug.com/486089293): Reset on federated login completion as well.
  bool found_conflicting_permissions_ = false;

  // Used to listen to whether the password login was successful.
  base::ScopedObservation<password_manager::PasswordManagerInterface,
                          password_manager::PasswordManagerInterface::Observer>
      password_manager_observation_{this};

  base::WeakPtrFactory<ActorLoginDelegateImpl> weak_ptr_factory_{this};
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_DELEGATE_IMPL_H_
