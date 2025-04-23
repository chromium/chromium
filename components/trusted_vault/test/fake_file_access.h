// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TEST_FAKE_FILE_ACCESS_H_
#define COMPONENTS_TRUSTED_VAULT_TEST_FAKE_FILE_ACCESS_H_

#include "components/trusted_vault/standalone_trusted_vault_storage.h"

namespace trusted_vault {

class FakeFileAccess : public StandaloneTrustedVaultStorage::FileAccess {
 public:
  FakeFileAccess();
  FakeFileAccess(const FakeFileAccess& other) = delete;
  FakeFileAccess& operator=(const FakeFileAccess& other) = delete;
  ~FakeFileAccess() override;

  trusted_vault_pb::LocalTrustedVault ReadFromDisk() override;
  void WriteToDisk(const trusted_vault_pb::LocalTrustedVault& data) override;

  void SetStoredLocalTrustedVault(
      const trusted_vault_pb::LocalTrustedVault& local_trusted_vault);
  trusted_vault_pb::LocalTrustedVault GetStoredLocalTrustedVault() const;

 private:
  trusted_vault_pb::LocalTrustedVault stored_data_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TEST_FAKE_FILE_ACCESS_H_
