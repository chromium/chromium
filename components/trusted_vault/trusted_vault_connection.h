// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

struct CoreAccountInfo;

namespace syncer {

class SecureBoxKeyPair;
class SecureBoxPublicKey;

enum class TrustedVaultRegistrationStatus {
  kSuccess,
  // Used when member corresponding to authentication factor already exists and
  // local keys that were sent as part of the request aren't stale.
  kAlreadyRegistered,
  // Used when trusted vault request can't be completed successfully due to
  // vault key being outdated or device key being not registered.
  kLocalDataObsolete,
  // Used when request wasn't sent due to transient auth error that prevented
  // fetching an access token.
  kTransientAccessTokenFetchError,
  // Used when request wasn't sent due to persistent auth error that prevented
  // fetching an access token.
  kPersistentAccessTokenFetchError,
  // Used when request wasn't sent because primary account changed meanwhile.
  kPrimaryAccountChangeAccessTokenFetchError,
  // Used for all network errors.
  kNetworkError,
  // Used for all http and protocol errors not covered by the above.
  kOtherError,
};

enum class TrustedVaultDownloadKeysStatus {
  kSuccess,
  // Member corresponding to the authentication factor doesn't exist.
  kMemberNotFound,
  // Member corresponding to the authentication factor not registered in the
  // security domain.
  kMembershipNotFound,
  // Membership exists but is corrupted.
  kMembershipCorrupted,
  // Membership exists but is empty.
  kMembershipEmpty,
  // Keys were successfully downloaded and verified, but no new keys exist.
  kNoNewKeys,
  // At least one of the key proofs isn't valid or unable to verify them using
  // latest local trusted vault key (e.g. it's too old).
  kKeyProofsVerificationFailed,
  // Used when request isn't sent due to access token fetching failure.
  kAccessTokenFetchingFailure,
  // Used for all network errors.
  kNetworkError,
  // Used for all http and protocol errors, when no statuses above fits.
  kOtherError,
};

// This enum is used in histograms. These values are persisted to logs. Entries
// should not be renumbered and numeric values should never be reused, only add
// at the end and. Also remember to update in tools/metrics/histograms/enums.xml
// TrustedVaultRecoverabilityStatus enum.
enum class TrustedVaultRecoverabilityStatus {
  // Recoverability status not retrieved due to network, http or protocol error.
  kNotDegraded = 0,
  kDegraded = 1,
  kError = 2,
  kMaxValue = kError,
};

enum class AuthenticationFactorType { kPhysicalDevice, kUnspecified };

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
      base::OnceCallback<void(TrustedVaultRegistrationStatus)>;
  // If registration request was successful without local keys, it means only
  // constant key exists server-side and it's exposed as
  // |vault_key_and_version|.
  using RegisterDeviceWithoutKeysCallback = base::OnceCallback<void(
      TrustedVaultRegistrationStatus,
      const TrustedVaultKeyAndVersion& /*vault_key_and_version*/)>;
  using DownloadNewKeysCallback =
      base::OnceCallback<void(TrustedVaultDownloadKeysStatus,
                              const std::vector<std::vector<uint8_t>>& /*keys*/,
                              int /*last_key_version*/)>;
  using IsRecoverabilityDegradedCallback =
      base::OnceCallback<void(TrustedVaultRecoverabilityStatus)>;

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
  // |trusted_vault_keys| must be ordered by version and must not be empty.
  [[nodiscard]] virtual std::unique_ptr<Request> RegisterAuthenticationFactor(
      const CoreAccountInfo& account_info,
      const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
      int last_trusted_vault_key_version,
      const SecureBoxPublicKey& authentication_factor_public_key,
      AuthenticationFactorType authentication_factor_type,
      absl::optional<int> authentication_factor_type_hint,
      RegisterAuthenticationFactorCallback callback) = 0;

  // Special version of the above for the case where the caller has no local
  // keys available. Attempts to register the device using constant key. May
  // succeed only if constant key is the only key known server-side.
  [[nodiscard]] virtual std::unique_ptr<Request> RegisterDeviceWithoutKeys(
      const CoreAccountInfo& account_info,
      const SecureBoxPublicKey& device_public_key,
      RegisterDeviceWithoutKeysCallback callback) = 0;

  // Asynchronously attempts to download new vault keys (e.g. keys with version
  // greater than the on in |last_trusted_vault_key_and_version|) from the
  // trusted vault server. Caller should hold returned request object until
  // |callback| call or until request needs to be cancelled.
  [[nodiscard]] virtual std::unique_ptr<Request> DownloadNewKeys(
      const CoreAccountInfo& account_info,
      const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
      std::unique_ptr<SecureBoxKeyPair> device_key_pair,
      DownloadNewKeysCallback callback) = 0;

  // Asynchronously attempts to download degraded recoverability status from the
  // trusted vault server. Caller should hold returned request object until
  // |callback| call or until request needs to be cancelled.
  [[nodiscard]] virtual std::unique_ptr<Request>
  DownloadIsRecoverabilityDegraded(
      const CoreAccountInfo& account_info,
      IsRecoverabilityDegradedCallback callback) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_H_
