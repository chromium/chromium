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
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
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
    o.OnDeviceEligibleUseCaseUsed(ToUseCaseName(feature), was_first_usage);
  }
}

void UsageTracker::OnDeviceEligibleUseCaseUsed(
    const std::string& use_case_name) {
  TRACE_EVENT("optimization_guide", "UsageTracker::OnDeviceEligibleUseCaseUsed",
              "use_case", use_case_name);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool was_first_usage = !WasUseCaseRecentlyUsed(use_case_name);
  model_execution::prefs::RecordUseCaseUsage(local_state_, use_case_name);

  for (auto& o : observers_) {
    o.OnDeviceEligibleUseCaseUsed(use_case_name, was_first_usage);
  }
}

bool UsageTracker::WasOnDeviceEligibleFeatureRecentlyUsed(
    mojom::OnDeviceFeature feature) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_execution::prefs::WasFeatureRecentlyUsed(&*local_state_,
                                                        feature);
}

bool UsageTracker::WasUseCaseRecentlyUsed(
    const std::string& use_case_name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_execution::prefs::WasUseCaseRecentlyUsed(&*local_state_,
                                                        use_case_name);
}

void UsageTracker::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void UsageTracker::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void UsageTracker::SetUseCaseRequested(const std::string& use_case_name,
                                       bool requested) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (requested) {
    OnDeviceEligibleUseCaseUsed(use_case_name);
  } else {
    model_execution::prefs::ClearUseCaseUsage(&*local_state_, use_case_name);
  }
}

}  // namespace optimization_guide
