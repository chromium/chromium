// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_KEY_AND_VERSION_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_KEY_AND_VERSION_H_

#include <cstdint>
#include <vector>

namespace trusted_vault {

// Holds a trusted vault (security domain) shared key together with its
// version (also referred to as key epoch on the server).
struct TrustedVaultKeyAndVersion {
  TrustedVaultKeyAndVersion(std::vector<uint8_t> key, int version);
  TrustedVaultKeyAndVersion(const TrustedVaultKeyAndVersion& other);
  TrustedVaultKeyAndVersion& operator=(const TrustedVaultKeyAndVersion& other);
  TrustedVaultKeyAndVersion(TrustedVaultKeyAndVersion&& other) noexcept;
  TrustedVaultKeyAndVersion& operator=(
      TrustedVaultKeyAndVersion&& other) noexcept;
  ~TrustedVaultKeyAndVersion();

  friend bool operator==(const TrustedVaultKeyAndVersion&,
                         const TrustedVaultKeyAndVersion&) = default;

  // Raw shared key bytes.
  std::vector<uint8_t> key;

  // Key version (epoch) assigned by the security domain service.
  int version = 0;
};

// Returns a vector of `TrustedVaultKeyAndVersion` given a vector of keys and
// the version of the last key, assuming that the versions are sequential.
std::vector<TrustedVaultKeyAndVersion> GetTrustedVaultKeysWithVersions(
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
    int last_key_version);

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_KEY_AND_VERSION_H_
