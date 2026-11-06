// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_EVENT_VALIDATOR_H_
#define COMPONENTS_METRICS_STRUCTURED_EVENT_VALIDATOR_H_

#include <cstdint>
#include <optional>
#include <string_view>

#include "components/metrics/structured/event.h"

namespace metrics::structured {

// Validates events and metrics against metadata generated from structured.xml.
class EventValidator final {
 public:
  // Metadata about a registered metric.
  struct MetricMetadata {
    Event::MetricType metric_type;
    uint64_t metric_name_hash;
  };

  // Compact static table entry generated for each metric.
  struct MetricEntry {
    uint64_t name_hash;
    uint16_t name_offset;
    uint16_t name_len;
    Event::MetricType metric_type;
  };

  constexpr EventValidator(uint64_t event_hash,
                           uint16_t name_offset,
                           uint8_t name_len,
                           uint16_t metrics_offset,
                           uint8_t metrics_count,
                           bool force_record,
                           bool can_validate_metrics = true)
      : event_hash_(event_hash),
        name_offset_(name_offset),
        metrics_offset_(metrics_offset),
        metrics_count_(metrics_count),
        name_len_(name_len),
        force_record_(force_record),
        can_validate_metrics_(can_validate_metrics) {}

  // Should not be copied or moved.
  EventValidator(const EventValidator&) = delete;
  EventValidator& operator=(const EventValidator& other) = delete;

  ~EventValidator() = default;

  // Returns the metric metadata if |metric_name| is a valid metric for this
  // event.
  std::optional<MetricMetadata> GetMetricMetadata(
      std::string_view metric_name) const;

  std::optional<std::string_view> GetMetricName(
      uint64_t metric_name_hash) const;

  uint64_t event_hash() const { return event_hash_; }
  bool can_force_record() const { return force_record_; }
  uint16_t name_offset() const { return name_offset_; }
  uint8_t name_len() const { return name_len_; }

 private:
  const uint64_t event_hash_;
  const uint16_t name_offset_;
  const uint16_t metrics_offset_;
  const uint8_t metrics_count_;
  const uint8_t name_len_;
  // Flag for whether an event can be recorded, not uploaded, before a user has
  // been able to opt-in.
  const bool force_record_;
  const bool can_validate_metrics_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_EVENT_VALIDATOR_H_
