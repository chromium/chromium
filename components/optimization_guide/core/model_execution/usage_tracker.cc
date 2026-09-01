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

void UsageTracker::RaisePriority(const std::string& use_case_name,
                                 Priority priority) {
  // TODO(crbug.com/548711885): Bandaid fix to avoid downloading scam detection
  // model at foreground priority.
  if (use_case_name == ToUseCaseName(mojom::OnDeviceFeature::kScamDetection)) {
    priority = Priority::kBestEffort;
  }

  TRACE_EVENT("optimization_guide", "UsageTracker::RaisePriority", "use_case",
              use_case_name, "priority", static_cast<int>(priority));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<Priority> previous_priority = GetPriority(use_case_name);
  if (priority == Priority::kUserBlocking) {
    user_blocking_use_cases_.insert(use_case_name);
  }
  model_execution::prefs::RecordUseCaseUsage(local_state_, use_case_name);

  bool priority_increased =
      !previous_priority.has_value() || priority > *previous_priority;

  if (priority_increased) {
    for (auto& o : observers_) {
      o.OnPriorityIncrease(use_case_name, previous_priority);
    }
  }
}

std::optional<UsageTracker::Priority> UsageTracker::GetPriority(
    const std::string& use_case_name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (user_blocking_use_cases_.contains(use_case_name)) {
    return Priority::kUserBlocking;
  }
  if (model_execution::prefs::WasUseCaseRecentlyUsed(&*local_state_,
                                                        use_case_name)) {
    return Priority::kBestEffort;
  }
  return std::nullopt;
}

void UsageTracker::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void UsageTracker::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void UsageTracker::SetPriority(const std::string& use_case_name,
                               std::optional<Priority> priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!priority.has_value()) {
    user_blocking_use_cases_.erase(use_case_name);
    model_execution::prefs::ClearUseCaseUsage(&*local_state_, use_case_name);
    return;
  }
  RaisePriority(use_case_name, *priority);
  if (*priority != Priority::kUserBlocking) {
    user_blocking_use_cases_.erase(use_case_name);
  }
}

void UsageTracker::ClearAllUseCaseUsages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_blocking_use_cases_.clear();
  model_execution::prefs::ClearAllUseCaseUsages(&*local_state_);
}

}  // namespace optimization_guide
