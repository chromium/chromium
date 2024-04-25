// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/event_validator.h"

#include <cstdint>
#include <string_view>

namespace metrics::structured {

EventValidator::EventValidator(uint64_t event_hash, bool force_record)
    : event_hash_(event_hash), force_record_(force_record) {}
EventValidator::~EventValidator() = default;

uint64_t EventValidator::event_hash() const {
  return event_hash_;
}

bool EventValidator::can_force_record() const {
  return force_record_;
}

std::optional<EventValidator::MetricMetadata> EventValidator::GetMetricMetadata(
    const std::string& metric_name) const {
  const auto it = metric_metadata_.find(metric_name);
  if (it == metric_metadata_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<std::string_view> EventValidator::GetMetricName(
    uint64_t metric_name_hash) const {
  const auto it = metrics_name_map_.find(metric_name_hash);
  if (it == metrics_name_map_.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace metrics::structured
