// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_FRONTEND_H_
#define COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_FRONTEND_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/trusted_vault/standalone_trusted_vault_backend.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_frontend.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"

struct CoreAccountInfo;
class GaiaId;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace trusted_vault {

class StandaloneTrustedVaultClient;

// UI-thread frontend that owns and manages sequence-posting to a single
// background StandaloneTrustedVaultBackend instance. Shared across multiple
// StandaloneTrustedVaultClient instances (one per SecurityDomainId) within a
// Profile.
class StandaloneTrustedVaultFrontend
    : public base::RefCounted<StandaloneTrustedVaultFrontend> {
 public:
  StandaloneTrustedVaultFrontend(
#if BUILDFLAG(IS_MAC)
      const std::string& icloud_keychain_access_group_prefix,
#endif
      const base::FilePath& base_dir,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  StandaloneTrustedVaultFrontend(const StandaloneTrustedVaultFrontend&) =
      delete;
  StandaloneTrustedVaultFrontend& operator=(
      const StandaloneTrustedVaultFrontend&) = delete;

  // Client registration / unregistration on the UI thread.
  void RegisterClient(SecurityDomainId security_domain,
                      StandaloneTrustedVaultClient* client);
  void UnregisterClient(SecurityDomainId security_domain,
                        StandaloneTrustedVaultClient* client);

  // Domain-parameterized UI-thread methods that post to the backend sequence.
  void FetchKeys(
      const CoreAccountInfo& account_info,
      SecurityDomainId security_domain,
      base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb);
  void StoreKeys(const GaiaId& gaia_id,
                 SecurityDomainId security_domain,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int last_key_version);
  void MarkLocalKeysAsStale(const CoreAccountInfo& account_info,
                            SecurityDomainId security_domain,
                            base::OnceCallback<void(bool)> cb);
  void GetIsRecoverabilityDegraded(const CoreAccountInfo& account_info,
                                   SecurityDomainId security_domain,
                                   base::OnceCallback<void(bool)> cb);
  void AddTrustedRecoveryMethod(const GaiaId& gaia_id,
                                SecurityDomainId security_domain,
                                const std::vector<uint8_t>& public_key,
                                int method_type_hint,
                                base::OnceCallback<void(bool)> cb);
  void ClearLocalDataForAccount(const CoreAccountInfo& account_info);

  // Testing methods.
  void WaitForIdleForTesting(base::OnceClosure cb) const;
  void GetLastAddedRecoveryMethodPublicKeyForTesting(
      base::OnceCallback<void(const std::vector<uint8_t>&)> callback);
  void GetLastKeyVersionForTesting(
      const GaiaId& gaia_id,
      SecurityDomainId security_domain,
      base::OnceCallback<void(int last_key_version)> callback);

 private:
  friend class base::RefCounted<StandaloneTrustedVaultFrontend>;

  ~StandaloneTrustedVaultFrontend();

  void NotifyTrustedVaultKeysChanged(
      std::optional<TrustedVaultUserActionTriggerForUMA> trigger);
  void NotifyRecoverabilityDegradedChanged(SecurityDomainId security_domain);

  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::flat_map<SecurityDomainId, raw_ptr<StandaloneTrustedVaultClient>>
      clients_;

  // Allows access token fetching for primary account on the UI thread. Passed
  // as WeakPtr to TrustedVaultAccessTokenFetcherImpl.
  TrustedVaultAccessTokenFetcherFrontend access_token_fetcher_frontend_;

  // |backend_| constructed on the UI thread, used on |backend_task_runner_|
  // and destroyed on |backend_task_runner_| via OnTaskRunnerDeleter.
  std::unique_ptr<StandaloneTrustedVaultBackend, base::OnTaskRunnerDeleter>
      backend_;

  // Observes changes of accounts state and populates them into |backend_|.
  // Declared after |backend_| so it is destroyed on the UI thread before
  // |backend_|'s OnTaskRunnerDeleter posts deletion to |backend_task_runner_|.
  std::unique_ptr<signin::IdentityManager::Observer> identity_manager_observer_;

  base::WeakPtrFactory<StandaloneTrustedVaultFrontend> weak_ptr_factory_{this};
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_FRONTEND_H_
