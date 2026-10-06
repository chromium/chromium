// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/test/fake_local_domains_storage_file_access.h"

namespace trusted_vault {

FakeLocalDomainsStorageFileAccess::FakeLocalDomainsStorageFileAccess() =
    default;
FakeLocalDomainsStorageFileAccess::~FakeLocalDomainsStorageFileAccess() =
    default;

trusted_vault_pb::LocalDomainsData
FakeLocalDomainsStorageFileAccess::ReadFromDisk() {
  trusted_vault_pb::LocalDomainsData data = stored_data_;
  if (!data.has_data_version() || data.data_version() == 0) {
    data.set_data_version(1);
  }
  return data;
}

void FakeLocalDomainsStorageFileAccess::WriteToDisk(
    const trusted_vault_pb::LocalDomainsData& data) {
  stored_data_ = data;
}

void FakeLocalDomainsStorageFileAccess::SetStoredLocalDomainsData(
    const trusted_vault_pb::LocalDomainsData& data) {
  stored_data_ = data;
}

trusted_vault_pb::LocalDomainsData
FakeLocalDomainsStorageFileAccess::GetStoredLocalDomainsData() const {
  return stored_data_;
}

}  // namespace trusted_vault
