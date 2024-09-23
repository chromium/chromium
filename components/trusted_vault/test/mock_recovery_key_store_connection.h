// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TEST_MOCK_RECOVERY_KEY_STORE_CONNECTION_H_
#define COMPONENTS_TRUSTED_VAULT_TEST_MOCK_RECOVERY_KEY_STORE_CONNECTION_H_

#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/recovery_key_store_connection.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace trusted_vault {

class MockRecoveryKeyStoreConnection : public RecoveryKeyStoreConnection {
 public:
  MockRecoveryKeyStoreConnection();
  ~MockRecoveryKeyStoreConnection() override;

  MOCK_METHOD(std::unique_ptr<Request>,
              UpdateRecoveryKeyStore,
              (const CoreAccountInfo& account_info,
               const trusted_vault_pb::Vault& request,
               UpdateRecoveryKeyStoreCallback callback),
              (override));
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TEST_MOCK_RECOVERY_KEY_STORE_CONNECTION_H_
