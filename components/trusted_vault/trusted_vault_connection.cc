// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_connection.h"

namespace trusted_vault {

TrustedVaultKeyAndVersion::TrustedVaultKeyAndVersion(
    const std::vector<uint8_t>& key,
    int version)
    : key(key), version(version) {}

TrustedVaultKeyAndVersion::TrustedVaultKeyAndVersion(
    const TrustedVaultKeyAndVersion& other) = default;

TrustedVaultKeyAndVersion& TrustedVaultKeyAndVersion::operator=(
    const TrustedVaultKeyAndVersion& other) = default;

TrustedVaultKeyAndVersion::~TrustedVaultKeyAndVersion() = default;

bool TrustedVaultKeyAndVersion::operator==(
    const TrustedVaultKeyAndVersion& other) const = default;

PrecomputedMemberKeys::PrecomputedMemberKeys(
    int in_version,
    std::vector<uint8_t> in_wrapped_key,
    std::vector<uint8_t> in_proof)
    : version(in_version),
      wrapped_key(std::move(in_wrapped_key)),
      proof(std::move(in_proof)) {}

PrecomputedMemberKeys::PrecomputedMemberKeys(PrecomputedMemberKeys&&) = default;

PrecomputedMemberKeys& PrecomputedMemberKeys::operator=(
    PrecomputedMemberKeys&&) = default;

PrecomputedMemberKeys::~PrecomputedMemberKeys() = default;

DownloadAuthenticationFactorsRegistrationStateResult::
    DownloadAuthenticationFactorsRegistrationStateResult() = default;

DownloadAuthenticationFactorsRegistrationStateResult::
    DownloadAuthenticationFactorsRegistrationStateResult(
        DownloadAuthenticationFactorsRegistrationStateResult&&) = default;

DownloadAuthenticationFactorsRegistrationStateResult&
DownloadAuthenticationFactorsRegistrationStateResult::operator=(
    DownloadAuthenticationFactorsRegistrationStateResult&&) = default;

DownloadAuthenticationFactorsRegistrationStateResult::
    ~DownloadAuthenticationFactorsRegistrationStateResult() = default;

}  // namespace trusted_vault
