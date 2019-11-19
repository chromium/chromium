// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_METADATA_SYNCER_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_METADATA_SYNCER_IMPL_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/services/device_sync/cryptauth_key.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/cryptauth_metadata_syncer.h"
#include "chromeos/services/device_sync/network_request_error.h"
#include "chromeos/services/device_sync/proto/cryptauth_better_together_device_metadata.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_directive.pb.h"

namespace chromeos {

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
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthMetadataSyncer> BuildInstance(
        CryptAuthClientFactory* client_factory,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());

   private:
    static Factory* test_factory_;
  };

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

  friend std::ostream& operator<<(std::ostream& stream, const State& state);

  static base::Optional<base::TimeDelta> GetTimeoutForState(State state);
  static base::Optional<CryptAuthDeviceSyncResult::ResultCode>
  ResultCodeErrorFromTimeoutDuringState(State state);

  // CryptAuthMetadataSyncer:
  void OnAttemptStarted(
      const cryptauthv2::RequestContext& request_context,
      const cryptauthv2::BetterTogetherDeviceMetadata& local_device_metadata,
      const CryptAuthKey* initial_group_key) override;

  CryptAuthMetadataSyncerImpl(CryptAuthClientFactory* client_factory,
                              std::unique_ptr<base::OneShotTimer> timer);

  void SetState(State state);
  void OnTimeout();

  const CryptAuthKey* GetGroupKey();
  GroupPublicKeyState GetGroupPublicKeyState();

  void AttemptNextStep();

  void EncryptLocalDeviceMetadata();
  void OnLocalDeviceMetadataEncrypted(
      const base::Optional<std::string>& encrypted_metadata);
  void CreateGroupKey();
  void OnGroupKeyCreated(
      const base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKey>& new_keys,
      const base::Optional<CryptAuthKey>& client_ephemeral_dh);
  void MakeSyncMetadataCall();
  void OnSyncMetadataSuccess(const cryptauthv2::SyncMetadataResponse& response);
  void OnSyncMetadataFailure(NetworkRequestError error);
  void FilterMetadataAndFinishAttempt();

  void FinishAttempt(CryptAuthDeviceSyncResult::ResultCode result_code);

  size_t num_sync_metadata_calls_ = 0;
  cryptauthv2::RequestContext request_context_;
  cryptauthv2::BetterTogetherDeviceMetadata local_device_metadata_;
  base::Optional<std::string> encrypted_local_device_metadata_;

  base::Optional<cryptauthv2::SyncMetadataResponse> sync_metadata_response_;

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
  const CryptAuthKey* initial_group_key_;
  CryptAuthClientFactory* client_factory_ = nullptr;
  std::unique_ptr<base::OneShotTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthMetadataSyncerImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_METADATA_SYNCER_IMPL_H_
