// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/ukm_config.h"

namespace segmentation_platform {

UkmConfig::UkmConfig() = default;
UkmConfig::~UkmConfig() = default;

UkmConfig::MergeResult UkmConfig::Merge(const UkmConfig& config) {
  MergeResult result = UkmConfig::NO_NEW_EVENT;
  for (auto& it : config.metrics_for_event_) {
    auto inserted = metrics_for_event_.insert(
        std::make_pair(it.first, base::flat_set<UkmMetricHash>()));
    inserted.first->second.insert(it.second.begin(), it.second.end());
    if (inserted.second) {
      result = UkmConfig::NEW_EVENT_ADDED;
    }
  }
  return result;
}

base::flat_set<uint64_t> UkmConfig::GetRawObservedEvents() const {
  base::flat_set<uint64_t> events;
  for (const auto& it : metrics_for_event_) {
    events.insert(it.first.GetUnsafeValue());
  }
  return events;
}

const base::flat_set<UkmMetricHash>* UkmConfig::GetObservedMetrics(
    UkmEventHash event) {
  const auto& it = metrics_for_event_.find(event);
  if (it != metrics_for_event_.end()) {
    return &it->second;
  }
  return nullptr;
}

void UkmConfig::AddEvent(UkmEventHash event_hash,
                         const base::flat_set<UkmMetricHash>& metrics) {
  metrics_for_event_[event_hash].insert(metrics.begin(), metrics.end());
}

bool UkmConfig::operator==(const UkmConfig& other) const {
  return metrics_for_event_ == other.metrics_for_event_;
}

}  // namespace segmentation_platform
