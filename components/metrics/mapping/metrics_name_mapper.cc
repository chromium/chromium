// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/mapping/metrics_name_mapper.h"

#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "components/metrics/mapping/metrics_mapping_features.h"
#include "components/metrics/mapping/metrics_name_mapping.pb.h"

namespace metrics {
namespace {

// The defaults registered by `MetricsNameMapper::SetDefaults()`.
struct Defaults {
  bool registered = false;
  // Set once any mapper is constructed so a late `SetDefaults()` call cannot
  // race with lookups on background threads.
  std::atomic<bool> locked{false};
  // Built-in rules, in the same form as `MetricsNameMapper::exact_rules_`.
  base::flat_map<std::string, std::optional<std::string>> rules;
  std::vector<std::string> allowed_prefixes;
};

Defaults& GetDefaults() {
  static base::NoDestructor<Defaults> defaults;
  return *defaults;
}

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
    : exact_rules_(ParseConfig(base64_config)) {
  GetDefaults().locked.store(true, std::memory_order_release);
}

MetricsNameMapper::~MetricsNameMapper() = default;

// static
void MetricsNameMapper::SetDefaults(
    base::span<const Rule> rules,
    base::span<const std::string_view> allowed_prefixes) {
  Defaults& defaults = GetDefaults();
  // Defaults must be registered once before any mapper is constructed; a
  // late or repeated call would race with lookups or free returned names.
  CHECK(!defaults.registered);
  CHECK(!defaults.locked.load(std::memory_order_acquire));
  defaults.registered = true;

  for (const Rule& rule : rules) {
    CHECK(!rule.metric_name.empty());
    std::optional<std::string> new_name;
    if (!rule.new_metric_name.empty()) {
      new_name.emplace(rule.new_metric_name);
    }
    // Like server rules, the first rule for a metric name wins.
    defaults.rules.try_emplace(std::string(rule.metric_name),
                               std::move(new_name));
  }

  for (std::string_view prefix : allowed_prefixes) {
    // An empty prefix would keep every metric.
    CHECK(!prefix.empty());
    defaults.allowed_prefixes.emplace_back(prefix);
  }
}

// static
void MetricsNameMapper::ResetDefaultsForTesting() {
  Defaults& defaults = GetDefaults();
  defaults.registered = false;
  defaults.locked.store(false, std::memory_order_relaxed);
  defaults.rules.clear();
  defaults.allowed_prefixes.clear();
}

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
  const Defaults& defaults = GetDefaults();
  // Built-in rules come first so that no server rule can undo a built-in
  // rename.
  const auto* exact_rule = base::FindOrNull(defaults.rules, metric_name);
  if (!exact_rule) {
    exact_rule = base::FindOrNull(exact_rules_, metric_name);
  }
  if (exact_rule) {
    if (exact_rule->has_value()) {
      return exact_rule->value();
    }
    // Rule exists but has no override name; return the original name.
    return metric_name;
  }

  for (const std::string& prefix : defaults.allowed_prefixes) {
    if (metric_name.starts_with(prefix)) {
      return metric_name;
    }
  }

  // If no rule or allowed prefix matches, the metric is dropped (return empty
  // string).
  return std::string_view();
}

}  // namespace metrics
