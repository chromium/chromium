// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_IMPL_H_

#include <memory>

#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"

namespace wallet {
class WalletHttpClient;
}

namespace autofill {

class EntityDataManager;

class WalletPassAccessManagerImpl : public WalletPassAccessManager {
 public:
  explicit WalletPassAccessManagerImpl(
      std::unique_ptr<wallet::WalletHttpClient> http_client,
      const EntityDataManager* data_manager);
  ~WalletPassAccessManagerImpl() override;

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
  const std::unique_ptr<wallet::WalletHttpClient> http_client_;
  const raw_ref<const EntityDataManager> data_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_IMPL_H_
