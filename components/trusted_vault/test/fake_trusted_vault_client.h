// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TEST_FAKE_TRUSTED_VAULT_CLIENT_H_
#define COMPONENTS_TRUSTED_VAULT_TEST_FAKE_TRUSTED_VAULT_CLIENT_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "components/trusted_vault/trusted_vault_client.h"

struct CoreAccountInfo;

namespace trusted_vault {

// Fake in-memory implementation of TrustedVaultClient.
class FakeTrustedVaultClient : public TrustedVaultClient {
 public:
  class FakeServer {
   public:
    FakeServer();
    ~FakeServer();

    void StoreKeysOnServer(const std::string& gaia_id,
                           const std::vector<std::vector<uint8_t>>& keys);

    // Mimics a user going through a key-retrieval flow (e.g. reauth) such that
    // keys are fetched from the server and cached in |client|.
    // TODO(crbug.com/1434667): replace usages with GetKeysFromServer() +
    // client.StoreKeys()?
    void MimicKeyRetrievalByUser(const std::string& gaia_id,
                                 TrustedVaultClient* client);

    // Mimics the server RPC endpoint that allows key rotation.
    std::vector<std::vector<uint8_t>> RequestRotatedKeysFromServer(
        const std::string& gaia_id,
        const std::vector<uint8_t>& key_known_by_client);

   private:
    std::map<std::string, std::vector<std::vector<uint8_t>>> gaia_id_to_keys_;
  };

  FakeTrustedVaultClient();
  ~FakeTrustedVaultClient() override;

  FakeServer* server() { return &server_; }

  // Exposes the total number of calls to FetchKeys().
  int fetch_count() const { return fetch_count_; }

  // Exposes the total number of calls to MarkLocalKeysAsStale().
  bool keys_marked_as_stale_count() const {
    return keys_marked_as_stale_count_;
  }

  // Exposes the total number of calls to the server's RequestKeysFromServer().
  int server_request_count() const { return server_request_count_; }

  // Exposes the total number of calls to GetIsRecoverabilityDegraded().
  int get_is_recoverablity_degraded_call_count() const {
    return get_is_recoverablity_degraded_call_count_;
  }

  // Similar to FetchKeys(), but synchronous and never requests new keys from
  // `server_`.
  std::vector<std::vector<uint8_t>> GetStoredKeys(
      const std::string& gaia_id) const;

  // Mimics the completion of all FetchKeys() and GetIsRecoverabilityDegraded()
  // requests.
  bool CompleteAllPendingRequests();

  void SetIsRecoverabilityDegraded(bool is_recoverability_degraded);

  // TrustedVaultClient implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void FetchKeys(
      const CoreAccountInfo& account_info,
      base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>
          callback) override;
  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int last_key_version) override;
  void MarkLocalKeysAsStale(const CoreAccountInfo& account_info,
                            base::OnceCallback<void(bool)> callback) override;
  void GetIsRecoverabilityDegraded(
      const CoreAccountInfo& account_info,
      base::OnceCallback<void(bool)> callback) override;
  void AddTrustedRecoveryMethod(const std::string& gaia_id,
                                const std::vector<uint8_t>& public_key,
                                int method_type_hint,
                                base::OnceClosure callback) override;
  void ClearLocalDataForAccount(const CoreAccountInfo& account_info) override;

 private:
  struct CachedKeysPerUser {
    CachedKeysPerUser();
    ~CachedKeysPerUser();

    bool marked_as_stale = false;
    std::vector<std::vector<uint8_t>> keys;
  };

  FakeServer server_;

  std::map<std::string, CachedKeysPerUser> gaia_id_to_cached_keys_;
  base::ObserverList<Observer> observer_list_;
  int fetch_count_ = 0;
  int keys_marked_as_stale_count_ = 0;
  int get_is_recoverablity_degraded_call_count_ = 0;
  int server_request_count_ = 0;
  std::vector<base::OnceClosure> pending_responses_;
  bool is_recoverability_degraded_ = false;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TEST_FAKE_TRUSTED_VAULT_CLIENT_H_
