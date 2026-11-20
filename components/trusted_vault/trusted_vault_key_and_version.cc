// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_key_and_version.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace trusted_vault {

TrustedVaultKeyAndVersion::TrustedVaultKeyAndVersion(std::vector<uint8_t> key,
                                                     int version)
    : key(std::move(key)), version(version) {}

TrustedVaultKeyAndVersion::TrustedVaultKeyAndVersion(
    const TrustedVaultKeyAndVersion& other) = default;

TrustedVaultKeyAndVersion& TrustedVaultKeyAndVersion::operator=(
    const TrustedVaultKeyAndVersion& other) = default;

TrustedVaultKeyAndVersion::TrustedVaultKeyAndVersion(
    TrustedVaultKeyAndVersion&& other) noexcept = default;

TrustedVaultKeyAndVersion& TrustedVaultKeyAndVersion::operator=(
    TrustedVaultKeyAndVersion&& other) noexcept = default;

TrustedVaultKeyAndVersion::~TrustedVaultKeyAndVersion() = default;

std::vector<TrustedVaultKeyAndVersion> GetTrustedVaultKeysWithVersions(
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
    int last_key_version) {
  if (trusted_vault_keys.empty()) {
    return {};
  }
  const int first_key_version =
      last_key_version - static_cast<int>(trusted_vault_keys.size()) + 1;
  std::vector<TrustedVaultKeyAndVersion> result;
  result.reserve(trusted_vault_keys.size());
  for (size_t i = 0; i < trusted_vault_keys.size(); ++i) {
    result.emplace_back(trusted_vault_keys[i],
                        first_key_version + static_cast<int>(i));
  }
  return result;
}

}  // namespace trusted_vault
