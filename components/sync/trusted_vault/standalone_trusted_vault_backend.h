// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_BACKEND_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_BACKEND_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/trusted_vault_histograms.h"
#include "components/sync/protocol/local_trusted_vault.pb.h"
#include "components/sync/trusted_vault/trusted_vault_connection.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Clock;
}  // namespace base

namespace signin {
struct AccountsInCookieJarInfo;
}  // namespace signin

namespace syncer {

// Provides interfaces to store/remove keys to/from file storage.
// This class performs expensive operations and expected to be run from
// dedicated sequence (using thread pool). Can be constructed on any thread/
// sequence.
class StandaloneTrustedVaultBackend
    : public base::RefCountedThreadSafe<StandaloneTrustedVaultBackend> {
 public:
  using FetchKeysCallback = base::OnceCallback<void(
      const std::vector<std::vector<uint8_t>>& vault_keys)>;

  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    virtual ~Delegate() = default;

    Delegate& operator=(const Delegate&) = delete;

    virtual void NotifyRecoverabilityDegradedChanged() = 0;
  };

  // |connection| can be null, in this case functionality that involves
  // interaction with vault service (such as device registration, keys
  // downloading, etc.) will be disabled.
  StandaloneTrustedVaultBackend(
      const base::FilePath& file_path,
      std::unique_ptr<Delegate> delegate,
      std::unique_ptr<TrustedVaultConnection> connection);
  StandaloneTrustedVaultBackend(const StandaloneTrustedVaultBackend& other) =
      delete;
  StandaloneTrustedVaultBackend& operator=(
      const StandaloneTrustedVaultBackend& other) = delete;

  // Restores state saved in |file_path_|, should be called before using the
  // object.
  void ReadDataFromDisk();

  // Populates vault keys corresponding to |account_info| into |callback|. If
  // recent keys are locally available, |callback| will be called immediately.
  // Otherwise, attempts to download new keys from the server. In case of
  // failure or if current state isn't sufficient it will populate locally
  // available keys regardless of their freshness.
  // Concurrent calls are not supported.
  void FetchKeys(const CoreAccountInfo& account_info,
                 FetchKeysCallback callback);

  // Replaces keys for given |gaia_id| both in memory and in |file_path_|.
  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int last_key_version);

  // Marks vault keys as stale.  Afterwards, the next FetchKeys() call for this
  // |account_info| will trigger a key download attempt.
  bool MarkLocalKeysAsStale(const CoreAccountInfo& account_info);

  // Sets/resets |primary_account_|.
  void SetPrimaryAccount(const absl::optional<CoreAccountInfo>& primary_account,
                         bool has_persistent_auth_error);

  // Handles changes of accounts in cookie jar and removes keys for some
  // accounts:
  // 1. Non-primary account keys are removed if account isn't in cookie jar.
  // 2. Primary account keys marked for deferred deletion if account isn't in
  // cookie jar.
  void UpdateAccountsInCookieJarInfo(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info);

  // Returns whether recoverability of the keys is degraded and user action is
  // required to add a new method.
  void GetIsRecoverabilityDegraded(const CoreAccountInfo& account_info,
                                   base::OnceCallback<void(bool)> cb);

  // Registers a new trusted recovery method that can be used to retrieve keys.
  void AddTrustedRecoveryMethod(const std::string& gaia_id,
                                const std::vector<uint8_t>& public_key,
                                int method_type_hint,
                                base::OnceClosure cb);

  void ClearDataForAccount(const CoreAccountInfo& account_info);

  absl::optional<CoreAccountInfo> GetPrimaryAccountForTesting() const;

  sync_pb::LocalDeviceRegistrationInfo GetDeviceRegistrationInfoForTesting(
      const std::string& gaia_id);

  std::vector<uint8_t> GetLastAddedRecoveryMethodPublicKeyForTesting() const;

  void SetDeviceRegisteredVersionForTesting(const std::string& gaia_id,
                                            int version);

  void SetClockForTesting(base::Clock* clock);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Exposed publicly for testing.
  enum class TrustedVaultDownloadKeysStatusForUMA {
    kSuccess = 0,
    // Deprecated in favor of the more fine-grained buckets.
    kDeprecatedMembershipNotFoundOrCorrupted = 1,
    kNoNewKeys = 2,
    kKeyProofsVerificationFailed = 3,
    kAccessTokenFetchingFailure = 4,
    kOtherError = 5,
    kMemberNotFound = 6,
    kMembershipNotFound = 7,
    kMembershipCorrupted = 8,
    kMembershipEmpty = 9,
    kNoPrimaryAccount = 10,
    kDeviceNotRegistered = 11,
    kThrottledClientSide = 12,
    kCorruptedLocalDeviceRegistration = 13,
    kAborted = 14,
    kMaxValue = kAborted
  };

 private:
  friend class base::RefCountedThreadSafe<StandaloneTrustedVaultBackend>;

  static TrustedVaultDownloadKeysStatusForUMA
  GetDownloadKeysStatusForUMAFromResponse(
      TrustedVaultDownloadKeysStatus response_status);

  ~StandaloneTrustedVaultBackend();

  // Finds the per-user vault in |data_| for |gaia_id|. Returns null if not
  // found.
  sync_pb::LocalTrustedVaultPerUser* FindUserVault(const std::string& gaia_id);

  // Attempts to register device in case it's not yet registered and currently
  // available local data is sufficient to do it. For the cases where
  // registration is desirable (i.e. feature toggle enabled and user signed in),
  // it returns an enum representing the registration state, intended to be used
  // for metric recording. Otherwise it returns nullopt.
  absl::optional<TrustedVaultDeviceRegistrationStateForUMA> MaybeRegisterDevice(
      bool has_persistent_auth_error_for_uma);

  // Called when device registration for |gaia_id| is completed (either
  // successfully or not). |data_| must contain LocalTrustedVaultPerUser for
  // given |gaia_id|.
  void OnDeviceRegistered(TrustedVaultRegistrationStatus status);
  void OnDeviceRegisteredWithoutKeys(
      TrustedVaultRegistrationStatus status,
      const TrustedVaultKeyAndVersion& vault_key_and_version);

  void OnKeysDownloaded(TrustedVaultDownloadKeysStatus status,
                        const std::vector<std::vector<uint8_t>>& new_vault_keys,
                        int last_vault_key_version);

  void OnTrustedRecoveryMethodAdded(base::OnceClosure cb,
                                    TrustedVaultRegistrationStatus status);

  void AbandonConnectionRequest();

  void FulfillOngoingFetchKeys(
      absl::optional<TrustedVaultDownloadKeysStatusForUMA> status_for_uma);

  // Returns true if the last failed request time imply that upcoming requests
  // should be throttled now (certain amount of time should pass since the last
  // failed request). Handles the situation, when last failed request time is
  // set to the future.
  bool AreConnectionRequestsThrottled();

  // Records request failure time, that will be used to determine whether new
  // requests should be throttled.
  void RecordFailedConnectionRequestForThrottling();

  // Removes all data for non-primary accounts if they were previously marked
  // for deletion due to accounts in cookie jar changes.
  void RemoveNonPrimaryAccountKeysIfMarkedForDeletion();

  void VerifyDeviceRegistrationForUMA(const std::string& gaia_id);

  const base::FilePath file_path_;

  const std::unique_ptr<Delegate> delegate_;

  // Used for communication with trusted vault server. Can be null, in this case
  // functionality that involves interaction with vault service (such as device
  // registration, keys downloading, etc.) will be disabled.
  // TODO(crbug.com/1113598): clean up logic around nullable |connection_|, once
  // kFollowTrustedVaultKeyRotation feature flag is removed.
  const std::unique_ptr<TrustedVaultConnection> connection_;

  sync_pb::LocalTrustedVault data_;

  // Only current |primary_account_| can be used for communication with trusted
  // vault server.
  absl::optional<CoreAccountInfo> primary_account_;

  // If AddTrustedRecoveryMethod() gets invoked before SetPrimaryAccount(), the
  // execution gets deferred until SetPrimaryAccount() is invoked.
  struct PendingTrustedRecoveryMethod {
    PendingTrustedRecoveryMethod();
    PendingTrustedRecoveryMethod(PendingTrustedRecoveryMethod&) = delete;
    PendingTrustedRecoveryMethod& operator=(PendingTrustedRecoveryMethod&) =
        delete;
    PendingTrustedRecoveryMethod(PendingTrustedRecoveryMethod&&);
    PendingTrustedRecoveryMethod& operator=(PendingTrustedRecoveryMethod&&);
    ~PendingTrustedRecoveryMethod();

    std::string gaia_id;
    std::vector<uint8_t> public_key;
    int method_type_hint;
    base::OnceClosure completion_callback;
  };
  absl::optional<PendingTrustedRecoveryMethod> pending_trusted_recovery_method_;

  // Used to plumb FetchKeys() result to the caller.
  FetchKeysCallback ongoing_fetch_keys_callback_;

  // Account used in last FetchKeys() call.
  absl::optional<std::string> ongoing_fetch_keys_gaia_id_;

  // Destroying this will cancel the ongoing request.
  std::unique_ptr<TrustedVaultConnection::Request> ongoing_connection_request_;
  std::unique_ptr<TrustedVaultConnection::Request>
      ongoing_verify_registration_request_;

  // Same as above, but specifically used for recoverability-related requests.
  // TODO(crbug.com/1201659): Move elsewhere.
  std::unique_ptr<TrustedVaultConnection::Request>
      ongoing_get_recoverability_request_;
  std::unique_ptr<TrustedVaultConnection::Request>
      ongoing_add_recovery_method_request_;

  // Used to determine current time, set to base::DefaultClock in prod and can
  // be overridden in tests.
  raw_ptr<base::Clock> clock_;

  std::vector<uint8_t> last_added_recovery_method_public_key_for_testing_;

  bool device_registration_state_recorded_to_uma_ = false;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_BACKEND_H_
