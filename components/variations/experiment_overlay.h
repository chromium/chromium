// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_EXPERIMENT_OVERLAY_H_
#define COMPONENTS_VARIATIONS_EXPERIMENT_OVERLAY_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/values.h"
#include "base/version_info/channel.h"

namespace variations {

// URL query parameter name for an encoded experiment overlay token.
inline constexpr char kExperimentOverlayQueryParam[] = "experiment_overlay";

// Configuration for a provisioned experiment arm, including its feature
// overrides, trial parameters, variation IDs, and optional layer target.
struct COMPONENT_EXPORT(VARIATIONS) ExperimentOverlayConfig {
  static constexpr int kCurrentPayloadVersion = 1;
  static constexpr char kDefaultSource[] = "json";
  static constexpr char kKeyVersion[] = "v";
  static constexpr char kKeySource[] = "src";
  static constexpr char kKeyStudyName[] = "s";
  static constexpr char kKeyGroupName[] = "g";
  static constexpr char kKeyEnableFeatures[] = "ef";
  static constexpr char kKeyDisableFeatures[] = "df";
  static constexpr char kKeyParams[] = "p";
  static constexpr char kKeyVariationIds[] = "vid";
  static constexpr char kKeyLayer[] = "l";
  static constexpr char kKeyLayerId[] = "id";
  static constexpr char kKeyLayerMemberId[] = "m";

  struct LayerMemberSpec {
    uint32_t layer_id = 0;
    uint32_t layer_member_id = 0;

    bool operator==(const LayerMemberSpec& other) const = default;
  };

  ExperimentOverlayConfig();
  ~ExperimentOverlayConfig();
  ExperimentOverlayConfig(const ExperimentOverlayConfig&);
  ExperimentOverlayConfig& operator=(const ExperimentOverlayConfig&);
  ExperimentOverlayConfig(ExperimentOverlayConfig&&);
  ExperimentOverlayConfig& operator=(ExperimentOverlayConfig&&);

  bool operator==(const ExperimentOverlayConfig& other) const = default;

  // Returns all feature names referenced by this configuration.
  base::flat_set<std::string> GetAllReferencedFeatures() const;

  // Serializes this configuration into a dictionary.
  base::DictValue ToDict() const;

  // Deserializes `dict` into a validated configuration, or returns
  // `std::nullopt` if `dict` is malformed.
  static std::optional<ExperimentOverlayConfig> FromDict(
      const base::DictValue& dict);

  std::string source;
  std::string study_name;
  std::string group_name;
  std::vector<std::string> enable_features;
  std::vector<std::string> disable_features;
  base::flat_map<std::string, std::string> params;
  std::vector<std::string> variation_ids;
  std::optional<LayerMemberSpec> layer;
};

// Returns whether experiment overlays are permitted on `channel`.
COMPONENT_EXPORT(VARIATIONS)
bool IsExperimentOverlayAllowed(version_info::Channel channel);

// Returns whether the experiment overlay feature is enabled.
COMPONENT_EXPORT(VARIATIONS)
bool IsExperimentOverlayFeatureEnabled();

// Parses a variation ID string (with an optional `'t'` trigger prefix) into
// a positive integer, or returns `std::nullopt` if invalid.
COMPONENT_EXPORT(VARIATIONS)
std::optional<int> ParseExperimentOverlayVariationId(std::string_view raw_id);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_EXPERIMENT_OVERLAY_H_
