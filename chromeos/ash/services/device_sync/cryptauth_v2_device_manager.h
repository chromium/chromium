// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_DEVICE_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_DEVICE_MANAGER_H_

#include <optional>
#include <string>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chromeos/ash/services/device_sync/cryptauth_device.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_registry.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/ash/services/device_sync/group_private_key_and_better_together_metadata_status.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"

namespace ash {

namespace device_sync {

// Manages syncing the user's devices that are registered with CryptAuth v2's
// "DeviceSync:BetterTogether" group.
class CryptAuthV2DeviceManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDeviceSyncStarted(
        const cryptauthv2::ClientMetadata& client_metadata) {}

    virtual void OnDeviceSyncFinished(
        const CryptAuthDeviceSyncResult& device_sync_result) {}
  };

  CryptAuthV2DeviceManager(const CryptAuthV2DeviceManager&) = delete;
  CryptAuthV2DeviceManager& operator=(const CryptAuthV2DeviceManager&) = delete;

  virtual ~CryptAuthV2DeviceManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts DeviceSync v2 scheduling. Should only be called once.
  virtual void Start() = 0;

  // Returns a map from Instance ID to CryptAuthDevice of synced devices. This
  // method makes no calls to CryptAuth, instead returning the local cache of
  // devices from the most recent DeviceSync.
  virtual const CryptAuthDeviceRegistry::InstanceIdToDeviceMap&
  GetSyncedDevices() const = 0;

  // Requests an immediate v2 DeviceSync.
  // |invocation_reason|: Specifies the reason that the DeviceSync was
  //                      triggered, which is uploaded to the server.
  // |session_id|: The session ID sent by CryptAuth v2 in a GCM message
  //               requesting a DeviceSync. Null if DeviceSync was not triggered
  //               by a GCM message or if no session ID was included in the GCM
  //               message.
  virtual void ForceDeviceSyncNow(
      const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
      const std::optional<std::string>& session_id) = 0;

  // Returns true if a v2 DeviceSync attempt is currently in progress.
  virtual bool IsDeviceSyncInProgress() const = 0;

  // Returns true if the last v2 DeviceSync attempt failed.
  virtual bool IsRecoveringFromFailure() const = 0;

  // Returns current better together metadata status of the owned device syncer
  // object. Clients can inspect GetDeviceSyncerGroupPrivateKeyStatus() if this
  // returns "waiting to process device metadata" or "group private key missing"
  // in order to understand why.
  virtual BetterTogetherMetadataStatus
  GetDeviceSyncerBetterTogetherMetadataStatus() const = 0;

  // Returns current group private key status of the owned device syncer object.
  virtual GroupPrivateKeyStatus GetDeviceSyncerGroupPrivateKeyStatus()
      const = 0;

  // Returns the time of the last successful v2 DeviceSync. Returns null if no
  // successful v2 DeviceSync has ever occurred.
  virtual std::optional<base::Time> GetLastDeviceSyncTime() const = 0;

  // Returns the time until the next scheduled v2 DeviceSync request. Returns
  // null if there is no request scheduled.
  virtual std::optional<base::TimeDelta> GetTimeToNextAttempt() const = 0;

 protected:
  CryptAuthV2DeviceManager();

  void NotifyDeviceSyncStarted(
      const cryptauthv2::ClientMetadata& client_metadata);
  void NotifyDeviceSyncFinished(
      const CryptAuthDeviceSyncResult& device_sync_result);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_V2_DEVICE_MANAGER_H_
