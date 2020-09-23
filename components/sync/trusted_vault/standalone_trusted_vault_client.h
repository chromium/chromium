// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_CLIENT_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/trusted_vault_client.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher_frontend.h"

struct CoreAccountInfo;

namespace syncer {

class StandaloneTrustedVaultBackend;

// Standalone, file-based implementation of TrustedVaultClient that stores the
// keys in a local file, containing a serialized protocol buffer encrypted with
// platform-dependent crypto mechanisms (OSCrypt).
//
// Reading of the file is done lazily.
class StandaloneTrustedVaultClient : public TrustedVaultClient {
 public:
  // |identity_manager| must not be null and must outlive this object.
  StandaloneTrustedVaultClient(const base::FilePath& file_path,
                               signin::IdentityManager* identity_manager);
  StandaloneTrustedVaultClient(const StandaloneTrustedVaultClient& other) =
      delete;
  StandaloneTrustedVaultClient& operator=(
      const StandaloneTrustedVaultClient& other) = delete;
  ~StandaloneTrustedVaultClient() override;

  // TrustedVaultClient implementation.
  std::unique_ptr<Subscription> AddKeysChangedObserver(
      const base::RepeatingClosure& cb) override;
  void FetchKeys(
      const CoreAccountInfo& account_info,
      base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb)
      override;
  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int last_key_version) override;
  void RemoveAllStoredKeys() override;
  void MarkKeysAsStale(const CoreAccountInfo& account_info,
                       base::OnceCallback<void(bool)> cb) override;
  void GetIsRecoverabilityDegraded(const CoreAccountInfo& account_info,
                                   base::OnceCallback<void(bool)> cb) override;

  // Runs |cb| when all requests have completed.
  void WaitForFlushForTesting(base::OnceClosure cb) const;
  void FetchBackendPrimaryAccountForTesting(
      base::OnceCallback<void(const base::Optional<CoreAccountInfo>&)> callback)
      const;
  void SetRecoverabilityDegradedForTesting();

 private:
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  CallbackList observer_list_;

  // Allows access token fetching for primary account on the ui thread. Passed
  // as WeakPtr to TrustedVaultAccessTokenFetcherImpl.
  TrustedVaultAccessTokenFetcherFrontend access_token_fetcher_frontend_;

  // |backend_| constructed in the UI thread, used in |backend_task_runner_|
  // and destroyed (refcounted) on any thread.
  scoped_refptr<StandaloneTrustedVaultBackend> backend_;

  // Observes changes of primary account and populates them into |backend_|.
  // Holds references to |backend_| and |backend_task_runner_|.
  std::unique_ptr<signin::IdentityManager::Observer> primary_account_observer_;

  bool is_recoverability_degraded_for_testing_ = false;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_CLIENT_H_
