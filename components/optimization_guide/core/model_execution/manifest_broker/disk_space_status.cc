// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/disk_space_status.h"

#include <cstdint>
#include <utility>

#include "base/byte_size.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"

namespace optimization_guide {

BASE_FEATURE(kOnDeviceModelBackgroundDownload,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceModelCachesDiskSpaceCheck,
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

// TTL for disk space evaluation result.
constexpr base::TimeDelta kDiskSpaceFreshnessThreshold = base::Seconds(10);

base::ByteSize GetDiskSpaceRequiredForBackgroundOnDeviceModelInstall() {
  return base::MiB(
      base::saturated_cast<uint64_t>(base::GetFieldTrialParamByFeatureAsInt(
          kOnDeviceModelBackgroundDownload,
          "on_device_model_free_space_mb_required_to_background_install",
          base::GiB(50).InMiB())));
}

}  // namespace

bool IsFreeDiskSpaceTooLowForOnDeviceModelCachesBuild(
    base::ByteSize free_disk_space_bytes) {
  if (!base::FeatureList::IsEnabled(kOnDeviceModelCachesDiskSpaceCheck)) {
    return false;
  }
  return base::MiB(base::saturated_cast<uint64_t>(
             base::GetFieldTrialParamByFeatureAsInt(
                 kOnDeviceModelCachesDiskSpaceCheck,
                 "free_space_mb_required_to_build_caches",
                 base::GiB(10).InMiB()))) >= free_disk_space_bytes;
}

bool IsFreeDiskSpaceSufficientForBackgroundOnDeviceModelInstall(
    base::ByteSize free_disk_space_bytes) {
  return GetDiskSpaceRequiredForBackgroundOnDeviceModelInstall() <=
         free_disk_space_bytes;
}

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
         IsFreeDiskSpaceSufficientForOnDeviceModelInstall(free_space_.value());
}

bool DiskSpaceStatus::IsSufficientForBackgroundInstall() const {
  return free_space_.has_value() &&
         IsFreeDiskSpaceSufficientForBackgroundOnDeviceModelInstall(
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
