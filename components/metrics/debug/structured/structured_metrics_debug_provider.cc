// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/debug/structured/structured_metrics_debug_provider.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "components/metrics/structured/event_validator.h"
#include "components/metrics/structured/project_validator.h"
#include "components/metrics/structured/proto/event_storage.pb.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_metrics_service.h"
#include "components/metrics/structured/structured_metrics_validator.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {
namespace {

struct EventInfo {
  std::string_view project_name;
  std::string_view event_name;
  raw_ptr<const EventValidator> event_validator;

  // Normalizes the name into an easier to read format as defined in the
  // structured.xml.
  std::string NormalizeProjectName() const;
  std::string NormalizeEventName() const;
};

std::string Normalize(std::string_view value) {
  std::string result;
  base::ReplaceChars(value, "_", ".", &result);
  return result;
}

std::string EventInfo::NormalizeProjectName() const {
  return Normalize(project_name);
}

std::string EventInfo::NormalizeEventName() const {
  return Normalize(event_name);
}

// Retrieves information about an event that is needed for rendering.
std::optional<EventInfo> GetEventInfo(const StructuredEventProto& proto) {
  validator::Validators* validators = validator::Validators::Get();
  auto project_name = validators->GetProjectName(proto.project_name_hash());
  if (!project_name.has_value()) {
    return std::nullopt;
  }

  // This will not fail.
  const auto* project_validator =
      validators->GetProjectValidator(*project_name);
  CHECK(project_validator);

  const auto event_name =
      project_validator->GetEventName(proto.event_name_hash());
  if (!event_name.has_value()) {
    return std::nullopt;
  }

  // This will not fail.
  const auto* event_validator =
      project_validator->GetEventValidator(*event_name);
  CHECK(event_validator);

  return EventInfo{.project_name = *project_name,
                   .event_name = *event_name,
                   .event_validator = event_validator};
}

// Creates a dictionary that represents a key-value pair.
base::Value::Dict CreateKeyValue(std::string_view key, base::Value value) {
  base::Value::Dict result;
  result.Set("key", key);
  result.Set("value", std::move(value));
  return result;
}

std::optional<base::Value> MetricToValue(
    const StructuredEventProto::Metric& metric) {
  using Metric = StructuredEventProto::Metric;
  switch (metric.value_case()) {
    case Metric::kValueHmac:
      return base::Value(base::NumberToString(metric.value_hmac()));
    case Metric::kValueInt64:
      return base::Value(base::NumberToString(metric.value_int64()));
    case Metric::kValueString:
      return base::Value(metric.value_string());
    case Metric::kValueDouble:
      return base::Value(metric.value_double());
    case Metric::kValueRepeatedInt64: {
      base::Value::List list;
      for (int value : metric.value_repeated_int64().values()) {
        list.Append(value);
      }
      return base::Value(std::move(list));
    }
    case Metric::VALUE_NOT_SET:
      return std::nullopt;
  }
}

// Creates a list of metrics represented by a key-value pair from the metrics of
// an event.
base::Value::List CreateMetricsList(const google::protobuf::RepeatedPtrField<
                                        StructuredEventProto::Metric>& metrics,
                                    const EventValidator* event_validator) {
  base::Value::List result;
  for (const auto& metric : metrics) {
    std::string metric_name =
        event_validator
            ? std::string(event_validator->GetMetricName(metric.name_hash())
                              .value_or("unknown"))
            : base::NumberToString(metric.name_hash());
    auto value = MetricToValue(metric);
    if (!value.has_value()) {
      continue;
    }
    result.Append(CreateKeyValue(metric_name, std::move(*value)));
  }
  return result;
}

// Creates an event metadata dictionary from an event.
base::Value::Dict CreateEventMetadataDict(
    const StructuredEventProto::EventSequenceMetadata& sequence_metadata) {
  base::Value::Dict metadata;
  metadata.Set("systemUptimeMs",
               base::FormatNumber(sequence_metadata.system_uptime()));
  metadata.Set("id", base::NumberToString(sequence_metadata.event_unique_id()));
  metadata.Set("resetCounter",
               base::NumberToString(sequence_metadata.reset_counter()));
  return metadata;
}

// Creates a dictionary from an event.
base::Value::Dict CreateEventDict(const StructuredEventProto& proto) {
  base::Value::Dict result;

  auto event_info = GetEventInfo(proto);
  const EventValidator* event_validator = nullptr;

  if (event_info.has_value()) {
    event_validator = event_info->event_validator;
    result.Set("project", event_info->NormalizeProjectName());
    result.Set("event", event_info->NormalizeEventName());
  } else {
    result.Set("project", base::NumberToString(proto.project_name_hash()));
    result.Set("event", base::NumberToString(proto.event_name_hash()));
  }

  result.Set("metrics", CreateMetricsList(proto.metrics(), event_validator));

  if (proto.event_type() == StructuredEventProto::SEQUENCE) {
    result.Set("type", "sequence");
    result.Set("sequenceMetadata",
               CreateEventMetadataDict(proto.event_sequence_metadata()));
  } else {
    result.Set("type", "metric");
  }

  return result;
}

}  // namespace

StructuredMetricsDebugProvider::StructuredMetricsDebugProvider(
    StructuredMetricsService* service)
    : service_(service) {
  CHECK(service);
  LoadRecordedEvents();
  service_->recorder()->AddEventsObserver(this);
}

StructuredMetricsDebugProvider::~StructuredMetricsDebugProvider() {
  service_->recorder()->RemoveEventsObserver(this);
}

void StructuredMetricsDebugProvider::OnEventRecorded(
    const StructuredEventProto& event) {
  events_.Append(CreateEventDict(event));
}

void StructuredMetricsDebugProvider::LoadRecordedEvents() {
  EventsProto proto;
  service_->recorder()->event_storage()->CopyEvents(&proto);
  for (const auto& event : proto.events()) {
    events_.Append(CreateEventDict(event));
  }
}

}  // namespace metrics::structured
