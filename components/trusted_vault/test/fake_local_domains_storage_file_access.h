// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TEST_FAKE_LOCAL_DOMAINS_STORAGE_FILE_ACCESS_H_
#define COMPONENTS_TRUSTED_VAULT_TEST_FAKE_LOCAL_DOMAINS_STORAGE_FILE_ACCESS_H_

#include "components/trusted_vault/local_domains_storage.h"
#include "components/trusted_vault/proto/local_domains_data.pb.h"

namespace trusted_vault {

class FakeLocalDomainsStorageFileAccess
    : public LocalDomainsStorage::StorageFileAccess {
 public:
  FakeLocalDomainsStorageFileAccess();
  FakeLocalDomainsStorageFileAccess(const FakeLocalDomainsStorageFileAccess&) =
      delete;
  FakeLocalDomainsStorageFileAccess& operator=(
      const FakeLocalDomainsStorageFileAccess&) = delete;
  ~FakeLocalDomainsStorageFileAccess() override;

  // LocalDomainsStorage::StorageFileAccess implementation:
  trusted_vault_pb::LocalDomainsData ReadFromDisk() override;
  void WriteToDisk(const trusted_vault_pb::LocalDomainsData& data) override;

  void SetStoredLocalDomainsData(
      const trusted_vault_pb::LocalDomainsData& data);
  trusted_vault_pb::LocalDomainsData GetStoredLocalDomainsData() const;

 private:
  trusted_vault_pb::LocalDomainsData stored_data_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TEST_FAKE_LOCAL_DOMAINS_STORAGE_FILE_ACCESS_H_
