// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_CLIENT_H_
#define COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/trusted_vault/recovery_key_store_controller.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_frontend.h"
#include "components/trusted_vault/trusted_vault_client.h"

struct CoreAccountInfo;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace trusted_vault {

enum class SecurityDomainId;
class StandaloneTrustedVaultBackend;

// Standalone, file-based implementation of TrustedVaultClient that stores the
// keys in a local file, containing a serialized protocol buffer encrypted with
// platform-dependent crypto mechanisms (OSCrypt).
//
// Reading of the file is done lazily.
class StandaloneTrustedVaultClient : public TrustedVaultClient {
 public:
  // Allows to observe backend state changes for testing. Production code should
  // use TrustedVaultClient::Observer.
  class DebugObserver : public base::CheckedObserver {
   public:
    DebugObserver() = default;
    DebugObserver(const DebugObserver&) = delete;
    DebugObserver& operator=(const DebugObserver&) = delete;
    ~DebugObserver() override = default;

    virtual void OnBackendStateChanged() = 0;
  };

  // |base_dir| is the directory in which to create snapshot
  // files. |identity_manager| must not be null and must outlive this object.
  // |url_loader_factory| must not be null.
  // |recovery_key_provider| may be null, in which case
  // |SetRecoveryKeyStoreUploadEnabled()| must not be called.
  StandaloneTrustedVaultClient(
      SecurityDomainId security_domain,
      const base::FilePath& base_dir,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<RecoveryKeyStoreController::RecoveryKeyProvider>
          recovery_key_provider);

  StandaloneTrustedVaultClient(
      SecurityDomainId security_domain,
      const base::FilePath& base_dir,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  StandaloneTrustedVaultClient(const StandaloneTrustedVaultClient& other) =
      delete;
  StandaloneTrustedVaultClient& operator=(
      const StandaloneTrustedVaultClient& other) = delete;
  ~StandaloneTrustedVaultClient() override;

  // TrustedVaultClient implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void FetchKeys(
      const CoreAccountInfo& account_info,
      base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb)
      override;
  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int last_key_version) override;
  void MarkLocalKeysAsStale(const CoreAccountInfo& account_info,
                            base::OnceCallback<void(bool)> cb) override;
  void GetIsRecoverabilityDegraded(const CoreAccountInfo& account_info,
                                   base::OnceCallback<void(bool)> cb) override;
  void AddTrustedRecoveryMethod(const std::string& gaia_id,
                                const std::vector<uint8_t>& public_key,
                                int method_type_hint,
                                base::OnceClosure cb) override;
  void ClearLocalDataForAccount(const CoreAccountInfo& account_info) override;

  // Runs |cb| when all requests have completed.
  void WaitForFlushForTesting(base::OnceClosure cb) const;
  void FetchBackendPrimaryAccountForTesting(
      base::OnceCallback<void(const std::optional<CoreAccountInfo>&)> callback)
      const;
  void FetchIsDeviceRegisteredForTesting(
      const std::string& gaia_id,
      base::OnceCallback<void(bool)> callback);
  void AddDebugObserverForTesting(DebugObserver* debug_observer);
  void RemoveDebugObserverForTesting(DebugObserver* debug_observer);
  // TODO(crbug.com/40178774): This this API and rely exclusively on
  // FakeSecurityDomainsServer.
  void GetLastAddedRecoveryMethodPublicKeyForTesting(
      base::OnceCallback<void(const std::vector<uint8_t>&)> callback);
  void GetLastKeyVersionForTesting(
      const std::string& gaia_id,
      base::OnceCallback<void(int last_key_version)> callback);

 private:
  void NotifyTrustedVaultKeysChanged();
  void NotifyRecoverabilityDegradedChanged();
  void NotifyBackendStateChanged();

  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::ObserverList<Observer> observer_list_;
  base::ObserverList<DebugObserver> debug_observer_list_;

  // Allows access token fetching for primary account on the ui thread. Passed
  // as WeakPtr to TrustedVaultAccessTokenFetcherImpl.
  TrustedVaultAccessTokenFetcherFrontend access_token_fetcher_frontend_;

  // |backend_| constructed in the UI thread, used and destroyed in
  // |backend_task_runner_|.
  scoped_refptr<StandaloneTrustedVaultBackend> backend_;

  // Observes changes of accounts state and populates them into |backend_|.
  // Holds references to |backend_| and |backend_task_runner_|.
  std::unique_ptr<signin::IdentityManager::Observer> identity_manager_observer_;

  base::WeakPtrFactory<StandaloneTrustedVaultClient> weak_ptr_factory_{this};
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_CLIENT_H_
