// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_MOJOM_EVENT_MOJOM_TRAITS_H_
#define COMPONENTS_METRICS_STRUCTURED_MOJOM_EVENT_MOJOM_TRAITS_H_

#include <memory>
#include <optional>

#include "base/strings/string_number_conversions.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/mojom/event.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"

namespace mojo {

// Converts MetricValue into the union mojom::MetricValue and vice versa.
template <>
struct UnionTraits<metrics::structured::mojom::MetricValueDataView,
                   metrics::structured::Event::MetricValue> {
  static metrics::structured::mojom::MetricValueDataView::Tag GetTag(
      const metrics::structured::Event::MetricValue& metric_value);

  static const std::string& hmac_value(
      const metrics::structured::Event::MetricValue& metric_value) {
    return metric_value.value.GetString();
  }

  static int64_t long_value(
      const metrics::structured::Event::MetricValue& metric_value) {
    int64_t long_value;
    base::StringToInt64(metric_value.value.GetString(), &long_value);
    return long_value;
  }

  static int32_t int_value(
      const metrics::structured::Event::MetricValue& metric_value) {
    return metric_value.value.GetInt();
  }

  static double double_value(
      const metrics::structured::Event::MetricValue& metric_value) {
    return metric_value.value.GetDouble();
  }

  static const std::string& raw_str_value(
      const metrics::structured::Event::MetricValue& metric_value) {
    return metric_value.value.GetString();
  }

  static bool bool_value(
      const metrics::structured::Event::MetricValue& metric_value) {
    return metric_value.value.GetBool();
  }

  static bool Read(metrics::structured::mojom::MetricValueDataView metric,
                   metrics::structured::Event::MetricValue* out);
};

// Converts mojom::Event to/from Event, so that Event can be used throughout the
// codebase without any direct reference to mojom::Event.
template <>
class StructTraits<metrics::structured::mojom::EventDataView,
                   metrics::structured::Event> {
 public:
  static const std::string& project_name(
      const metrics::structured::Event& event) {
    return event.project_name();
  }

  static const std::string& event_name(
      const metrics::structured::Event& event) {
    return event.event_name();
  }

  static const std::map<std::string, metrics::structured::Event::MetricValue>&
  metrics(const metrics::structured::Event& event) {
    return event.metric_values();
  }

  static std::optional<base::TimeDelta> system_uptime(
      const metrics::structured::Event& event);

  static bool is_event_sequence(const metrics::structured::Event& event) {
    return event.IsEventSequenceType();
  }

  static bool Read(metrics::structured::mojom::EventDataView event,
                   metrics::structured::Event* out);
};

}  // namespace mojo

#endif  // COMPONENTS_METRICS_STRUCTURED_MOJOM_EVENT_MOJOM_TRAITS_H_
