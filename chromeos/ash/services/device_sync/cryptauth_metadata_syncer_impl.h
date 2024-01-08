// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_METADATA_SYNCER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_METADATA_SYNCER_IMPL_H_

#include <memory>
#include <optional>
#include <ostream>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/cryptauth_metadata_syncer.h"
#include "chromeos/ash/services/device_sync/network_request_error.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_better_together_device_metadata.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_directive.pb.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

namespace device_sync {

class CryptAuthClient;
class CryptAuthClientFactory;
class CryptAuthEciesEncryptor;
class CryptAuthKeyCreator;

// An implementation of CryptAuthMetadataSyncer, using instances of
// CryptAuthClient to make the SyncMetadata API calls to CryptAuth. Timeouts are
// handled internally, so ShareGroupPrivateKey() is always guaranteed to return.
//
// All returned DeviceMetadataPackets are guaranteed to have a nontrivial device
// ID, device name, and device public key.
class CryptAuthMetadataSyncerImpl : public CryptAuthMetadataSyncer {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthMetadataSyncer> Create(
        CryptAuthClientFactory* client_factory,
        PrefService* pref_service,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthMetadataSyncer> CreateInstance(
        CryptAuthClientFactory* client_factory,
        PrefService* pref_service,
        std::unique_ptr<base::OneShotTimer> timer) = 0;

   private:
    static Factory* test_factory_;
  };

  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  CryptAuthMetadataSyncerImpl(const CryptAuthMetadataSyncerImpl&) = delete;
  CryptAuthMetadataSyncerImpl& operator=(const CryptAuthMetadataSyncerImpl&) =
      delete;

  ~CryptAuthMetadataSyncerImpl() override;

 private:
  enum class State {
    kNotStarted,
    kWaitingForGroupKeyCreation,
    kWaitingForLocalDeviceMetadataEncryption,
    kWaitingForFirstSyncMetadataResponse,
    kWaitingForSecondSyncMetadataResponse,
    kFinished
  };
  friend std::ostream& operator<<(std::ostream& stream, const State& state);

  // kKeyExistsButNotConfirmedWithCryptAuth: A local group public key exists but
  //     CryptAuth has yet to confirm or deny that it is the correct group key.
  // kNewKeyNeedsToBeCreate: Either a local group public key does not exist or
  //     an empty group_public_key field in the first SyncMetadataResponse is a
  //     signal from CryptAuth that we should generate a new group key pair and
  //     make another SyncMetadataRequest.
  // kNewKeyReceivedFromCryptAuth: If our local group public key differs from
  //     the one received from CryptAuth, replace the local key and make another
  //     SyncMetadataRequest. The group private key will be provided in the next
  //     SyncMetadataResponse if available.
  // kEstablished: Our local group public key agrees with the group_public_key
  //     field sent in the latest SyncMetadataResponse. It should take at most
  //     two SyncMetadata calls to establish the group public key.
  enum class GroupPublicKeyState {
    kUndetermined,
    kKeyExistsButNotConfirmedWithCryptAuth,
    kNewKeyNeedsToBeCreated,
    kNewKeyReceivedFromCryptAuth,
    kEstablished
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const GroupPublicKeyState& state);

  static std::optional<base::TimeDelta> GetTimeoutForState(State state);
  static std::optional<CryptAuthDeviceSyncResult::ResultCode>
  ResultCodeErrorFromTimeoutDuringState(State state);

  // CryptAuthMetadataSyncer:
  void OnAttemptStarted(
      const cryptauthv2::RequestContext& request_context,
      const cryptauthv2::BetterTogetherDeviceMetadata& local_device_metadata,
      const CryptAuthKey* initial_group_key) override;

  CryptAuthMetadataSyncerImpl(CryptAuthClientFactory* client_factory,
                              PrefService* pref_service,
                              std::unique_ptr<base::OneShotTimer> timer);

  void SetState(State state);
  void OnTimeout();

  const CryptAuthKey* GetGroupKey();
  GroupPublicKeyState GetGroupPublicKeyState();

  void AttemptNextStep();

  // If the local device metadata and the encrypting group public key have not
  // changed since they were last cached, reuse the cached encrypted local
  // device metadata. Because the ECIES encryptor uses a different session key
  // for each encryption, the blob could change even if the underlying metadata
  // and group public key have not changed. We do not want the CryptAuth server
  // to act as though device metadata has changed if the underlying data and
  // encrypting key remain the same.
  bool ShouldUseCachedEncryptedLocalDeviceMetadata();

  void EncryptLocalDeviceMetadata();
  void OnLocalDeviceMetadataEncrypted(
      const std::optional<std::string>& encrypted_metadata);
  void CreateGroupKey();
  void OnGroupKeyCreated(
      const base::flat_map<CryptAuthKeyBundle::Name,
                           std::optional<CryptAuthKey>>& new_keys,
      const std::optional<CryptAuthKey>& client_ephemeral_dh);
  void MakeSyncMetadataCall();
  void OnSyncMetadataSuccess(const cryptauthv2::SyncMetadataResponse& response);
  void OnSyncMetadataFailure(NetworkRequestError error);
  void FilterMetadataAndFinishAttempt();

  void FinishAttempt(CryptAuthDeviceSyncResult::ResultCode result_code);

  size_t num_sync_metadata_calls_ = 0;
  cryptauthv2::RequestContext request_context_;
  cryptauthv2::BetterTogetherDeviceMetadata local_device_metadata_;
  std::optional<std::string> encrypted_local_device_metadata_;

  std::optional<cryptauthv2::SyncMetadataResponse> sync_metadata_response_;

  // The filtered map of DeviceMetadataPackets from the SyncMetadataResponse,
  // keyed by device ID. All DeviceMetadataPackets are guaranteed to have a
  // nontrivial device ID, device name, and device public key.
  base::flat_map<std::string, cryptauthv2::DeviceMetadataPacket>
      id_to_device_metadata_packet_map_;

  // Non-null if a new group key is created or if CryptAuth sends a new group
  // public key during the SyncMetadata flow. This value is returned in a
  // callback when the attempt finishes.
  std::unique_ptr<CryptAuthKey> new_group_key_;

  // The CryptAuthClient for the latest CryptAuth request. The client can only
  // be used for one call; therefore, for each API call, a new client needs to
  // be generated from |client_factory_|.
  std::unique_ptr<CryptAuthClient> cryptauth_client_;

  // Used to generate the group key pair if necessary.
  std::unique_ptr<CryptAuthKeyCreator> key_creator_;

  // The CryptAuthEciesEncryptor for the latest encryption/decryption. An
  // instance can only be used for one method call; therefore, for each
  // encryption/decryption, a new encryptor needs to be generated.
  std::unique_ptr<CryptAuthEciesEncryptor> encryptor_;

  // The time of the last state change. Used for execution time metrics.
  base::TimeTicks last_state_change_timestamp_;

  State state_ = State::kNotStarted;
  raw_ptr<const CryptAuthKey> initial_group_key_;
  raw_ptr<CryptAuthClientFactory> client_factory_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
  std::unique_ptr<base::OneShotTimer> timer_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_METADATA_SYNCER_IMPL_H_
