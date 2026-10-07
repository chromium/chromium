// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/icloud_recovery_util.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "components/device_event_log/device_event_log.h"
#include "components/trusted_vault/icloud_recovery_key_mac.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_crypto.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"

namespace webauthn {

namespace {

struct MatchingICloudKey {
  raw_ptr<const trusted_vault::VaultMember> recovery_key = nullptr;
  raw_ptr<const trusted_vault::SecureBoxKeyPair> local_key = nullptr;
};

std::optional<MatchingICloudKey> FindMatchingICloudKey(
    base::span<const trusted_vault::VaultMember> security_domain_icloud_keys,
    base::span<const std::unique_ptr<trusted_vault::ICloudRecoveryKey>>
        local_icloud_keys) {
  for (const trusted_vault::VaultMember& recovery_key :
       security_domain_icloud_keys) {
    if (!recovery_key.public_key) {
      continue;
    }
    const std::vector<uint8_t> recovery_public_key =
        recovery_key.public_key->ExportToBytes();
    for (const std::unique_ptr<trusted_vault::ICloudRecoveryKey>& local_key :
         local_icloud_keys) {
      if (local_key && local_key->key() &&
          local_key->id() == recovery_public_key) {
        return MatchingICloudKey{&recovery_key, local_key->key()};
      }
    }
  }
  return std::nullopt;
}

// `trusted_vault::VaultMember` is move-only because it owns a
// `std::unique_ptr<SecureBoxPublicKey>`. Deep-copies members with valid public
// keys so they can be owned across the asynchronous
// `ICloudRecoveryKey::Retrieve` call.
std::vector<trusted_vault::VaultMember> CloneVaultMembers(
    base::span<const trusted_vault::VaultMember> members) {
  std::vector<trusted_vault::VaultMember> copy;
  copy.reserve(members.size());
  for (const auto& member : members) {
    if (!member.public_key) {
      continue;
    }
    std::vector<trusted_vault::MemberKeys> member_keys_copy;
    member_keys_copy.reserve(member.member_keys.size());
    for (const auto& mk : member.member_keys) {
      member_keys_copy.emplace_back(mk.version, mk.wrapped_key, mk.proof);
    }
    copy.emplace_back(trusted_vault::SecureBoxPublicKey::CreateByImport(
                          member.public_key->ExportToBytes()),
                      std::move(member_keys_copy));
  }
  return copy;
}

ICloudRecoveryResult RecoverSecurityDomainSecretWithICloudKeys(
    base::span<const trusted_vault::VaultMember> security_domain_icloud_keys,
    base::span<const std::unique_ptr<trusted_vault::ICloudRecoveryKey>>
        local_icloud_keys) {
  // 1. Find the matching pair of local iCloud private key and the security
  // domain recovery member.
  std::optional<MatchingICloudKey> matching_key =
      FindMatchingICloudKey(security_domain_icloud_keys, local_icloud_keys);
  if (!matching_key) {
    FIDO_LOG(DEBUG) << "Could not find matching iCloud recovery key";
    return base::unexpected(ICloudRecoveryError::kKeyNotFound);
  }

  // 2. Select the member key with the highest version.
  if (matching_key->recovery_key->member_keys.empty()) {
    FIDO_LOG(ERROR) << "Matching iCloud recovery key has no member keys";
    return base::unexpected(ICloudRecoveryError::kDecryptionFailed);
  }
  const auto member_key_it =
      std::ranges::max_element(matching_key->recovery_key->member_keys, {},
                               &trusted_vault::MemberKeys::version);

  // 3. Decrypt the wrapped security domain secret.
  std::optional<std::vector<uint8_t>> security_domain_secret =
      trusted_vault::DecryptTrustedVaultWrappedKey(
          matching_key->local_key->private_key(), member_key_it->wrapped_key);
  if (!security_domain_secret) {
    FIDO_LOG(ERROR)
        << "Could not decrypt security domain secret with iCloud key";
    return base::unexpected(ICloudRecoveryError::kDecryptionFailed);
  }

  FIDO_LOG(EVENT) << "Successful recovery from iCloud recovery key";
  return trusted_vault::TrustedVaultKeyAndVersion(
      std::move(*security_domain_secret), member_key_it->version);
}

void OnICloudKeysRetrieved(
    std::vector<trusted_vault::VaultMember> security_domain_icloud_keys,
    ICloudRecoveryCallback callback,
    std::vector<std::unique_ptr<trusted_vault::ICloudRecoveryKey>>
        local_icloud_keys) {
  if (callback.IsCancelled()) {
    return;
  }
  std::move(callback).Run(RecoverSecurityDomainSecretWithICloudKeys(
      security_domain_icloud_keys, local_icloud_keys));
}

}  // namespace

void RecoverSecurityDomainSecretFromICloudKeychain(
    base::span<const trusted_vault::VaultMember> security_domain_icloud_keys,
    ICloudRecoveryCallback callback) {
  trusted_vault::ICloudRecoveryKey::Retrieve(
      base::BindOnce(&OnICloudKeysRetrieved,
                     CloneVaultMembers(security_domain_icloud_keys),
                     std::move(callback)),
      trusted_vault::SecurityDomainId::kPasskeys,
      kICloudKeychainRecoveryKeyAccessGroup);
}

}  // namespace webauthn
