// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/test/fake_file_access.h"

namespace trusted_vault {

FakeFileAccess::FakeFileAccess() = default;
FakeFileAccess::~FakeFileAccess() = default;

trusted_vault_pb::LocalTrustedVault FakeFileAccess::ReadFromDisk() {
  return stored_data_;
}
void FakeFileAccess::WriteToDisk(
    const trusted_vault_pb::LocalTrustedVault& data) {
  stored_data_ = data;
}

void FakeFileAccess::SetStoredLocalTrustedVault(
    const trusted_vault_pb::LocalTrustedVault& local_trusted_vault) {
  stored_data_ = local_trusted_vault;
}

trusted_vault_pb::LocalTrustedVault FakeFileAccess::GetStoredLocalTrustedVault()
    const {
  return stored_data_;
}

}  // namespace trusted_vault
