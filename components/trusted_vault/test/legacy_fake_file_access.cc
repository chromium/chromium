// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/test/legacy_fake_file_access.h"

namespace trusted_vault {

LegacyFakeFileAccess::LegacyFakeFileAccess() = default;
LegacyFakeFileAccess::~LegacyFakeFileAccess() = default;

trusted_vault_pb::LocalTrustedVault LegacyFakeFileAccess::ReadFromDisk() {
  return stored_data_;
}
void LegacyFakeFileAccess::WriteToDisk(
    const trusted_vault_pb::LocalTrustedVault& data) {
  stored_data_ = data;
}

void LegacyFakeFileAccess::SetStoredLocalTrustedVault(
    const trusted_vault_pb::LocalTrustedVault& local_trusted_vault) {
  stored_data_ = local_trusted_vault;
}

trusted_vault_pb::LocalTrustedVault
LegacyFakeFileAccess::GetStoredLocalTrustedVault() const {
  return stored_data_;
}

}  // namespace trusted_vault
