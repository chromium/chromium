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

struct TrustedVaultKeyAndVersion {
  TrustedVaultKeyAndVersion(const std::vector<uint8_t>& key, int version);
  TrustedVaultKeyAndVersion(const TrustedVaultKeyAndVersion& other);
  TrustedVaultKeyAndVersion& operator=(const TrustedVaultKeyAndVersion& other);
  ~TrustedVaultKeyAndVersion();

  std::vector<uint8_t> key;
  int version;
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

  // Used to control ongoing request lifetime, destroying Request object causes
  // request cancellation.
  class Request {
   public:
    Request() = default;
    Request(const Request& other) = delete;
    Request& operator=(const Request& other) = delete;
    virtual ~Request() = default;
  };

  TrustedVaultConnection() = default;
  TrustedVaultConnection(const TrustedVaultConnection& other) = delete;
  TrustedVaultConnection& operator=(const TrustedVaultConnection& other) =
      delete;
  virtual ~TrustedVaultConnection() = default;

  // Asynchronously attempts to register the authentication factor on the
  // trusted vault server to allow further vault server API calls using this
  // authentication factor. Calls |callback| upon completion, unless the
  // returned object is destroyed earlier. Caller should hold returned request
  // object until |callback| call or until request needs to be cancelled.
  virtual std::unique_ptr<Request> RegisterAuthenticationFactor(
      const CoreAccountInfo& account_info,
      const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
      const SecureBoxPublicKey& authentication_factor_public_key,
      RegisterAuthenticationFactorCallback callback) WARN_UNUSED_RESULT = 0;

  // Asynchronously attempts to download new vault keys from the trusted vault
  // server. Caller should hold returned request object until |callback| call
  // or until request needs to be cancelled.
  virtual std::unique_ptr<Request> DownloadKeys(
      const CoreAccountInfo& account_info,
      const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
      std::unique_ptr<SecureBoxKeyPair> device_key_pair,
      DownloadKeysCallback callback) WARN_UNUSED_RESULT = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_H_
