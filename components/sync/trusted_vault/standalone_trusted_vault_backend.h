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
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/protocol/local_trusted_vault.pb.h"
#include "components/sync/trusted_vault/trusted_vault_connection.h"

namespace base {
class Clock;
}  // namespace base

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
  bool MarkKeysAsStale(const CoreAccountInfo& account_info);

  // Removes all keys for all accounts from both memory and |file_path_|.
  void RemoveAllStoredKeys();

  // Sets/resets |primary_account_|.
  void SetPrimaryAccount(
      const base::Optional<CoreAccountInfo>& primary_account);

  // Returns whether recoverability of the keys is degraded and user action is
  // required to add a new method.
  void GetIsRecoverabilityDegraded(const CoreAccountInfo& account_info,
                                   base::OnceCallback<void(bool)> cb);

  // Registers a new trusted recovery method that can be used to retrieve keys.
  void AddTrustedRecoveryMethod(const std::string& gaia_id,
                                const std::vector<uint8_t>& public_key,
                                base::OnceClosure cb);

  base::Optional<CoreAccountInfo> GetPrimaryAccountForTesting() const;

  sync_pb::LocalDeviceRegistrationInfo GetDeviceRegistrationInfoForTesting(
      const std::string& gaia_id);

  void SetRecoverabilityDegradedForTesting();

  void SetClockForTesting(base::Clock* clock);

 private:
  friend class base::RefCountedThreadSafe<StandaloneTrustedVaultBackend>;

  ~StandaloneTrustedVaultBackend();

  // Finds the per-user vault in |data_| for |gaia_id|. Returns null if not
  // found.
  sync_pb::LocalTrustedVaultPerUser* FindUserVault(const std::string& gaia_id);

  // Attempts to register device in case it's not yet registered and currently
  // available local data is sufficient to do it.
  void MaybeRegisterDevice(const std::string& gaia_id);

  // Called when device registration for |gaia_id| is completed (either
  // successfully or not).
  void OnDeviceRegistered(const std::string& gaia_id,
                          TrustedVaultRequestStatus status);

  void OnKeysDownloaded(const std::string& gaia_id,
                        TrustedVaultRequestStatus status,
                        const std::vector<std::vector<uint8_t>>& vault_keys,
                        int last_vault_key_version);

  void AbandonConnectionRequest();

  void FulfillOngoingFetchKeys();

  // Returns true if the last failed request time imply that upcoming requests
  // should be throttled now (certain amount of time should pass since the last
  // failed request). Handles the situation, when last failed request time is
  // set to the future.
  bool AreConnectionRequestsThrottled(const std::string& gaia_id);

  // Records request failure time, that will be used to determine whether new
  // requests should be throttled.
  void RecordFailedConnectionRequestForThrottling(const std::string& gaia_id);

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
  base::Optional<CoreAccountInfo> primary_account_;

  // Used to plumb FetchKeys() result to the caller.
  FetchKeysCallback ongoing_fetch_keys_callback_;

  // Account used in last FetchKeys() call.
  base::Optional<std::string> ongoing_fetch_keys_gaia_id_;

  // Destroying this will cancel the ongoing request.
  std::unique_ptr<TrustedVaultConnection::Request> ongoing_connection_request_;

  // Used to determine current time, set to base::DefaultClock in prod and can
  // be overridden in tests.
  base::Clock* clock_;

  bool is_recoverability_degraded_for_testing_ = false;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_BACKEND_H_
