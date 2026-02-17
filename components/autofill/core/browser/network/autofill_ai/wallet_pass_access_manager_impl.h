// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_IMPL_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

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
  void SaveWalletEntityInstance(const EntityInstance& entity,
                                UpsertEntityInstanceCallback callback) override;
  void UpdateWalletEntityInstance(
      const EntityInstance& entity,
      UpsertEntityInstanceCallback callback) override;
  void GetUnmaskedWalletEntityInstance(
      const EntityInstance::EntityId& entity_id,
      GetUnmaskedEntityInstanceCallback callback) override;

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

  const std::unique_ptr<wallet::WalletHttpClient> http_client_;
  const raw_ref<EntityDataManager> data_manager_;

  // Cache of recently unmasked entity instances.
  // Cache entries are cleared after `kCacheTTL` (see `CacheUnmaskResult()`) or
  // if the corresponding entity is changed.
  absl::flat_hash_map<EntityInstance::EntityId, EntityInstance>
      unmasked_entity_cache_;

  base::ScopedObservation<EntityDataManager, EntityDataManager::Observer>
      data_manager_observer_{this};

  base::WeakPtrFactory<WalletPassAccessManagerImpl> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_IMPL_H_
