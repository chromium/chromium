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
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"

namespace metrics {

// Evaluates whether a given metric should be allowed, dropped, or renamed based
// on embedder-supplied defaults and an optional server-provided configuration.
class COMPONENT_EXPORT(METRICS_MAPPING) MetricsNameMapper {
 public:
  // A built-in rule with the meaning of a served `MetricsNameMapping`: keeps
  // `metric_name`, or renames it to `new_metric_name` if that is not empty.
  struct Rule {
    std::string_view metric_name;
    std::string_view new_metric_name;
  };

  // Registers the embedder's built-in `rules` and the `allowed_prefixes` of
  // metric names that are kept when no rule matches. Copies its arguments into
  // static storage. Call at most once, during startup before any mapper is
  // constructed. Metric names and prefixes must not be empty.
  //
  // Like served rules, these apply only to histograms that a mapper evaluates,
  // all of which come from top-chrome WebUI renderers (`IsWebiumRenderer()` in
  // `SubprocessMetricsProvider`, `is_webium_renderer` in
  // `HistogramSynchronizer`). See `GetMetricsNameIfAllowed()` for the matching
  // order.
  static void SetDefaults(base::span<const Rule> rules,
                          base::span<const std::string_view> allowed_prefixes);

  // Clears the defaults so that a test can register its own. Names returned
  // for built-in rules become invalid.
  static void ResetDefaultsForTesting();

  explicit MetricsNameMapper(const std::optional<std::string>& base64_config);
  MetricsNameMapper(const MetricsNameMapper&) = delete;
  MetricsNameMapper& operator=(const MetricsNameMapper&) = delete;
  ~MetricsNameMapper();

  static std::unique_ptr<MetricsNameMapper> CreateInstance();

  // Evaluates `metric_name` against the registered defaults and server rules,
  // using the first match:
  // 1. A built-in rule from `SetDefaults()`. Built-in rules come first so that
  //    no server rule can undo a built-in rename.
  // 2. A server-provided rule in `exact_rules_`.
  // 3. An allowed prefix from `SetDefaults()`, which keeps `metric_name`.
  // A matching rule returns its new name, or `metric_name` if it has none. If
  // nothing matches, returns an empty string_view to drop the metric.
  //
  // The returned string_view relies on the underlying storage from the
  // passed-in `metric_name`, `exact_rules_`, or the static storage that
  // `SetDefaults()` fills, so it stays valid while `metric_name` and this
  // instance are alive.
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
