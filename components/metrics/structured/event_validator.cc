// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/event_validator.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

#include "components/metrics/structured/structured_metrics_validator.h"

namespace metrics::structured {

std::optional<EventValidator::MetricMetadata> EventValidator::GetMetricMetadata(
    std::string_view metric_name) const {
  if (!can_validate_metrics_) {
    return std::nullopt;
  }
  const auto metrics =
      validator::Validators::GetMetrics(metrics_offset_, metrics_count_);
  const auto it = std::ranges::lower_bound(
      metrics, metric_name, std::less{}, [](const MetricEntry& entry) {
        return validator::Validators::GetString(entry.name_offset,
                                                entry.name_len);
      });
  if (it == metrics.end() ||
      validator::Validators::GetString(it->name_offset, it->name_len) !=
          metric_name) {
    return std::nullopt;
  }
  return MetricMetadata{it->metric_type, it->name_hash};
}

std::optional<std::string_view> EventValidator::GetMetricName(
    uint64_t metric_name_hash) const {
  for (const MetricEntry& entry :
       validator::Validators::GetMetrics(metrics_offset_, metrics_count_)) {
    if (entry.name_hash == metric_name_hash) {
      return validator::Validators::GetString(entry.name_offset,
                                              entry.name_len);
    }
  }
  return std::nullopt;
}

}  // namespace metrics::structured
