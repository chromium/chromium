// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"

#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace optimization_guide {

using prefs::localstate::kOnDeviceModelChromeVersion;
using prefs::localstate::kOnDeviceModelCrashCount;
using prefs::localstate::kOnDeviceModelTimeoutCount;

OnDeviceModelAccessController::OnDeviceModelAccessController(
    PrefService& pref_service)
    : pref_service_(pref_service) {
  if (pref_service_->GetString(kOnDeviceModelChromeVersion) !=
      version_info::GetVersionNumber()) {
    // When the version changes, reset the counts so that we try again.
    pref_service_->SetInteger(kOnDeviceModelCrashCount, 0);
    pref_service_->SetInteger(kOnDeviceModelTimeoutCount, 0);
    pref_service_->SetString(kOnDeviceModelChromeVersion,
                             version_info::GetVersionNumber());
  }
}

OnDeviceModelAccessController::~OnDeviceModelAccessController() = default;

OnDeviceModelEligibilityReason
OnDeviceModelAccessController::ShouldStartNewSession() const {
  if (is_gpu_blocked_) {
    return OnDeviceModelEligibilityReason::kGpuBlocked;
  }
  if (pref_service_->GetInteger(kOnDeviceModelCrashCount) >=
      features::GetOnDeviceModelCrashCountBeforeDisable()) {
    return OnDeviceModelEligibilityReason::kTooManyRecentCrashes;
  }
  if (pref_service_->GetInteger(kOnDeviceModelTimeoutCount) >=
      features::GetOnDeviceModelTimeoutCountBeforeDisable()) {
    return OnDeviceModelEligibilityReason::kTooManyRecentTimeouts;
  }
  return OnDeviceModelEligibilityReason::kSuccess;
}

void OnDeviceModelAccessController::OnResponseCompleted() {
  pref_service_->SetInteger(kOnDeviceModelCrashCount, 0);
  pref_service_->SetInteger(kOnDeviceModelTimeoutCount, 0);
}

void OnDeviceModelAccessController::OnDisconnectedFromRemote() {
  pref_service_->SetInteger(
      kOnDeviceModelCrashCount,
      pref_service_->GetInteger(kOnDeviceModelCrashCount) + 1);
}

void OnDeviceModelAccessController::OnGpuBlocked() {
  is_gpu_blocked_ = true;
}

void OnDeviceModelAccessController::OnSessionTimedOut() {
  pref_service_->SetInteger(
      kOnDeviceModelTimeoutCount,
      pref_service_->GetInteger(kOnDeviceModelTimeoutCount) + 1);
}

}  // namespace optimization_guide
