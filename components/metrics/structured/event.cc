// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/event.h"

#include <map>
#include <memory>
#include <string>

#include "base/values.h"
#include "components/metrics/structured/structured_metrics_client.h"

namespace metrics {
namespace structured {

Event::MetricValue::MetricValue(MetricType type, base::Value value)
    : type(type), value(std::move(value)) {}

Event::MetricValue::MetricValue(Event::MetricValue&& other) = default;
Event::MetricValue& Event::MetricValue::operator=(Event::MetricValue&& other) =
    default;

bool Event::MetricValue::operator==(const Event::MetricValue& rhs) const {
  return type == rhs.type && value == rhs.value;
}

Event::MetricValue::~MetricValue() = default;

Event::Event() = default;
Event::Event(const std::string& project_name, const std::string& event_name)
    : project_name_(project_name), event_name_(event_name) {}
Event::~Event() = default;

Event::Event(Event&& other)
    : project_name_(std::move(other.project_name_)),
      event_name_(std::move(other.event_name_)) {
  metric_values_.insert(std::make_move_iterator(other.metric_values_.begin()),
                        std::make_move_iterator(other.metric_values_.end()));
}

Event& Event::operator=(Event&& other) {
  project_name_ = std::move(other.project_name_);
  event_name_ = std::move(other.event_name_);
  metric_values_.insert(std::make_move_iterator(other.metric_values_.begin()),
                        std::make_move_iterator(other.metric_values_.end()));
  return *this;
}

void Event::Record() {
  StructuredMetricsClient::Get()->Record(std::move(*this));
}

const std::string& Event::project_name() const {
  return project_name_;
}

const std::string& Event::event_name() const {
  return event_name_;
}

const std::map<std::string, Event::MetricValue>& Event::metric_values() const {
  return metric_values_;
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
  if (!valid)
    return false;

  auto pair =
      metric_values_.emplace(metric_name, MetricValue(type, std::move(value)));
  return pair.second;
}

}  // namespace structured
}  // namespace metrics
