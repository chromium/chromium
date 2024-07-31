// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_DEVICE_SOURCE_SYNC_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_DEVICE_SOURCE_SYNC_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sharing_message/sharing_device_source.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/local_device_info_provider.h"

namespace syncer {
class SyncService;
class DeviceInfo;
class DeviceInfoTracker;
}  // namespace syncer

class SharingDeviceSourceSync : public SharingDeviceSource,
                                public syncer::DeviceInfoTracker::Observer {
 public:
  SharingDeviceSourceSync(
      syncer::SyncService* sync_service,
      syncer::LocalDeviceInfoProvider* local_device_info_provider,
      syncer::DeviceInfoTracker* device_info_tracker);

  SharingDeviceSourceSync(const SharingDeviceSourceSync&) = delete;
  SharingDeviceSourceSync& operator=(const SharingDeviceSourceSync&) = delete;

  ~SharingDeviceSourceSync() override;

  // SharingDeviceSource:
  bool IsReady() override;
  std::optional<SharingTargetDeviceInfo> GetDeviceByGuid(
      const std::string& guid) override;
  std::vector<SharingTargetDeviceInfo> GetDeviceCandidates(
      sync_pb::SharingSpecificFields::EnabledFeatures required_feature)
      override;

  // syncer::DeviceInfoTracker::Observer:
  void OnDeviceInfoChange() override;

  // Used to fake client names in integration tests.
  void SetDeviceInfoTrackerForTesting(syncer::DeviceInfoTracker* tracker);

 private:
  void InitPersonalizableLocalDeviceName(
      std::string personalizable_local_device_name);

  // Called by |local_device_info_provider_| when it is ready.
  void OnLocalDeviceInfoProviderReady();

  // Deduplicates devices based on their full name. For devices with duplicate
  // full names, only the most recently updated device is returned. All devices
  // are renamed to either their short name if that one is unique, or their full
  // name otherwise. The returned list is sorted in (not strictly) descending
  // order by last_updated_timestamp.
  std::vector<SharingTargetDeviceInfo> ConvertAndDeduplicateDevices(
      std::vector<const syncer::DeviceInfo*> devices) const;

  std::vector<const syncer::DeviceInfo*> FilterDeviceCandidates(
      std::vector<const syncer::DeviceInfo*> devices,
      sync_pb::SharingSpecificFields::EnabledFeatures required_feature) const;

  raw_ptr<syncer::SyncService> sync_service_;
  raw_ptr<syncer::LocalDeviceInfoProvider> local_device_info_provider_;
  raw_ptr<syncer::DeviceInfoTracker> device_info_tracker_;
  base::CallbackListSubscription local_device_info_ready_subscription_;

  // The personalized name is stored for deduplicating devices running older
  // clients.
  std::optional<std::string> personalizable_local_device_name_;

  base::WeakPtrFactory<SharingDeviceSourceSync> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_DEVICE_SOURCE_SYNC_H_
