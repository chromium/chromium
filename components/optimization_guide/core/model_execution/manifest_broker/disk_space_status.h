// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_DISK_SPACE_STATUS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_DISK_SPACE_STATUS_H_

#include <optional>
#include <vector>

#include "base/byte_size.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager_delegate.h"

namespace optimization_guide {

// Whether on-device models are downloaded in background prior to feature usage.
BASE_DECLARE_FEATURE(kOnDeviceModelBackgroundDownload);

// Whether on-device model session creation is gated on sufficient disk space to
// build execution caches.
BASE_DECLARE_FEATURE(kOnDeviceModelCachesDiskSpaceCheck);

// Return the disk space required for on-device model install.
inline constexpr base::ByteSize GetDiskSpaceRequiredForOnDeviceModelInstall() {
  return base::GiB(20);
}

// Return the disk space required to retain the on-device model.
inline constexpr base::ByteSize GetDiskSpaceRequiredForOnDeviceModelRetain() {
  return base::GiB(5);
}

// Whether there is enough free disk space to allow on-device model
// installation.
inline constexpr bool IsFreeDiskSpaceSufficientForOnDeviceModelInstall(
    base::ByteSize free_disk_space_bytes) {
  return GetDiskSpaceRequiredForOnDeviceModelInstall() <= free_disk_space_bytes;
}

// Whether there is too little disk space to retain the on-device model
// installation.
inline constexpr bool IsFreeDiskSpaceTooLowForOnDeviceModelInstall(
    base::ByteSize free_disk_space_bytes) {
  return GetDiskSpaceRequiredForOnDeviceModelRetain() >= free_disk_space_bytes;
}

// Whether there is too little disk space to build caches for the
// on-device model installed.
bool IsFreeDiskSpaceTooLowForOnDeviceModelCachesBuild(
    base::ByteSize free_disk_space_bytes);

// Whether there is enough free disk space to allow on-device model
// installation proactively in background.
bool IsFreeDiskSpaceSufficientForBackgroundOnDeviceModelInstall(
    base::ByteSize free_disk_space_bytes);

// Tracks the available free disk space, its freshness TTL, and pending
// evaluation callbacks.
class DiskSpaceStatus {
 public:
  explicit DiskSpaceStatus(const ManifestMonitorDelegate& delegate);
  ~DiskSpaceStatus();

  DiskSpaceStatus(const DiskSpaceStatus&) = delete;
  DiskSpaceStatus& operator=(const DiskSpaceStatus&) = delete;

  // Ensures the disk space status is fresh. If already fresh, invokes
  // `on_complete` (if non-null) immediately. Otherwise, starts an evaluation
  // via `delegate_` (if one is not already pending) and queues `on_complete` to
  // run once the evaluation completes.
  void EnsureEvaluated(base::OnceClosure on_complete);

  bool IsFresh() const;
  bool IsEvaluationPending() const { return evaluation_pending_; }
  bool IsSufficientForOnDemandInstall() const;
  bool IsSufficientForBackgroundInstall() const;
  std::optional<base::ByteSize> free_space() const { return free_space_; }

 private:
  void OnDiskSpaceEvaluated(std::optional<base::ByteSize> free_space);

  const raw_ref<const ManifestMonitorDelegate> delegate_;
  std::optional<base::ByteSize> free_space_;
  base::Time last_evaluated_;
  bool evaluation_pending_ = false;
  std::vector<base::OnceClosure> callbacks_;
  base::WeakPtrFactory<DiskSpaceStatus> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_DISK_SPACE_STATUS_H_
