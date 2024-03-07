// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/services/device_sync/feature_status_change.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace ash {

namespace device_sync {

// Provides clients access to the DeviceSync API.
class DeviceSyncClient {
 public:
  class Observer {
   public:
    // Called when the DeviceSyncClient is ready, i.e. local device metadata
    // and synced devices are available.
    virtual void OnReady() {}

    // OnEnrollmentFinished() and OnNewDevicesSynced() will only be called once
    // DeviceSyncClient is ready, i.e., OnReady() will always be the first
    // callback called.
    virtual void OnEnrollmentFinished() {}
    virtual void OnNewDevicesSynced() {}

   protected:
    virtual ~Observer() = default;
  };

  using FindEligibleDevicesCallback =
      base::OnceCallback<void(mojom::NetworkRequestResult,
                              multidevice::RemoteDeviceRefList,
                              multidevice::RemoteDeviceRefList)>;

  DeviceSyncClient();

  DeviceSyncClient(const DeviceSyncClient&) = delete;
  DeviceSyncClient& operator=(const DeviceSyncClient&) = delete;

  virtual ~DeviceSyncClient();

  // Completes initialization. Must be called after connecting the DeviceSync
  // mojo remote to the implementation.
  virtual void Initialize(scoped_refptr<base::TaskRunner> task_runner) {}

  // Returns the DeviceSync mojo remote.
  virtual mojo::Remote<mojom::DeviceSync>* GetDeviceSyncRemote();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Clients of DeviceSyncClient should ensure that this returns true before
  // calling any methods. If false, clients should listen on and wait for the
  // OnReady() callback.
  bool is_ready() { return is_ready_; }

  virtual void ForceEnrollmentNow(
      mojom::DeviceSync::ForceEnrollmentNowCallback callback) = 0;
  virtual void ForceSyncNow(
      mojom::DeviceSync::ForceSyncNowCallback callback) = 0;
  virtual void GetBetterTogetherMetadataStatus(
      mojom::DeviceSync::GetBetterTogetherMetadataStatusCallback callback) = 0;
  virtual void GetGroupPrivateKeyStatus(
      mojom::DeviceSync::GetGroupPrivateKeyStatusCallback callback) = 0;
  virtual multidevice::RemoteDeviceRefList GetSyncedDevices() = 0;
  virtual std::optional<multidevice::RemoteDeviceRef>
  GetLocalDeviceMetadata() = 0;

  // Note: In the special case of passing |software_feature| =
  // SoftwareFeature::EASY_UNLOCK_HOST and |enabled| = false, |public_key| is
  // ignored.
  virtual void SetSoftwareFeatureState(
      const std::string public_key,
      multidevice::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      mojom::DeviceSync::SetSoftwareFeatureStateCallback callback) = 0;
  virtual void SetFeatureStatus(
      const std::string& device_instance_id,
      multidevice::SoftwareFeature feature,
      FeatureStatusChange status_change,
      mojom::DeviceSync::SetFeatureStatusCallback callback) = 0;
  virtual void FindEligibleDevices(
      multidevice::SoftwareFeature software_feature,
      FindEligibleDevicesCallback callback) = 0;
  virtual void NotifyDevices(
      const std::vector<std::string>& device_instance_ids,
      cryptauthv2::TargetService target_service,
      multidevice::SoftwareFeature feature,
      mojom::DeviceSync::NotifyDevicesCallback callback) = 0;
  virtual void GetDevicesActivityStatus(
      mojom::DeviceSync::GetDevicesActivityStatusCallback callback) = 0;
  virtual void GetDebugInfo(
      mojom::DeviceSync::GetDebugInfoCallback callback) = 0;

 protected:
  void NotifyReady();
  void NotifyEnrollmentFinished();
  void NotifyNewDevicesSynced();

 private:
  bool is_ready_ = false;
  base::ObserverList<Observer>::UncheckedAndDanglingUntriaged observer_list_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_H_
