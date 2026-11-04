// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/disk_space_status.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

namespace optimization_guide {

namespace {

// TTL for disk space evaluation result.
constexpr base::TimeDelta kDiskSpaceFreshnessThreshold = base::Seconds(10);

}  // namespace

DiskSpaceStatus::DiskSpaceStatus(const ManifestMonitorDelegate& delegate)
    : delegate_(delegate) {}

DiskSpaceStatus::~DiskSpaceStatus() = default;

void DiskSpaceStatus::EnsureEvaluated(base::OnceClosure on_complete) {
  if (IsFresh()) {
    if (on_complete) {
      std::move(on_complete).Run();
    }
    return;
  }
  if (on_complete) {
    callbacks_.push_back(std::move(on_complete));
  }
  if (!evaluation_pending_) {
    evaluation_pending_ = true;
    delegate_->GetFreeDiskSpace(
        base::BindOnce(&DiskSpaceStatus::OnDiskSpaceEvaluated,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

bool DiskSpaceStatus::IsFresh() const {
  return free_space_.has_value() &&
         (base::Time::Now() - last_evaluated_) < kDiskSpaceFreshnessThreshold;
}

bool DiskSpaceStatus::IsSufficientForOnDemandInstall() const {
  return free_space_.has_value() &&
         features::IsFreeDiskSpaceSufficientForOnDeviceModelInstall(
             free_space_.value());
}

bool DiskSpaceStatus::IsSufficientForBackgroundInstall() const {
  return free_space_.has_value() &&
         features::IsFreeDiskSpaceSufficientForBackgroundOnDeviceModelInstall(
             free_space_.value());
}

void DiskSpaceStatus::OnDiskSpaceEvaluated(
    std::optional<base::ByteSize> free_space) {
  evaluation_pending_ = false;
  free_space_ = free_space;
  last_evaluated_ = base::Time::Now();
  for (auto& callback : std::exchange(callbacks_, {})) {
    std::move(callback).Run();
  }
}

}  // namespace optimization_guide
