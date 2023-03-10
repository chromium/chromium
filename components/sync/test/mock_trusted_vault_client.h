// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_TRUSTED_VAULT_CLIENT_H_
#define COMPONENTS_SYNC_TEST_MOCK_TRUSTED_VAULT_CLIENT_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/sync/driver/trusted_vault_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class MockTrustedVaultClient : public TrustedVaultClient {
 public:
  MockTrustedVaultClient();
  ~MockTrustedVaultClient() override;

  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));

  MOCK_METHOD(
      void,
      FetchKeys,
      (const CoreAccountInfo&,
       base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>),
      (override));
  MOCK_METHOD(void,
              MarkLocalKeysAsStale,
              (const CoreAccountInfo&, base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              StoreKeys,
              (const std::string&,
               const std::vector<std::vector<uint8_t>>&,
               int),
              (override));
  MOCK_METHOD(void,
              GetIsRecoverabilityDegraded,
              (const CoreAccountInfo&, base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(
      void,
      AddTrustedRecoveryMethod,
      (const std::string&, const std::vector<uint8_t>&, int, base::OnceClosure),
      (override));
  MOCK_METHOD(void,
              ClearLocalDataForAccount,
              (const CoreAccountInfo&),
              (override));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_TRUSTED_VAULT_CLIENT_H_
