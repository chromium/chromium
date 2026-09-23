// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_IMPL_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace wallet {
class PrivatePass;
}

namespace autofill {

class WalletPassAccessManagerImpl : public EntityDataManager::Observer,
                                    public WalletPassAccessManager {
 public:
  static constexpr base::TimeDelta kCacheTTL = base::Minutes(1);

  explicit WalletPassAccessManagerImpl(
      std::unique_ptr<wallet::WalletHttpClient> http_client,
      EntityDataManager* data_manager);
  ~WalletPassAccessManagerImpl() override;

  // EntityDataManager::Observer:
  // Clears cache entries for entities that changed.
  void OnEntityInstancesChanged() override;

  // WalletPassAccessManager:
  void SaveWalletEntityInstance(
      const EntityInstance& entity,
      const consent_auditor::ConsentAuditor::SessionId& session_id,
      UpsertEntityInstanceCallback callback) override;
  void UpdateWalletEntityInstance(
      const EntityInstance& entity,
      UpsertEntityInstanceCallback callback) override;
  void GetUnmaskedWalletEntityInstance(
      const EntityInstance::EntityId& entity_id,
      GetUnmaskedEntityInstanceCallback callback) override;
  void PreloadDetailsForUpsertPass(EntityType entity_type) override;
  void GetDetailsForUpsertPass(
      EntityType entity_type,
      GetDetailsForUpsertPassCallback callback) override;

 private:
  // Constructs a callback that takes the response of an
  // WalletHttpClient::GetUnmaskedPass call and convert it into an unmasked
  // EntityInstance. This is done by unmasking the `masked_entity` using the
  // pass number from the response.
  base::OnceCallback<std::optional<EntityInstance>(
      const base::expected<wallet::PrivatePass,
                           wallet::WalletHttpClient::WalletRequestError>&)>
  GetUnmaskResponseToUnmaskedEntityCallback(
      const EntityInstance& masked_entity) const;

  // Constructs a callback that takes the response of an
  // WalletHttpClient::UpsertPrivatePass call and convert it into a mask
  // EntityInstance. This is done by masking the `unmasked_entity` using the
  // pass number from the response. The unmasked entity's ID is overwritten with
  // the pass ID of the response, since the server-side assigns IDs for new
  // passes.
  base::OnceCallback<std::optional<EntityInstance>(
      const base::expected<wallet::PrivatePass,
                           wallet::WalletHttpClient::WalletRequestError>&)>
  GetUpsertResponseToMaskedEntityCallback(
      const EntityInstance& unmasked_entity) const;

  // Caches an unmasked `entity`, so it can be refilled without an additional
  // network round trip for `kCacheTTL`.
  // Besides updating `unmasked_entity_cache_`, the function posts a delayed
  // task that clears the cache entry again.
  void CacheUnmaskResult(EntityInstance entity);

  // Helper to make the HTTP request via `http_client_` and transform the
  // response to `GetDetailsForUpsertPassResponse`.
  void FetchDetailsForUpsertPass(wallet::WalletHttpClient::PassType pass_type,
                                 GetDetailsForUpsertPassCallback callback);

  // Invoked when an asynchronous background preload request completes.
  // Stores the successful response in `upsert_details_cache_`.
  void OnPreloadDetailsForUpsertPassComplete(
      wallet::WalletHttpClient::PassType pass_type,
      base::expected<GetDetailsForUpsertPassResponse,
                     wallet::WalletHttpClient::WalletRequestError> response);

  const std::unique_ptr<wallet::WalletHttpClient> http_client_;
  const raw_ref<EntityDataManager> data_manager_;

  // Cache of recently unmasked entity instances.
  // Cache entries are cleared after `kCacheTTL` (see `CacheUnmaskResult()`) or
  // if the corresponding entity is changed.
  absl::flat_hash_map<EntityInstance::EntityId, EntityInstance>
      unmasked_entity_cache_;

  // Cache of preloaded details for upserting passes.
  // Entries are populated by `PreloadDetailsForUpsertPass` and consumed on
  // read via `GetDetailsForUpsertPass` because Google Wallet `context_token`s
  // are single-use. Because tokens and disclosure lines validate user consent
  // for a given pass type and do not depend on client-side entity instances,
  // entries are not affected by entity data changes.
  absl::flat_hash_map<wallet::WalletHttpClient::PassType,
                      GetDetailsForUpsertPassResponse>
      upsert_details_cache_;

  // Set of pass types with active in-flight background preloads to prevent
  // duplicate network requests.
  absl::flat_hash_set<wallet::WalletHttpClient::PassType> in_flight_preloads_;

  base::ScopedObservation<EntityDataManager, EntityDataManager::Observer>
      data_manager_observer_{this};

  base::WeakPtrFactory<WalletPassAccessManagerImpl> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_IMPL_H_
