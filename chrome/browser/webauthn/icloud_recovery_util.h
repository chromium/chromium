// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ICLOUD_RECOVERY_UTIL_H_
#define CHROME_BROWSER_WEBAUTHN_ICLOUD_RECOVERY_UTIL_H_

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "chrome/common/chrome_version.h"
#include "components/trusted_vault/trusted_vault_connection.h"

namespace webauthn {

inline constexpr char kICloudKeychainRecoveryKeyAccessGroup[] =
    MAC_TEAM_IDENTIFIER_STRING ".com.google.common.folsom";

enum class ICloudRecoveryError {
  // No local iCloud recovery key matched any security domain recovery member.
  kKeyNotFound,
  // A matching iCloud recovery key was found, but decrypting the wrapped
  // security domain secret failed.
  kDecryptionFailed,
};

using ICloudRecoveryResult =
    base::expected<trusted_vault::TrustedVaultKeyAndVersion,
                   ICloudRecoveryError>;
using ICloudRecoveryCallback = base::OnceCallback<void(ICloudRecoveryResult)>;

// Retrieves local iCloud Keychain recovery keys for the passkeys security
// domain, matches them against `security_domain_icloud_keys`, and decrypts the
// security domain secret with the highest version.
void RecoverSecurityDomainSecretFromICloudKeychain(
    base::span<const trusted_vault::VaultMember> security_domain_icloud_keys,
    ICloudRecoveryCallback callback);

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_ICLOUD_RECOVERY_UTIL_H_
