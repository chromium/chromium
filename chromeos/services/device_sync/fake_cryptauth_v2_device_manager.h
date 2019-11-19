// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_V2_DEVICE_MANAGER_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_V2_DEVICE_MANAGER_H_

#include <string>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/services/device_sync/cryptauth_device.h"
#include "chromeos/services/device_sync/cryptauth_device_registry.h"
#include "chromeos/services/device_sync/cryptauth_device_sync_result.h"
#include "chromeos/services/device_sync/cryptauth_v2_device_manager.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"

namespace chromeos {

namespace device_sync {

// An implementation of CryptAuthV2DeviceManager used for tests. This
// implementation queues DeviceSync requests made via ForceDeviceSyncNow().
// These requests are sequentially processed by calls to
// FinishNextForcedDeviceSync(), which also updates parameters such as the last
// DeviceSync time.
class FakeCryptAuthV2DeviceManager : public CryptAuthV2DeviceManager {
 public:
  FakeCryptAuthV2DeviceManager();
  ~FakeCryptAuthV2DeviceManager() override;

  // CryptAuthV2DeviceManager:
  void Start() override;
  const CryptAuthDeviceRegistry::InstanceIdToDeviceMap& GetSyncedDevices()
      const override;
  void ForceDeviceSyncNow(
      const cryptauthv2::ClientMetadata::InvocationReason& invocation_reason,
      const base::Optional<std::string>& session_id) override;
  bool IsDeviceSyncInProgress() const override;
  bool IsRecoveringFromFailure() const override;
  base::Optional<base::Time> GetLastDeviceSyncTime() const override;
  base::Optional<base::TimeDelta> GetTimeToNextAttempt() const override;

  bool has_started() const { return has_started_; }

  const base::queue<cryptauthv2::ClientMetadata>&
  force_device_sync_now_requests() const {
    return force_device_sync_now_requests_;
  }

  void set_synced_devices(
      const CryptAuthDeviceRegistry::InstanceIdToDeviceMap& synced_devices) {
    synced_devices_ = synced_devices;
  }

  void set_time_to_next_attempt(
      const base::Optional<base::TimeDelta>& time_to_next_attempt) {
    time_to_next_attempt_ = time_to_next_attempt;
  }

  // Finishes the next forced DeviceSync request in the queue. Should only be
  // called if the queue of requests if not empty. If |device_sync_result|
  // indicates success, |device_sync_finish_time| will be stored as the last
  // DeviceSync time and will be returned by future calls to
  // GetLastDeviceSyncTime().
  void FinishNextForcedDeviceSync(
      const CryptAuthDeviceSyncResult& device_sync_result,
      base::Time device_sync_finish_time);

 private:
  bool has_started_ = false;
  bool is_recovering_from_failure_ = false;
  base::Optional<base::Time> last_device_sync_time_;
  base::Optional<base::TimeDelta> time_to_next_attempt_;
  CryptAuthDeviceRegistry::InstanceIdToDeviceMap synced_devices_;
  base::queue<cryptauthv2::ClientMetadata> force_device_sync_now_requests_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthV2DeviceManager);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_CRYPTAUTH_V2_DEVICE_MANAGER_H_
