// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_SYNC_DEVICE_INFO_OBSERVER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_SYNC_DEVICE_INFO_OBSERVER_H_

#include "base/containers/circular_deque.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace segmentation_platform::processing {

BASE_DECLARE_FEATURE(kSegmentationDeviceCountByOsType);

class SyncDeviceInfoObserver : public syncer::DeviceInfoTracker::Observer,
                               public InputDelegate {
 public:
  // |device_info_tracker| must not be null and must outlive this object.
  explicit SyncDeviceInfoObserver(
      syncer::DeviceInfoTracker* device_info_tracker);
  ~SyncDeviceInfoObserver() override;

  // DeviceInfoTracker::Observer overrides.
  void OnDeviceInfoChange() override;

  // InputDelegate impl
  void Process(const proto::CustomInput& input,
               FeatureProcessorState& feature_processor_state,
               ProcessedCallback callback) override;

 private:
  // Describes device info status for processing.
  enum DeviceInfoStatus {
    TIMEOUT_NOT_POSTED,
    TIMEOUT_POSTED_BUT_NOT_HIT,
    INFO_UNAVAILABLE,
    INFO_AVAILABLE,
  };

  // Returns the count of active devices per os type. Each device is identified
  // by one unique guid. No deduping is applied.
  std::map<syncer::DeviceInfo::OsType, int> CountActiveDevicesByOsType(
      base::TimeDelta active_threshold) const;

  // Called when ready to finish processing.
  void ReadyToFinishProcessing(const proto::CustomInput& input,
                               scoped_refptr<InputContext> input_context,
                               ProcessedCallback callback,
                               bool success);

  void OnTimeout();

  // Device info tracker. Not owned. It is managed by
  // the DeviceInfoSynceService, which is guaranteed to outlive the
  // SegmentationPlatformService, who owns this observer and depends on the
  // DeviceInfoSyncService.
  const raw_ptr<syncer::DeviceInfoTracker> device_info_tracker_;

  // Queue to put the pending actions request.
  base::circular_deque<base::OnceCallback<void(bool)>> pending_actions_;

  // Describes device info status for processing.
  DeviceInfoStatus device_info_status_{DeviceInfoStatus::TIMEOUT_NOT_POSTED};

  base::WeakPtrFactory<SyncDeviceInfoObserver> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_SYNC_DEVICE_INFO_OBSERVER_H_