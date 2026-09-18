// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/usage_tracker.h"

#include <vector>

#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/prefs/pref_service.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

namespace optimization_guide {

namespace {

std::string PrefKey(mojom::OnDeviceFeature feature) {
  return base::NumberToString(
      static_cast<uint64_t>(ToModelExecutionFeatureProto(feature)));
}

bool WasUsedWithin(std::optional<base::Time> last_use, base::TimeDelta period) {
  if (!last_use) {
    return false;
  }
  auto time_since_use = base::Time::Now() - *last_use;
  // Note: Since we're storing a base::Time, we need to consider the possibility
  // of clock changes.
  return time_since_use < period && time_since_use > -period;
}

}  // namespace

BASE_FEATURE(kOnDeviceModelUsageTracking, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOnDeviceModelEviction, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kRecentUsePeriod{
    &kOnDeviceModelUsageTracking, "recent_use_period", base::Days(30)};

const base::FeatureParam<base::TimeDelta> kRetentionPeriod{
    &kOnDeviceModelUsageTracking, "retention_period", base::Days(90)};

UsageTracker::UsageTracker(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
  PruneOldUsagePrefs();
}

UsageTracker::~UsageTracker() = default;

void UsageTracker::RaisePriority(const std::string& use_case_name,
                                 Priority priority) {
  DCHECK_GE(priority, Priority::kBestEffort);
  // TODO(crbug.com/548711885): Bandaid fix to avoid downloading scam detection
  // model at foreground priority.
  if (use_case_name == ToUseCaseName(mojom::OnDeviceFeature::kScamDetection)) {
    priority = Priority::kBestEffort;
  }

  TRACE_EVENT("optimization_guide", "UsageTracker::RaisePriority", "use_case",
              use_case_name, "priority", static_cast<int>(priority));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Priority previous_priority = GetPriority(use_case_name);
  if (priority == Priority::kUserBlocking) {
    user_blocking_use_cases_.insert(use_case_name);
  } else if (previous_priority != Priority::kUserBlocking) {
    user_blocking_use_cases_.erase(use_case_name);
  }
  RecordUseCaseUsage(use_case_name);

  if (priority > previous_priority) {
    for (auto& o : observers_) {
      o.OnPriorityIncrease(use_case_name, previous_priority);
    }
  }
}

UsageTracker::Priority UsageTracker::GetPriority(
    const std::string& use_case_name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (WasUseCaseUsedWithin(use_case_name, kRecentUsePeriod.Get())) {
    if (user_blocking_use_cases_.contains(use_case_name)) {
      return Priority::kUserBlocking;
    }
    return Priority::kBestEffort;
  }
  if (!base::FeatureList::IsEnabled(kOnDeviceModelEviction) ||
      WasUseCaseUsedWithin(use_case_name, kRetentionPeriod.Get())) {
    return Priority::kRetain;
  }
  return Priority::kEvictable;
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
                               Priority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (priority == Priority::kEvictable) {
    user_blocking_use_cases_.erase(use_case_name);
    ClearUseCaseUsage(use_case_name);
    return;
  }
  // Only `kBestEffort` and `kUserBlocking` can be set directly. A use case
  // transitions to `kRetain` only when `kRecentUsePeriod` elapses since its
  // last usage.
  RaisePriority(use_case_name, priority);
  if (priority != Priority::kUserBlocking) {
    user_blocking_use_cases_.erase(use_case_name);
  }
}

void UsageTracker::ClearAllUseCaseUsages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_blocking_use_cases_.clear();
  local_state_->ClearPref(
      model_execution::prefs::localstate::kLastUsageByFeature);
}

void UsageTracker::PruneOldUsagePrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ::prefs::ScopedDictionaryPrefUpdate update(
      local_state_, model_execution::prefs::localstate::kLastUsageByFeature);
  std::vector<std::string> keys_to_prune;  // Avoid iterator invalidation.
  for (auto kv : *update->AsConstDict()) {
    if (!WasUsedWithin(base::ValueToTime(kv.second), kRetentionPeriod.Get())) {
      keys_to_prune.emplace_back(kv.first);
    }
  }
  for (const auto& key : keys_to_prune) {
    update->Remove(key);
  }
}

void UsageTracker::RecordUseCaseUsage(const std::string& use_case_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ::prefs::ScopedDictionaryPrefUpdate update(
      local_state_, model_execution::prefs::localstate::kLastUsageByFeature);
  update->Set(use_case_name, base::TimeToValue(base::Time::Now()));
}

void UsageTracker::ClearUseCaseUsage(const std::string& use_case_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ::prefs::ScopedDictionaryPrefUpdate update(
      local_state_, model_execution::prefs::localstate::kLastUsageByFeature);
  update->Remove(use_case_name);
  // TODO(crbug.com/489511499): Remove this fallback once all features have
  // migrated to using RecordUseCaseUsage with string names.
  if (std::optional<mojom::OnDeviceFeature> feature =
          GetFeatureForUseCase(use_case_name)) {
    update->Remove(PrefKey(*feature));
  }
}

bool UsageTracker::WasUseCaseUsedWithin(const std::string& use_case_name,
                                        base::TimeDelta period) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto& dict = local_state_->GetDict(
      model_execution::prefs::localstate::kLastUsageByFeature);

  const auto* value = dict.Find(use_case_name);
  if (value && WasUsedWithin(base::ValueToTime(*value), period)) {
    return true;
  }

  // Fallback to legacy integer keys mapped to this use case.
  // TODO(crbug.com/489511499): Remove this fallback once all features have
  // migrated to using RecordUseCaseUsage with string names.
  if (std::optional<mojom::OnDeviceFeature> feature =
          GetFeatureForUseCase(use_case_name)) {
    value = dict.Find(PrefKey(*feature));
    if (value && WasUsedWithin(base::ValueToTime(*value), period)) {
      return true;
    }
  }
  return false;
}

}  // namespace optimization_guide
