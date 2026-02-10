// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_MOCK_WALLET_PASS_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_MOCK_WALLET_PASS_ACCESS_MANAGER_H_

#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockWalletPassAccessManager : public WalletPassAccessManager {
 public:
  MockWalletPassAccessManager();
  ~MockWalletPassAccessManager() override;

  // WalletPassAccessManager:
  MOCK_METHOD(void,
              SaveWalletEntityInstance,
              (const EntityInstance& entity,
               UpsertEntityInstanceCallback callback),
              (override));
  MOCK_METHOD(void,
              UpdateWalletEntityInstance,
              (const EntityInstance& entity,
               UpsertEntityInstanceCallback callback),
              (override));
  MOCK_METHOD(void,
              GetUnmaskedWalletEntityInstance,
              (const EntityInstance::EntityId& entity_id,
               GetUnmaskedEntityInstanceCallback callback),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_MOCK_WALLET_PASS_ACCESS_MANAGER_H_
