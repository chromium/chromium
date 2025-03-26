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

UsableRecoveryPinMetadata::UsableRecoveryPinMetadata(std::string in_wrapped_pin,
                                                     base::Time in_expiry)
    : wrapped_pin(std::move(in_wrapped_pin)), expiry(in_expiry) {}

UsableRecoveryPinMetadata::UsableRecoveryPinMetadata(
    const UsableRecoveryPinMetadata&) = default;

UsableRecoveryPinMetadata& UsableRecoveryPinMetadata::operator=(
    const UsableRecoveryPinMetadata&) = default;

UsableRecoveryPinMetadata::UsableRecoveryPinMetadata(
    UsableRecoveryPinMetadata&&) = default;

UsableRecoveryPinMetadata& UsableRecoveryPinMetadata::operator=(
    UsableRecoveryPinMetadata&&) = default;

UsableRecoveryPinMetadata::~UsableRecoveryPinMetadata() = default;

bool UsableRecoveryPinMetadata::operator==(
    const UsableRecoveryPinMetadata&) const = default;

GpmPinMetadata::GpmPinMetadata(
    std::optional<std::string> in_public_key,
    std::optional<UsableRecoveryPinMetadata> in_pin_metadata)
    : public_key(std::move(in_public_key)),
      usable_pin_metadata(std::move(in_pin_metadata)) {}

GpmPinMetadata::GpmPinMetadata(const GpmPinMetadata&) = default;

GpmPinMetadata& GpmPinMetadata::operator=(const GpmPinMetadata&) = default;

GpmPinMetadata::GpmPinMetadata(GpmPinMetadata&&) = default;

GpmPinMetadata& GpmPinMetadata::operator=(GpmPinMetadata&&) = default;

GpmPinMetadata::~GpmPinMetadata() = default;

bool GpmPinMetadata::operator==(const GpmPinMetadata&) const = default;

MemberKeys::MemberKeys(int in_version,
                       std::vector<uint8_t> in_wrapped_key,
                       std::vector<uint8_t> in_proof)
    : version(in_version),
      wrapped_key(std::move(in_wrapped_key)),
      proof(std::move(in_proof)) {}

MemberKeys::MemberKeys(MemberKeys&&) = default;

MemberKeys& MemberKeys::operator=(MemberKeys&&) = default;

MemberKeys::~MemberKeys() = default;

VaultMember::VaultMember(std::unique_ptr<SecureBoxPublicKey> public_key,
                         std::vector<MemberKeys> member_keys)
    : public_key(std::move(public_key)), member_keys(std::move(member_keys)) {}

VaultMember::VaultMember(VaultMember&&) = default;

VaultMember& VaultMember::operator=(VaultMember&&) = default;

VaultMember::~VaultMember() = default;

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
