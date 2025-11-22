// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/usage_tracker.h"

#include "base/strings/to_string.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {

UsageTracker::UsageTracker(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}

UsageTracker::~UsageTracker() = default;

void UsageTracker::OnDeviceEligibleFeatureUsed(mojom::OnDeviceFeature feature) {
  TRACE_EVENT("optimization_guide", "UsageTracker::OnDeviceEligibleFeatureUsed",
              "feature", base::ToString(feature));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool was_first_usage = !WasOnDeviceEligibleFeatureRecentlyUsed(feature);
  model_execution::prefs::RecordFeatureUsage(local_state_, feature);

  for (auto& o : observers_) {
    o.OnDeviceEligibleFeatureUsed(feature);
  }
  if (was_first_usage) {
    for (auto& o : observers_) {
      o.OnDeviceEligibleFeatureFirstUsed(feature);
    }
  }
}

bool UsageTracker::WasOnDeviceEligibleFeatureRecentlyUsed(
    mojom::OnDeviceFeature feature) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_execution::prefs::WasFeatureRecentlyUsed(&*local_state_,
                                                        feature);
}

bool UsageTracker::WasAnyOnDeviceEligibleFeatureRecentlyUsed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::ranges::any_of(
      OnDeviceFeatureSet::All(), [&](mojom::OnDeviceFeature feature) {
        return WasOnDeviceEligibleFeatureRecentlyUsed(feature);
      });
}

void UsageTracker::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void UsageTracker::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

}  // namespace optimization_guide
