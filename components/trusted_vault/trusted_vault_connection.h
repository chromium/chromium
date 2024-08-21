// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

struct CoreAccountInfo;

namespace trusted_vault {

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
// LINT.IfChange(TrustedVaultRecoverabilityStatus)
enum class TrustedVaultRecoverabilityStatus {
  // Recoverability status not retrieved due to network, http or protocol error.
  kNotDegraded = 0,
  kDegraded = 1,
  kError = 2,
  kMaxValue = kError,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:TrustedVaultRecoverabilityStatus)

// Contains information about a Google Password Manager PIN that is stored in
// a trusted vault.
struct GpmPinMetadata {
  GpmPinMetadata(std::optional<std::string> public_key,
                 std::string wrapped_pin,
                 base::Time expiry);
  GpmPinMetadata(const GpmPinMetadata&);
  GpmPinMetadata& operator=(const GpmPinMetadata&);
  GpmPinMetadata(GpmPinMetadata&&);
  GpmPinMetadata& operator=(GpmPinMetadata&&);
  ~GpmPinMetadata();

  bool operator==(const GpmPinMetadata&) const;

  // The securebox public key for the virtual member. This will always have a
  // value when this metadata is downloaded with
  // `DownloadAuthenticationFactorsRegistrationState`. When used with
  // `RegisterAuthenticationFactor`, this can be empty to upload the first GPM
  // PIN to an account, or non-empty to replace a GPM PIN.
  std::optional<std::string> public_key;
  // The encrypted PIN value, for validation.
  std::string wrapped_pin;
  // The time when the underlying recovery-key-store entry will expire. Ignored
  // when uploading.
  base::Time expiry;
};

// A MemberKeys contains the cryptographic outputs needed to add or use an
// authentication factor: the trusted vault key, encrypted to the public key of
// the member, and an authenticator of that public key.
struct MemberKeys {
  MemberKeys(int version,
             std::vector<uint8_t> wrapped_key,
             std::vector<uint8_t> proof);
  MemberKeys(const MemberKeys&) = delete;
  MemberKeys& operator=(const MemberKeys&) = delete;
  MemberKeys(MemberKeys&&);
  MemberKeys& operator=(MemberKeys&&);
  ~MemberKeys();

  int version;
  std::vector<uint8_t> wrapped_key;
  std::vector<uint8_t> proof;
};

// A vault member public key and its member keys.
struct VaultMember {
  VaultMember(std::unique_ptr<SecureBoxPublicKey> public_key,
              std::vector<MemberKeys> member_keys);
  VaultMember(const VaultMember&) = delete;
  VaultMember& operator=(const VaultMember&) = delete;
  VaultMember(VaultMember&&);
  VaultMember& operator=(VaultMember&&);
  ~VaultMember();

  std::unique_ptr<SecureBoxPublicKey> public_key;
  std::vector<MemberKeys> member_keys;
};

// The result of calling
// DownloadAuthenticationFactorsRegistrationState.
struct DownloadAuthenticationFactorsRegistrationStateResult {
  DownloadAuthenticationFactorsRegistrationStateResult();
  DownloadAuthenticationFactorsRegistrationStateResult(
      DownloadAuthenticationFactorsRegistrationStateResult&&);
  DownloadAuthenticationFactorsRegistrationStateResult& operator=(
      DownloadAuthenticationFactorsRegistrationStateResult&&);
  ~DownloadAuthenticationFactorsRegistrationStateResult();

  // These values are persisted in histograms. Entries should not be renumbered
  // and numeric values should never be reused.
  enum class State {
    // The state of the security domain could not be determined.
    kError = 0,
    // The security domain is empty and thus doesn't have any secrets.
    kEmpty = 1,
    // The security domain is non-empty, but has virtual devices that are valid
    // for recovery.
    kRecoverable = 2,
    // The security domain is non-empty, but has no virtual devices that can be
    // used for recovery.
    kIrrecoverable = 3,
    kMaxValue = kIrrecoverable,
  };
  State state = State::kError;

  // If there are members in the domain then this will contain the current key
  // version.
  std::optional<int> key_version;

  // The expiry time of any LSKF virtual devices.
  std::vector<base::Time> lskf_expiries;

  // If a Google Password Manager PIN is a member of the domain, and is usable
  // for retrieval, then this will contain its metadata.
  std::optional<GpmPinMetadata> gpm_pin_metadata;

  // The list of iCloud recovery key domain members.
  std::vector<VaultMember> icloud_keys;
};

// Authentication factor types:
using LocalPhysicalDevice =
    base::StrongAlias<class LocalPhysicalDeviceTag, absl::monostate>;
using LockScreenKnowledgeFactor =
    base::StrongAlias<class VirtualDeviceTag, absl::monostate>;
using ICloudKeychain =
    base::StrongAlias<class ICloudKeychainTag, absl::monostate>;
// UnspecifiedAuthenticationFactorType carries a type hint for the backend.
using UnspecifiedAuthenticationFactorType =
    base::StrongAlias<class UnspecifiedAuthenticationFactorTypeTag, int>;

using AuthenticationFactorType =
    absl::variant<LocalPhysicalDevice,
                  LockScreenKnowledgeFactor,
                  UnspecifiedAuthenticationFactorType,
                  GpmPinMetadata,
                  ICloudKeychain>;

struct TrustedVaultKeyAndVersion {
  TrustedVaultKeyAndVersion(const std::vector<uint8_t>& key, int version);
  TrustedVaultKeyAndVersion(const TrustedVaultKeyAndVersion& other);
  TrustedVaultKeyAndVersion& operator=(const TrustedVaultKeyAndVersion& other);
  ~TrustedVaultKeyAndVersion();

  bool operator==(const TrustedVaultKeyAndVersion& other) const;

  std::vector<uint8_t> key;
  int version;
};

// Returns a vector of `TrustedVaultKeyAndVersion` given a vector of keys and
// the version of the last key, assuming that the versions are sequential.
std::vector<TrustedVaultKeyAndVersion> GetTrustedVaultKeysWithVersions(
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
    int last_key_version);

// A MemberKeysSource provides a method of calculating the values needed to
// add an authenticator factor.
using MemberKeysSource =
    absl::variant<std::vector<TrustedVaultKeyAndVersion>, MemberKeys>;

// Supports interaction with vault service, all methods must called on trusted
// vault backend sequence.
class TrustedVaultConnection {
 public:
  // The result of attempting to add a member to the security domain. If the
  // status is successful then `key_version` carries the current version of
  // the security domain, otherwise it's zero.
  using RegisterAuthenticationFactorCallback =
      base::OnceCallback<void(TrustedVaultRegistrationStatus,
                              /*key_version=*/int)>;
  using DownloadNewKeysCallback =
      base::OnceCallback<void(TrustedVaultDownloadKeysStatus,
                              const std::vector<std::vector<uint8_t>>& /*keys*/,
                              int /*last_key_version*/)>;
  using IsRecoverabilityDegradedCallback =
      base::OnceCallback<void(TrustedVaultRecoverabilityStatus)>;
  using DownloadAuthenticationFactorsRegistrationStateCallback =
      base::OnceCallback<void(
          DownloadAuthenticationFactorsRegistrationStateResult)>;

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
      const MemberKeysSource& member_keys_source,
      const SecureBoxPublicKey& authentication_factor_public_key,
      AuthenticationFactorType authentication_factor_type,
      RegisterAuthenticationFactorCallback callback) = 0;

  // Special version of the above for the case where the caller has no local
  // keys available. Attempts to register the device using constant key. May
  // succeed only if constant key is the only key known server-side.
  [[nodiscard]] virtual std::unique_ptr<Request> RegisterLocalDeviceWithoutKeys(
      const CoreAccountInfo& account_info,
      const SecureBoxPublicKey& device_public_key,
      RegisterAuthenticationFactorCallback callback) = 0;

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

  // Enumerates the members of the security domain and determines the
  // recoverability of the security domain. (See the values of
  // `DownloadAuthenticationFactorsRegistrationStateResult`.)
  [[nodiscard]] virtual std::unique_ptr<Request>
  DownloadAuthenticationFactorsRegistrationState(
      const CoreAccountInfo& account_info,
      DownloadAuthenticationFactorsRegistrationStateCallback callback) = 0;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_H_
