// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/services/device_sync/feature_status_change.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace ash {

namespace multidevice {
class ExpiringRemoteDeviceCache;
}

namespace device_sync {

// Concrete implementation of DeviceSyncClient.
class DeviceSyncClientImpl : public DeviceSyncClient,
                             public mojom::DeviceSyncObserver {
 public:
  class Factory {
   public:
    static std::unique_ptr<DeviceSyncClient> Create();
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<DeviceSyncClient> CreateInstance() = 0;

   private:
    static Factory* test_factory_;
  };

  DeviceSyncClientImpl();

  DeviceSyncClientImpl(const DeviceSyncClientImpl&) = delete;
  DeviceSyncClientImpl& operator=(const DeviceSyncClientImpl&) = delete;

  ~DeviceSyncClientImpl() override;

  void Initialize(scoped_refptr<base::TaskRunner> task_runner) override;
  mojo::Remote<mojom::DeviceSync>* GetDeviceSyncRemote() override;

  // DeviceSyncClient:
  void ForceEnrollmentNow(
      mojom::DeviceSync::ForceEnrollmentNowCallback callback) override;
  void ForceSyncNow(mojom::DeviceSync::ForceSyncNowCallback callback) override;
  void GetBetterTogetherMetadataStatus(
      mojom::DeviceSync::GetBetterTogetherMetadataStatusCallback callback)
      override;
  void GetGroupPrivateKeyStatus(
      mojom::DeviceSync::GetGroupPrivateKeyStatusCallback callback) override;
  multidevice::RemoteDeviceRefList GetSyncedDevices() override;
  std::optional<multidevice::RemoteDeviceRef> GetLocalDeviceMetadata() override;
  void SetSoftwareFeatureState(
      const std::string public_key,
      multidevice::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      mojom::DeviceSync::SetSoftwareFeatureStateCallback callback) override;
  void SetFeatureStatus(
      const std::string& device_instance_id,
      multidevice::SoftwareFeature feature,
      FeatureStatusChange status_change,
      mojom::DeviceSync::SetFeatureStatusCallback callback) override;
  void FindEligibleDevices(multidevice::SoftwareFeature software_feature,
                           FindEligibleDevicesCallback callback) override;
  void NotifyDevices(
      const std::vector<std::string>& device_instance_ids,
      cryptauthv2::TargetService target_service,
      multidevice::SoftwareFeature feature,
      mojom::DeviceSync::NotifyDevicesCallback callback) override;
  void GetDevicesActivityStatus(
      mojom::DeviceSync::GetDevicesActivityStatusCallback callback) override;
  void GetDebugInfo(mojom::DeviceSync::GetDebugInfoCallback callback) override;

  // device_sync::mojom::DeviceSyncObserver:
  void OnEnrollmentFinished() override;
  void OnNewDevicesSynced() override;

 private:
  friend class DeviceSyncClientImplTest;

  void AttemptToBecomeReady();

  void LoadSyncedDevices();
  void LoadLocalDeviceMetadata();

  void OnGetSyncedDevicesCompleted(
      const std::optional<std::vector<multidevice::RemoteDevice>>&
          remote_devices);
  void OnGetLocalDeviceMetadataCompleted(
      const std::optional<multidevice::RemoteDevice>& local_device_metadata);
  void OnFindEligibleDevicesCompleted(
      FindEligibleDevicesCallback callback,
      mojom::NetworkRequestResult result_code,
      mojom::FindEligibleDevicesResponsePtr response);

  mojo::PendingRemote<mojom::DeviceSyncObserver> GenerateRemote();

  void FlushForTesting();

  mojo::Remote<mojom::DeviceSync> device_sync_;
  mojo::Receiver<mojom::DeviceSyncObserver> observer_receiver_{this};
  std::unique_ptr<multidevice::ExpiringRemoteDeviceCache>
      expiring_device_cache_;

  bool waiting_for_synced_devices_ = true;
  bool waiting_for_local_device_metadata_ = true;

  bool pending_notify_enrollment_finished_ = false;
  bool pending_notify_new_synced_devices_ = false;

  std::optional<std::string> local_instance_id_;

  // TODO(crbug.com/40105247): Track only the local Instance ID after v1
  // DeviceSync is disabled, when the local device is guaranteed to have an
  // Instance ID. Note: When v1 and v2 DeviceSync are running in parallel, if we
  // are still waiting for the first v2 DeviceSync to successfully complete, it
  // is possible that only v1 device data--which does not contain Instance
  // IDs--is loaded by the RemoteDeviceProvider. In that case, the local device
  // will not have an Instance ID until the very first v2 DeviceSync succeeds.
  std::optional<std::string> local_legacy_device_id_;

  base::WeakPtrFactory<DeviceSyncClientImpl> weak_ptr_factory_{this};
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_IMPL_H_
