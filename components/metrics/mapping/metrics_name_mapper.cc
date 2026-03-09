// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/mapping/metrics_name_mapper.h"

#include "base/base64.h"
#include "base/containers/map_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "components/metrics/mapping/metrics_mapping_features.h"

namespace metrics {
namespace {

base::flat_map<std::string, std::optional<std::string>> ParseConfig(
    const std::optional<std::string>& base64_config) {
  if (!base64_config || base64_config->empty()) {
    return {};
  }

  std::string serialized_proto;
  if (!base::Base64Decode(*base64_config, &serialized_proto)) {
    return {};
  }

  MetricsNameMappingConfiguration config;
  if (!config.ParseFromString(serialized_proto)) {
    return {};
  }

  base::flat_map<std::string, std::optional<std::string>> exact_rules;
  for (const auto& rule : config.rules()) {
    if (!rule.has_metric_name() || rule.metric_name().empty()) {
      // Wildcards are not supported, so skip any rule without a metric name.
      continue;
    }

    std::optional<std::string> new_name;
    if (rule.has_new_metric_name() && !rule.new_metric_name().empty()) {
      // Accepts only the non-empty new metric name.
      new_name = rule.new_metric_name();
    }

    // try_emplace only inserts if the key is not already present.
    // This correctly implements the "first rule wins" logic.
    exact_rules.try_emplace(rule.metric_name(), new_name);
  }

  return exact_rules;
}

}  // namespace

MetricsNameMapper::MetricsNameMapper(
    const std::optional<std::string>& base64_config)
    : exact_rules_(ParseConfig(base64_config)) {}

MetricsNameMapper::~MetricsNameMapper() = default;

// static
std::unique_ptr<MetricsNameMapper> MetricsNameMapper::CreateInstance() {
  if (base::FeatureList::IsEnabled(metrics::features::kWebiumMetricsMapping)) {
    return std::make_unique<MetricsNameMapper>(
        metrics::features::kWebiumMetricsMappingConfig.Get());
  }
  return nullptr;
}

std::string_view MetricsNameMapper::GetMetricsNameIfAllowed(
    std::string_view metric_name) const {
  if (auto* exact_rule = base::FindOrNull(exact_rules_, metric_name)) {
    if (exact_rule->has_value()) {
      return exact_rule->value();
    }
    // Rule exists but has no override name; return the original name.
    return metric_name;
  }

  // If no rule matches, the metric is dropped (return empty string).
  // Note: if parsing completely fails, `exact_rules_` is empty,
  // meaning *all* metrics will be dropped.
  return std::string_view();
}

}  // namespace metrics
