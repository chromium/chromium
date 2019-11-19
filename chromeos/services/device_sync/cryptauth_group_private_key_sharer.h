// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_GROUP_PRIVATE_KEY_SHARER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_GROUP_PRIVATE_KEY_SHARER_H_

#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"

namespace cryptauthv2 {
class RequestContext;
}  // namespace cryptauthv2

namespace chromeos {

namespace device_sync {

class CryptAuthKey;

// Handles the ShareGroupPrivateKey portion of the CryptAuth v2 DeviceSync
// protocol. Sends the group private key--encrypted with each requesting
// device's public key--to CryptAuth for distribution to the requesting devices
// during their next DeviceSync calls.
//
// A CryptAuthGroupPrivateKeySharer object is designed to be used for only one
// ShareGroupPrivateKey() call. For a new attempt, a new object should be
// created.
class CryptAuthGroupPrivateKeySharer {
 public:
  using IdToEncryptingKeyMap = base::flat_map<std::string, std::string>;
  using ShareGroupPrivateKeyAttemptFinishedCallback =
      base::OnceCallback<void(CryptAuthDeviceSyncResult::ResultCode)>;

  virtual ~CryptAuthGroupPrivateKeySharer();

  // Starts the ShareGroupPrivateKey portion of the CryptAuth v2 DeviceSync
  // flow. Sends |group_key|'s private key to CryptAuth, encrypted for each
  // device with ID in |id_to_encrypting_key| using the corresponding encrypting
  // key. |group_key|'s private key and |id_to_encrypting_key_map| cannot be
  // empty.
  void ShareGroupPrivateKey(
      const cryptauthv2::RequestContext& request_context,
      const CryptAuthKey& group_key,
      const IdToEncryptingKeyMap& id_to_encrypting_key_map,
      ShareGroupPrivateKeyAttemptFinishedCallback callback);

 protected:
  CryptAuthGroupPrivateKeySharer();

  virtual void OnAttemptStarted(
      const cryptauthv2::RequestContext& request_context,
      const CryptAuthKey& group_key,
      const IdToEncryptingKeyMap& id_to_encrypting_key_map) = 0;

  void OnAttemptFinished(
      CryptAuthDeviceSyncResult::ResultCode device_sync_result_code);

 private:
  ShareGroupPrivateKeyAttemptFinishedCallback callback_;
  bool was_share_group_private_key_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthGroupPrivateKeySharer);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  //  CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_GROUP_PRIVATE_KEY_SHARER_H_
