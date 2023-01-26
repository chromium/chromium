// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SYNC_DEVICE_INFO_OBSERVER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SYNC_DEVICE_INFO_OBSERVER_H_

#include "base/feature_list.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace segmentation_platform {

BASE_DECLARE_FEATURE(kSegmentationDeviceCountByOsType);

class SyncDeviceInfoObserver : public syncer::DeviceInfoTracker::Observer {
 public:
  SyncDeviceInfoObserver() = delete;
  // |device_info_tracker| must not be null and must outlive this object.
  explicit SyncDeviceInfoObserver(
      syncer::DeviceInfoTracker* device_info_tracker);
  ~SyncDeviceInfoObserver() override;

  // DeviceInfoTracker::Observer overrides.
  void OnDeviceInfoChange() override;

 private:
  // Returns the count of active devices per os type. Each device is identified
  // by one unique guid. No deduping is applied.
  std::map<syncer::DeviceInfo::OsType, int> CountActiveDevicesByOsType() const;

  // Device info tracker. Not owned. It is managed by
  // the DeviceInfoSynceService, which is guaranteed to outlive the
  // SegmentationPlatformService, who owns this observer and depends on the
  // DeviceInfoSyncService.
  const raw_ptr<syncer::DeviceInfoTracker> device_info_tracker_;
  // Flag indicating if device info has been recorded.
  bool device_info_recorded_ = false;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SYNC_DEVICE_INFO_OBSERVER_H_
