// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/event_base.h"

#include "components/metrics/structured/recorder.h"

namespace metrics {
namespace structured {

EventBase::EventBase(uint64_t event_name_hash,
                     uint64_t project_name_hash,
                     IdType id_type,
                     IdScope id_scope,
                     StructuredEventProto_EventType event_type)
    : event_name_hash_(event_name_hash),
      project_name_hash_(project_name_hash),
      id_type_(id_type),
      id_scope_(id_scope),
      event_type_(event_type) {}
EventBase::EventBase(const EventBase& other) = default;
EventBase::~EventBase() = default;

void EventBase::Record() {
  Recorder::GetInstance()->Record(std::move(*this));
}

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

}  // namespace structured
}  // namespace metrics
