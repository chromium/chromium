// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_DISK_SPACE_STATUS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_DISK_SPACE_STATUS_H_

#include <optional>
#include <vector>

#include "base/byte_size.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager_delegate.h"

namespace optimization_guide {

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
