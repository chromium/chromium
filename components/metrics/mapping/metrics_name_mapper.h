// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_MAPPING_METRICS_NAME_MAPPER_H_
#define COMPONENTS_METRICS_MAPPING_METRICS_NAME_MAPPER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "components/metrics/mapping/metrics_name_mapping.pb.h"

namespace metrics {

// Evaluates whether a given metric should be allowed, dropped, or renamed based
// on a server-provided configuration.
class COMPONENT_EXPORT(METRICS_MAPPING) MetricsNameMapper {
 public:
  explicit MetricsNameMapper(const std::optional<std::string>& base64_config);
  MetricsNameMapper(const MetricsNameMapper&) = delete;
  MetricsNameMapper& operator=(const MetricsNameMapper&) = delete;
  ~MetricsNameMapper();

  static std::unique_ptr<MetricsNameMapper> CreateInstance();

  // Searches a metric within the loaded rules.
  //
  // Checks the keys of `exact_rules_` for an exact match on `metric_name`.
  // If the matching rule specifies:
  // - A `new_metric_name`: Returns the `new_metric_name`.
  // - No `new_metric_name`: Returns the original `metric_name`.
  // If no rule matches `metric_name`, an empty string_view is returned,
  // indicating that the metric should be dropped.
  //
  // The returned string_view relies on the underlying storage from either the
  // passed-in `metric_name` or the mapped string backed by `exact_rules_`,
  // which outlives any string_view returned from here as long as this instance
  // is still alive.
  std::string_view GetMetricsNameIfAllowed(std::string_view metric_name) const;

  base::WeakPtr<MetricsNameMapper> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Maps a metric_name to an optional new_metric_name.
  // If the value is std::nullopt, the metric is allowed with its original name.
  base::flat_map<std::string, std::optional<std::string>> exact_rules_;

  base::WeakPtrFactory<MetricsNameMapper> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_MAPPING_METRICS_NAME_MAPPER_H_
