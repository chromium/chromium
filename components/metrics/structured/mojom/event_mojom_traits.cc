// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/mojom/event_mojom_traits.h"

#include <map>
#include <optional>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/mojom/event.mojom.h"

namespace mojo {

// static
metrics::structured::mojom::MetricValueDataView::Tag
UnionTraits<metrics::structured::mojom::MetricValueDataView,
            metrics::structured::Event::MetricValue>::
    GetTag(const metrics::structured::Event::MetricValue& metric_value) {
  switch (metric_value.type) {
    case metrics::structured::Event::MetricType::kHmac:
      return metrics::structured::mojom::MetricValueDataView::Tag::kHmacValue;
    case metrics::structured::Event::MetricType::kLong:
      return metrics::structured::mojom::MetricValueDataView::Tag::kLongValue;
    case metrics::structured::Event::MetricType::kInt:
      return metrics::structured::mojom::MetricValueDataView::Tag::kIntValue;
    case metrics::structured::Event::MetricType::kDouble:
      return metrics::structured::mojom::MetricValueDataView::Tag::kDoubleValue;
    case metrics::structured::Event::MetricType::kRawString:
      return metrics::structured::mojom::MetricValueDataView::Tag::kRawStrValue;
    case metrics::structured::Event::MetricType::kBoolean:
      return metrics::structured::mojom::MetricValueDataView::Tag::kBoolValue;
  }
}

// static
bool UnionTraits<metrics::structured::mojom::MetricValueDataView,
                 metrics::structured::Event::MetricValue>::
    Read(metrics::structured::mojom::MetricValueDataView metric,
         metrics::structured::Event::MetricValue* out) {
  switch (metric.tag()) {
    case metrics::structured::mojom::MetricValueDataView::Tag::kHmacValue: {
      std::string hmac_value;
      if (!metric.ReadHmacValue(&hmac_value))
        return false;
      out->type = metrics::structured::Event::MetricType::kHmac;
      out->value = base::Value(std::move(hmac_value));
      break;
    }
    case metrics::structured::mojom::MetricValueDataView::Tag::kLongValue:
      out->type = metrics::structured::Event::MetricType::kLong;
      out->value = base::Value(base::NumberToString(metric.long_value()));
      break;
    case metrics::structured::mojom::MetricValueDataView::Tag::kIntValue:
      out->type = metrics::structured::Event::MetricType::kInt;
      out->value = base::Value(metric.int_value());
      break;
    case metrics::structured::mojom::MetricValueDataView::Tag::kDoubleValue:
      out->type = metrics::structured::Event::MetricType::kDouble;
      out->value = base::Value(metric.double_value());
      break;
    case metrics::structured::mojom::MetricValueDataView::Tag::kRawStrValue: {
      std::string raw_str_value;
      if (!metric.ReadRawStrValue(&raw_str_value))
        return false;
      out->type = metrics::structured::Event::MetricType::kRawString;
      out->value = base::Value(std::move(raw_str_value));
      break;
    }
    case metrics::structured::mojom::MetricValueDataView::Tag::kBoolValue:
      out->type = metrics::structured::Event::MetricType::kBoolean;
      out->value = base::Value(metric.bool_value());
      break;
  }
  return true;
}

// static
std::optional<base::TimeDelta> StructTraits<
    metrics::structured::mojom::EventDataView,
    metrics::structured::Event>::system_uptime(const metrics::structured::Event&
                                                   event) {
  if (event.IsEventSequenceType())
    return event.recorded_time_since_boot();
  return std::nullopt;
}

// static
bool StructTraits<metrics::structured::mojom::EventDataView,
                  metrics::structured::Event>::
    Read(metrics::structured::mojom::EventDataView event,
         metrics::structured::Event* out) {
  std::string project_name, event_name;
  std::map<std::string, metrics::structured::Event::MetricValue> metrics;
  std::optional<base::TimeDelta> system_uptime;
  bool is_event_sequence = event.is_event_sequence();

  if (!event.ReadProjectName(&project_name) ||
      !event.ReadEventName(&event_name) || !event.ReadMetrics(&metrics) ||
      !event.ReadSystemUptime(&system_uptime))
    return false;

  *out =
      metrics::structured::Event(project_name, event_name, is_event_sequence);

  if (system_uptime.has_value())
    out->SetRecordedTimeSinceBoot(system_uptime.value());

  for (auto&& metric : metrics) {
    if (!out->AddMetric(metric.first, metric.second.type,
                        std::move(metric.second.value)))
      return false;
  }
  return true;
}

}  // namespace mojo
