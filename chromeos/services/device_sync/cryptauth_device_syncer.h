// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_SYNCER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_SYNCER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"

namespace cryptauthv2 {
class ClientMetadata;
class ClientAppMetadata;
}  // namespace cryptauthv2

namespace chromeos {

namespace device_sync {

// Implements the client end of the CryptAuth v2 DeviceSync protocol, which
// consists of three to four request/response interactions with the CryptAuth
// servers:
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
//     requesting another DeviceSync when the key becomes available.
//
// 3)  BatchGetFeatureStatusesRequest/Response: Gets the supported and enabled
//     states of all multidevice (BetterTogether) features for all devices.
//
// 4a) ShareGroupPrivateKeyRequest: Sends CryptAuth this device's group private
//     key encrypted with each remote device's
//     CryptAuthKeyBundle::kDeviceSyncBetterTogether public key. This ensures
//     end-to-end encryption of the group private key and consequently the
//     device metadata.
//
// 4b) ShareGroupPrivateKeyResponse: This response is only an indication that
//     the ShareGroupPrivateKeyRequest was successful.
//
// A CryptAuthDeviceSyncer object is designed to be used for only one Sync()
// call. For a new DeviceSync attempt, a new object should be created.
class CryptAuthDeviceSyncer {
 public:
  virtual ~CryptAuthDeviceSyncer();

  // The DeviceSync result is passed by value so that the device syner can be
  // deleted before the result is processed.
  using DeviceSyncAttemptFinishedCallback =
      base::OnceCallback<void(CryptAuthDeviceSyncResult)>;

  // Starts the CryptAuth v2 DeviceSync flow.
  // |client_metadata|: Information about the DeviceSync attempt--such as
  //     invocation reason, retry count, etc.--that is sent to CryptAuth in
  //     each request.
  // |client_app_metadata|: Information about the local device such as the
  //     Instance ID and hardware information.
  // |callback|: Invoked when the DeviceSync attempt concludes, successfully or
  //     not. The CryptAuthDeviceSyncResult provides information about the
  //     outcome of the DeviceSync attempt and possibly a new ClientDirective.
  void Sync(const cryptauthv2::ClientMetadata& client_metadata,
            const cryptauthv2::ClientAppMetadata& client_app_metadata,
            DeviceSyncAttemptFinishedCallback callback);

 protected:
  CryptAuthDeviceSyncer();

  virtual void OnAttemptStarted(
      const cryptauthv2::ClientMetadata& client_metadata,
      const cryptauthv2::ClientAppMetadata& client_app_metadata) = 0;

  void OnAttemptFinished(const CryptAuthDeviceSyncResult& device_sync_result);

 private:
  DeviceSyncAttemptFinishedCallback callback_;
  bool was_sync_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthDeviceSyncer);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_SYNCER_H_
