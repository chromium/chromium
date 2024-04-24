// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_connection.h"

#include "components/trusted_vault/securebox.h"

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

GpmPinMetadata::GpmPinMetadata(std::optional<std::string> in_public_key,
                               std::string in_wrapped_pin,
                               base::Time in_expiry)
    : public_key(std::move(in_public_key)),
      wrapped_pin(std::move(in_wrapped_pin)),
      expiry(in_expiry) {}

GpmPinMetadata::GpmPinMetadata(const GpmPinMetadata&) = default;

GpmPinMetadata& GpmPinMetadata::operator=(const GpmPinMetadata&) = default;

GpmPinMetadata::GpmPinMetadata(GpmPinMetadata&&) = default;

GpmPinMetadata& GpmPinMetadata::operator=(GpmPinMetadata&&) = default;

GpmPinMetadata::~GpmPinMetadata() = default;

bool GpmPinMetadata::operator==(const GpmPinMetadata&) const = default;

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
