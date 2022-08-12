// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/event_base.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/metrics/structured/enums.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/event_validator.h"
#include "components/metrics/structured/project_validator.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/metrics/structured/structured_metrics_validator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics {
namespace structured {

EventBase::EventBase(uint64_t event_name_hash,
                     uint64_t project_name_hash,
                     IdType id_type,
                     IdScope id_scope,
                     StructuredEventProto_EventType event_type,
                     int key_rotation_period)
    : event_name_hash_(event_name_hash),
      project_name_hash_(project_name_hash),
      id_type_(id_type),
      id_scope_(id_scope),
      event_type_(event_type),
      key_rotation_period_(key_rotation_period) {}
EventBase::EventBase(const EventBase& other) = default;
EventBase::~EventBase() = default;

absl::optional<int> EventBase::LastKeyRotation() {
  return Recorder::GetInstance()->LastKeyRotation(project_name_hash_);
}

void EventBase::AddHmacMetric(uint64_t name_hash, const std::string& value) {
  Metric metric(name_hash, MetricType::kHmac);
  metric.hmac_value = value;
  metrics_.push_back(metric);
}

void EventBase::AddIntMetric(uint64_t name_hash, int64_t value) {
  Metric metric(name_hash, MetricType::kInt);
  metric.int_value = value;
  metrics_.push_back(metric);
}

void EventBase::AddRawStringMetric(uint64_t name_hash,
                                   const std::string& value) {
  Metric metric(name_hash, MetricType::kRawString);
  metric.string_value = value;
  metrics_.push_back(metric);
}

EventBase::Metric::Metric(uint64_t name_hash, MetricType type)
    : name_hash(name_hash), type(type) {}
EventBase::Metric::~Metric() = default;

bool EventBase::Metric::operator==(const EventBase::Metric& other) const {
  // Terminate early to avoid accessing irrelevant values.
  if (name_hash != other.name_hash || type != other.type)
    return false;

  switch (type) {
    case MetricType::kHmac:
      return hmac_value == other.hmac_value;
    case MetricType::kInt:
      return int_value == other.int_value;
    case MetricType::kRawString:
      return string_value == other.string_value;
  }

  NOTREACHED();
}

// static
absl::optional<EventBase> EventBase::FromEvent(const Event& event) {
  auto project_validator = validator::GetProjectValidator(event.project_name());
  if (!project_validator.has_value())
    return absl::nullopt;

  const auto event_validator =
      project_validator.value()->GetEventValidator(event.event_name());
  if (!event_validator.has_value())
    return absl::nullopt;

  EventBase event_base(event_validator.value()->event_hash(),
                       project_validator.value()->project_hash(),
                       project_validator.value()->id_type(),
                       project_validator.value()->id_scope(),
                       project_validator.value()->event_type(),
                       project_validator.value()->key_rotation_period());

  for (const auto& metric_value : event.metric_values()) {
    // Validate that both name and metric type are valid structured metrics.
    absl::optional<EventValidator::MetricMetadata> metric_metadata =
        event_validator.value()->GetMetricMetadata(metric_value.first);
    if (!metric_metadata.has_value() ||
        metric_metadata.value().metric_type != metric_value.second.type) {
      return absl::nullopt;
    }

    const auto& value = metric_value.second.value;
    switch (metric_value.second.type) {
      case Event::MetricType::kHmac:
        event_base.AddHmacMetric(metric_metadata.value().metric_name_hash,
                                 value.GetString());
        break;
      case Event::MetricType::kLong:
        int64_t long_value;
        base::StringToInt64(value.GetString(), &long_value);
        event_base.AddIntMetric(metric_metadata.value().metric_name_hash,
                                long_value);
        break;
      case Event::MetricType::kInt:
        event_base.AddIntMetric(metric_metadata.value().metric_name_hash,
                                value.GetInt());
        break;
      case Event::MetricType::kRawString:
        event_base.AddRawStringMetric(metric_metadata.value().metric_name_hash,
                                      value.GetString());
        break;
      // TODO(jongahn): Support these types.
      case Event::MetricType::kBoolean:
      case Event::MetricType::kDouble:
        VLOG(1) << "Metric type not supported";
        NOTREACHED();
        break;
    }
  }
  return event_base;
}

}  // namespace structured
}  // namespace metrics
