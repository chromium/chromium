// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/debug/structured/structured_metrics_utils.h"

#include "base/i18n/number_formatting.h"
#include "components/metrics/structured/structured_metrics_service.h"

namespace metrics::structured {

namespace {

// Creates a dictionary that represents a key-value pair.
base::Value::Dict CreateKeyValue(base::StringPiece key, base::Value value) {
  base::Value::Dict result;
  result.Set("key", key);
  result.Set("value", std::move(value));
  return result;
}

// Creates a list of metrics represented by a key-value pair from the metrics of
// an event.
base::Value::List CreateMetricsList(
    const std::map<std::string, Event::MetricValue>& metrics) {
  base::Value::List result;
  for (const auto& metric : metrics) {
    result.Append(CreateKeyValue(metric.first, metric.second.value.Clone()));
  }
  return result;
}

// Creates an event metadata dictionary from an event.
base::Value::Dict CreateEventMetadataDict(const Event& event) {
  base::Value::Dict metadata;
  const auto& event_metadata = event.event_sequence_metadata();
  metadata.Set(
      "systemUptimeMs",
      base::FormatNumber(event.recorded_time_since_boot().InMilliseconds()));
  metadata.Set("id", event_metadata.event_unique_id);
  metadata.Set("resetCounter", event_metadata.reset_counter);
  return metadata;
}

// Creates a dictionary from an event.
base::Value::Dict CreateEventDict(const Event& event) {
  base::Value::Dict result;

  result.Set("project", event.project_name());
  result.Set("event", event.event_name());
  result.Set("metrics", CreateMetricsList(event.metric_values()));

  if (event.IsEventSequenceType()) {
    result.Set("type", "sequence");
    result.Set("sequenceMetadata", CreateEventMetadataDict(event));
  } else {
    result.Set("type", "normal");
  }

  return result;
}

}  // namespace

base::Value ConvertEventsIntoValue(const std::vector<Event>& events) {
  base::Value::List result;

  for (const auto& event : events) {
    result.Append(CreateEventDict(event));
  }

  return base::Value(std::move(result));
}

base::Value GetStructuredMetricsSummary(StructuredMetricsService* service) {
  base::Value::Dict result;
  result.Set("enabled", service->recording_enabled());
  auto id =
      service->recorder()->key_data_provider()->GetSecondaryId("CrOSEvents");
  if (id.has_value()) {
    result.Set("crosDeviceId", base::NumberToString(id.value()));
  }
  return base::Value(std::move(result));
}

}  // namespace metrics::structured
