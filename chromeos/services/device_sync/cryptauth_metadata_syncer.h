// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_METADATA_SYNCER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_METADATA_SYNCER_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/services/device_sync/cryptauth_key.h"
#include "chromeos/services/device_sync/proto/cryptauth_devicesync.pb.h"
#include "chromeos/services/device_sync/proto/cryptauth_directive.pb.h"

namespace cryptauthv2 {
class BetterTogetherDeviceMetadata;
}  // namespace cryptauthv2

namespace chromeos {

namespace device_sync {

// Handles the SyncMetadata portion of the CryptAuth v2 DeviceSync protocol,
// which consists of one or two SyncMetadata request/response interactions with
// the CryptAuth servers:
//
// 1a) First SyncMetadataRequest: Contains what this device believes to be the
//     group public key. This could be a newly generated key or one from local
//     storage. The device's metadata is encrypted with this key and included in
//     the request.
//
// 1b) First SyncMetadataResponse: The response from CryptAuth possibly includes
//     the correct group key pair, where the private key is encrypted with this
//     device's CryptAuthKeyBunde::kDeviceSyncBetterTogether public key. If the
//     group public key is not set, the server is indicating that a new group
//     key pair must be created by this device. In this case or if the group
//     public key in the response differs from the one sent in the request,
//     another SyncMetadataRequest is made. Otherwise, the second
//     SyncMetadataRequest is skipped and the encrypted remote device metadata
//     is decrypted using the group private key if available. Note: The remote
//     devices are only those registered in the DeviceSync:BetterTogether group,
//     in other words, those using v2 DeviceSync.
//
// 2a) (Possible) Second SyncMetadataRequest: Not invoked if the group public
//     key from the first SyncMetadataResponse (1b) agrees with the one sent in
//     the first SyncMetadataRequest (1a). The client has the correct group
//     public key at this point.
//
// 2b) (Possible) Second SyncMetadataResponse: The included remote device
//     metadata can be decrypted if a group private key is sent. If no group
//     private key is returned, the client must wait for a GCM message
//     requesting a DeviceSync when the key becomes available.
//
// The output of a successful flow is:
// - A map from Instance ID to DeviceMetadataPacket, which contains device
//   metadata encrypted with the group public key.
// - (Optional) A new group public and private key created by the client or a
//   new group public key provided by CryptAuth. Null if no new group key was
//   created.
// - (Optional) A group private key, encrypted with this device's
//   CryptAuthKeyBunde::kDeviceSyncBetterTogether public key.
// - (Optional) A new ClientDirective sent from the CryptAuth server.
//
// A CryptAuthMetadataSyncer object is designed to be used for only one
// SyncMetadata() call. For a new DeviceSync attempt, a new object should be
// created.
class CryptAuthMetadataSyncer {
 public:
  using IdToDeviceMetadataPacketMap =
      base::flat_map<std::string, cryptauthv2::DeviceMetadataPacket>;
  using SyncMetadataAttemptFinishedCallback = base::OnceCallback<void(
      const IdToDeviceMetadataPacketMap&,
      std::unique_ptr<CryptAuthKey>,
      const base::Optional<cryptauthv2::EncryptedGroupPrivateKey>&,
      const base::Optional<cryptauthv2::ClientDirective>&,
      CryptAuthDeviceSyncResult::ResultCode)>;

  virtual ~CryptAuthMetadataSyncer();

  // Starts the SyncMetadata portion of the CryptAuth v2 DeviceSync flow.
  void SyncMetadata(
      const cryptauthv2::RequestContext& request_context,
      const cryptauthv2::BetterTogetherDeviceMetadata& local_device_metadata,
      const CryptAuthKey* initial_group_key,
      SyncMetadataAttemptFinishedCallback callback);

 protected:
  CryptAuthMetadataSyncer();

  virtual void OnAttemptStarted(
      const cryptauthv2::RequestContext& request_context,
      const cryptauthv2::BetterTogetherDeviceMetadata& local_device_metadata,
      const CryptAuthKey* initial_group_key) = 0;

  void OnAttemptFinished(
      const IdToDeviceMetadataPacketMap& id_to_device_metadata_packet_map,
      std::unique_ptr<CryptAuthKey> new_group_key,
      const base::Optional<cryptauthv2::EncryptedGroupPrivateKey>&
          encrypted_group_private_key,
      const base::Optional<cryptauthv2::ClientDirective>& new_client_directive,
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code);

 private:
  SyncMetadataAttemptFinishedCallback callback_;
  bool was_sync_metadata_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthMetadataSyncer);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_METADATA_SYNCER_H_
