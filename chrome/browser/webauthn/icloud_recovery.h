// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ICLOUD_RECOVERY_H_
#define CHROME_BROWSER_WEBAUTHN_ICLOUD_RECOVERY_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_span.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/common/chrome_version.h"
#include "components/trusted_vault/trusted_vault_connection.h"

namespace trusted_vault {
class ICloudRecoveryKey;
}  // namespace trusted_vault

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
// domain, matches them against `keys`, and decrypts the security domain secret
// with the highest version.
class ICloudRecovery {
 public:
  // `keys` must outlive this object.
  ICloudRecovery(base::span<const trusted_vault::VaultMember> keys,
                 ICloudRecoveryCallback callback);
  ICloudRecovery(const ICloudRecovery&) = delete;
  ICloudRecovery& operator=(const ICloudRecovery&) = delete;
  ~ICloudRecovery();

  void Start();

 private:
  void OnICloudKeysRetrieved(
      std::vector<std::unique_ptr<trusted_vault::ICloudRecoveryKey>>
          local_icloud_keys);

  base::raw_span<const trusted_vault::VaultMember> keys_;
  ICloudRecoveryCallback callback_;
  base::WeakPtrFactory<ICloudRecovery> weak_ptr_factory_{this};
};

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_ICLOUD_RECOVERY_H_
