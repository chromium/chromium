// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"

struct CoreAccountInfo;

namespace syncer {

class SecureBoxKeyPair;
class SecureBoxPublicKey;

enum class TrustedVaultRequestStatus {
  kSuccess,
  // Used when trusted vault request can't be completed successfully due to
  // vault key being outdated or device key being not registered.
  kLocalDataObsolete,
  // Used for all network, http and protocol errors.
  kOtherError
};

// Supports interaction with vault service, all methods must called on trusted
// vault backend sequence.
class TrustedVaultConnection {
 public:
  using RegisterAuthenticationFactorCallback =
      base::OnceCallback<void(TrustedVaultRequestStatus)>;
  using DownloadKeysCallback =
      base::OnceCallback<void(TrustedVaultRequestStatus,
                              const std::vector<std::vector<uint8_t>>& /*keys*/,
                              int /*last_key_version*/)>;

  TrustedVaultConnection() = default;
  TrustedVaultConnection(const TrustedVaultConnection& other) = delete;
  TrustedVaultConnection& operator=(const TrustedVaultConnection& other) =
      delete;
  virtual ~TrustedVaultConnection() = default;

  // Asynchronously attempts to register the authentication factor on the
  // trusted vault server to allow further vault server API calls using this
  // authentication factor. Calls |callback| upon completion.
  virtual void RegisterAuthenticationFactor(
      const CoreAccountInfo& account_info,
      const std::vector<uint8_t>& last_trusted_vault_key,
      int last_trusted_vault_key_version,
      const SecureBoxPublicKey& authentication_factor_public_key,
      RegisterAuthenticationFactorCallback callback) = 0;

  // Asynchronously attempts to download new vault keys from the trusted vault
  // server.
  virtual void DownloadKeys(const CoreAccountInfo& account_info,
                            const std::vector<uint8_t>& last_trusted_vault_key,
                            int last_trusted_vault_key_version,
                            std::unique_ptr<SecureBoxKeyPair> device_key_pair,
                            DownloadKeysCallback callback) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_H_
