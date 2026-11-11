// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_CONTROLLER_H_

#include <cstddef>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
#include "components/password_manager/core/browser/remote_actor/remote_actor_credential_sharing_result.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace password_manager {

class PasswordStoreInterface;

// Platform-agnostic logic for sharing one of the user's saved credentials for
// a domain with a remote actor (e.g. Gemini). Each instance handles a single
// request.
class RemoteActorCredentialSharingController : public PasswordStoreConsumer {
 public:
  // The credentials synced to the user's account that match the requested
  // domain, or the result explaining why they could not be fetched.
  using CredentialsOrError = base::expected<std::vector<PasswordForm>,
                                            RemoteActorCredentialSharingResult>;
  // Called with the outcome of `Start()`.
  using CredentialsCallback = base::OnceCallback<void(CredentialsOrError)>;

  // `identity_manager` and `sync_service` may be null when unavailable for the
  // profile. The store matching the user's sync state must be non-null. All
  // must outlive this controller.
  RemoteActorCredentialSharingController(
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service,
      PasswordStoreInterface* profile_store,
      PasswordStoreInterface* account_store);
  RemoteActorCredentialSharingController(
      const RemoteActorCredentialSharingController&) = delete;
  RemoteActorCredentialSharingController& operator=(
      const RemoteActorCredentialSharingController&) = delete;
  ~RemoteActorCredentialSharingController() override;

  // Fetches the credentials for `domain` synced to the account of the user
  // identified by `gaia_id`. If `gaia_id` is not the signed-in user, sync is
  // in an error state or no store holds account credentials, `callback` is run
  // synchronously with the corresponding error. Otherwise, it is run
  // asynchronously with the matching credentials, which may be empty. Must be
  // called at most once.
  // The owner may destroy the controller from within `callback`.
  void Start(std::string_view gaia_id,
             std::string_view domain,
             CredentialsCallback callback);

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

  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<PasswordStoreInterface> profile_store_;
  const raw_ptr<PasswordStoreInterface> account_store_;

  // The callback to return the result to the caller.
  CredentialsCallback callback_;
  // The number of password store queries started.
  size_t expected_callbacks_ = 0;
  // The number of password store queries that have completed.
  size_t received_callbacks_ = 0;
  // Collected credentials from the stores.
  std::vector<PasswordForm> credentials_;

  base::WeakPtrFactory<RemoteActorCredentialSharingController>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_CONTROLLER_H_
