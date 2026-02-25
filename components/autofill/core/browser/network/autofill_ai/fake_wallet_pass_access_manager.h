// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_FAKE_WALLET_PASS_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_FAKE_WALLET_PASS_ACCESS_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"

namespace autofill {

// A fake implementation of `WalletPassAccessManager` for testing and UI
// development.
//
// When the feature `kFakeWalletApiResponses` is enabled, this class
// intercepts calls to the Wallet API and returns locally simulated responses.
// The behavior of the fake responses can be configured via feature params:
// - `delay_ms`: To simulate network latency.
// - `simulate_failure`: To force all responses to fail.
//
// Note: This fake only supports unmasking entities that were previously
// upserted during the current session.
class FakeWalletPassAccessManager : public WalletPassAccessManager {
 public:
  explicit FakeWalletPassAccessManager(EntityDataManager* data_manager);
  ~FakeWalletPassAccessManager() override;

  FakeWalletPassAccessManager(const FakeWalletPassAccessManager&) = delete;
  FakeWalletPassAccessManager& operator=(const FakeWalletPassAccessManager&) =
      delete;

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
  std::optional<EntityInstance> RunUpsertCallback(EntityInstance entity);
  std::optional<EntityInstance> RunGetUnmaskedCallback(
      EntityInstance::EntityId entity_id);

  // Cache to store the unmasked state of upserted entities. This allows us to
  // unmask Chrome-upserted passes to their upserted unmasked value.
  absl::flat_hash_map<EntityInstance::EntityId, EntityInstance>
      upserted_unmasked_entities_;

  const raw_ref<EntityDataManager> data_manager_;
  base::WeakPtrFactory<FakeWalletPassAccessManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_FAKE_WALLET_PASS_ACCESS_MANAGER_H_
