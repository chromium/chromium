// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_EVENT_VALIDATOR_H_
#define COMPONENTS_METRICS_STRUCTURED_EVENT_VALIDATOR_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "components/metrics/structured/enums.h"
#include "components/metrics/structured/event.h"

namespace metrics::structured {

// Interface to be implemented by codegen for every event to validate
// messages received by the structured metric service.
class EventValidator {
 public:
  // Metadata about a registered metric.
  struct MetricMetadata {
    Event::MetricType metric_type;
    uint64_t metric_name_hash;
  };

  // Should not be copied or moved.
  EventValidator(const EventValidator&) = delete;
  EventValidator& operator=(const EventValidator& other) = delete;

  virtual ~EventValidator();

  // Returns the event validator if |metric_name| is a valid metric for this
  // event. This method is virtual because a static constexpr map will be
  // defined within each event validator implementation.
  std::optional<MetricMetadata> GetMetricMetadata(
      const std::string& metric_name) const;

  std::optional<std::string_view> GetMetricName(
      uint64_t metric_name_hash) const;

  uint64_t event_hash() const;
  bool can_force_record() const;

 protected:
  // Should not be constructed directly.
  explicit EventValidator(uint64_t event_hash, bool force_record);

  std::unordered_map<std::string_view, EventValidator::MetricMetadata>
      metric_metadata_;

  std::unordered_map<uint64_t, std::string_view> metrics_name_map_;

 private:
  uint64_t event_hash_;
  // Flag for whether an event can be recorded, not uploaded, before a user has
  // been able to opt-in.
  bool force_record_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_EVENT_VALIDATOR_H_
