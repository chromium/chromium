// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_CONTROLLER_H_

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"
#include "url/origin.h"

namespace device_reauth {
class DeviceAuthenticator;
}  // namespace device_reauth

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace password_manager {

class PasswordManagerClient;
class PasswordStoreInterface;
class RemoteActorCredentialSharingService;

// Platform-agnostic logic for sharing one of the user's saved credentials for
// a domain with a remote actor (e.g. Gemini). Each instance handles a single
// request.
class RemoteActorCredentialSharingController : public PasswordStoreConsumer {
 public:
  // Called with the outcome of `Start()`.
  using ResultCallback =
      base::OnceCallback<void(RemoteActorCredentialSharingResult)>;

  // Platform hooks for the user-facing steps.
  class Delegate {
   public:
    // Selected credential on success, or the failure reason.
    using CredentialOrError =
        base::expected<PasswordForm, RemoteActorCredentialSharingResult>;

    virtual ~Delegate() = default;

    // Shows the credential selection dialog for `credentials` on `origin`.
    virtual void ShowCredentialSelection(
        std::vector<std::unique_ptr<PasswordForm>> credentials,
        const url::Origin& origin,
        base::OnceCallback<void(CredentialOrError)> callback) = 0;
  };

  // `identity_manager`, `sync_service`, `sharing_service` and `client` may be
  // null when unavailable. The store matching the user's sync state and
  // `delegate` must be non-null. All must outlive this controller.
  RemoteActorCredentialSharingController(
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service,
      PasswordStoreInterface* profile_store,
      PasswordStoreInterface* account_store,
      RemoteActorCredentialSharingService* sharing_service,
      PasswordManagerClient* client,
      Delegate* delegate);
  RemoteActorCredentialSharingController(
      const RemoteActorCredentialSharingController&) = delete;
  RemoteActorCredentialSharingController& operator=(
      const RemoteActorCredentialSharingController&) = delete;
  ~RemoteActorCredentialSharingController() override;

  // Starts sharing a credential for `domain` on behalf of the user identified
  // by `gaia_id`, scoped to `task_id`. Must be called at most once.
  void Start(std::string_view gaia_id,
             std::string_view domain,
             std::string_view task_id,
             ResultCallback callback);

  // Returns whether `Start()` was called and the result was not reported yet.
  bool IsActive() const;

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResultsOrErrorFrom(
      PasswordStoreInterface* store,
      base::expected<std::vector<StoredCredential>, PasswordStoreBackendError>
          results_or_error) override;

  // Verifies the GAIA ID matches the signed-in user and sync is not in error.
  bool VerifyUserIdentityAndSyncState(std::string_view gaia_id) const;

  // Initiates asynchronous queries to the relevant password stores.
  void QueryPasswordStores(std::string_view domain);

  // Called when all password store queries have completed.
  void OnAllLoginsRetrieved();

  // Called with the selected credential, or the error if none was selected.
  void OnCredentialSelected(Delegate::CredentialOrError selected_form);

  // Re-authenticates the user if required before proceeding with
  // `selected_form`.
  void AuthenticateIfNeeded(PasswordForm selected_form);

  // Callback triggered when device re-authentication completes.
  void OnAuthenticationResult(PasswordForm selected_form, bool success);

  // Shares `selected_form` via `sharing_service_`.
  void ProceedWithCredential(PasswordForm selected_form);

  // Callback triggered when the sharing service completes the operation.
  void OnShareCompleted(bool success);

  // Asynchronously posts `result` to the result callback.
  void RespondWithError(RemoteActorCredentialSharingResult result);

  // Runs `callback_` with `result`.
  void RunResultCallback(RemoteActorCredentialSharingResult result);

  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<PasswordStoreInterface> profile_store_;
  const raw_ptr<PasswordStoreInterface> account_store_;
  const raw_ptr<RemoteActorCredentialSharingService> sharing_service_;
  const raw_ptr<PasswordManagerClient> client_;
  const raw_ref<Delegate> delegate_;

  // The GAIA ID of the user requesting authentication.
  std::string gaia_id_;
  // The origin for which credentials are requested.
  url::Origin origin_;
  // The ID of the remote actor task requesting credentials.
  std::string task_id_;
  // The callback to return the result to the caller.
  ResultCallback callback_;
  // The number of password store queries started.
  size_t expected_callbacks_ = 0;
  // The number of password store queries that have completed.
  size_t received_callbacks_ = 0;
  // Collected credentials from the stores.
  std::vector<PasswordForm> credentials_;
  // Device authenticator used when re-authentication is required.
  std::unique_ptr<device_reauth::DeviceAuthenticator> device_authenticator_;

  base::WeakPtrFactory<RemoteActorCredentialSharingController>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_CONTROLLER_H_
