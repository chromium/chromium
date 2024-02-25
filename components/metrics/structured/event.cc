// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/event.h"

#include <map>
#include <memory>
#include <string>

#include "base/uuid.h"
#include "base/values.h"

namespace metrics::structured {

Event::MetricValue::MetricValue(MetricType type, base::Value value)
    : type(type), value(std::move(value)) {}

Event::MetricValue::MetricValue(Event::MetricValue&& other) = default;
Event::MetricValue& Event::MetricValue::operator=(Event::MetricValue&& other) =
    default;

bool Event::MetricValue::operator==(const Event::MetricValue& rhs) const {
  return type == rhs.type && value == rhs.value;
}

Event::MetricValue::~MetricValue() = default;

Event::EventSequenceMetadata::EventSequenceMetadata(int reset_counter)
    : reset_counter(reset_counter),
      event_unique_id(base::Uuid::GenerateRandomV4().AsLowercaseString()) {}

Event::EventSequenceMetadata::EventSequenceMetadata(
    const Event::EventSequenceMetadata& other) = default;
Event::EventSequenceMetadata& Event::EventSequenceMetadata::operator=(
    const Event::EventSequenceMetadata& other) = default;

Event::EventSequenceMetadata::~EventSequenceMetadata() = default;

Event::Event() = default;
Event::Event(const std::string& project_name, const std::string& event_name)
    : project_name_(project_name), event_name_(event_name) {}
Event::Event(const std::string& project_name,
             const std::string& event_name,
             bool is_event_sequence)
    : project_name_(project_name),
      event_name_(event_name),
      is_event_sequence_(is_event_sequence) {}

Event::~Event() = default;

Event::Event(Event&& other)
    : project_name_(std::move(other.project_name_)),
      event_name_(std::move(other.event_name_)),
      metric_values_(std::move(other.metric_values_)),
      recorded_time_since_boot_(std::move(other.recorded_time_since_boot_)),
      event_sequence_metadata_(std::move(other.event_sequence_metadata_)),
      is_event_sequence_(other.is_event_sequence_) {}

Event& Event::operator=(Event&& other) {
  project_name_ = std::move(other.project_name_);
  event_name_ = std::move(other.event_name_);
  metric_values_ = std::move(other.metric_values_);
  recorded_time_since_boot_ = std::move(other.recorded_time_since_boot_);
  event_sequence_metadata_ = std::move(other.event_sequence_metadata_);
  is_event_sequence_ = other.is_event_sequence_;
  return *this;
}

bool Event::IsEventSequenceType() const {
  return is_event_sequence_;
}

Event Event::Clone() const {
  auto clone = Event(project_name_, event_name_, is_event_sequence_);
  for (const auto& metric : metric_values()) {
    const Event::MetricValue& metric_value = metric.second;
    clone.AddMetric(metric.first, metric_value.type,
                    metric_value.value.Clone());
  }
  clone.recorded_time_since_boot_ = recorded_time_since_boot_;
  clone.event_sequence_metadata_ = event_sequence_metadata_;
  return clone;
}

const Event::EventSequenceMetadata& Event::event_sequence_metadata() const {
  CHECK(event_sequence_metadata_.has_value());
  return event_sequence_metadata_.value();
}

const base::TimeDelta Event::recorded_time_since_boot() const {
  CHECK(recorded_time_since_boot_.has_value());
  return recorded_time_since_boot_.value();
}

bool Event::AddMetric(const std::string& metric_name,
                      Event::MetricType type,
                      base::Value&& value) {
  bool valid = true;
  switch (type) {
    case MetricType::kHmac:
      valid = value.is_string();
      break;
    // no base::LongValue so int64_t is encoded in a string.
    case MetricType::kLong:
      valid = value.is_string();
      break;
    case MetricType::kInt:
      valid = value.is_int();
      break;
    case MetricType::kDouble:
      valid = value.is_double();
      break;
    case MetricType::kRawString:
      valid = value.is_string();
      break;
    case MetricType::kBoolean:
      valid = value.is_bool();
      break;
  }
  if (!valid) {
    return false;
  }

  auto pair =
      metric_values_.emplace(metric_name, MetricValue(type, std::move(value)));
  return pair.second;
}

void Event::SetEventSequenceMetadata(
    const Event::EventSequenceMetadata& event_sequence_metadata) {
  event_sequence_metadata_ = event_sequence_metadata;
}

void Event::SetRecordedTimeSinceBoot(base::TimeDelta recorded_time_since_boot) {
  recorded_time_since_boot_ = recorded_time_since_boot;
}

}  // namespace metrics::structured
