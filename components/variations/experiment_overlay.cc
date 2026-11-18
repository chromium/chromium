// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/experiment_overlay.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/variations/processed_study.h"
#include "components/variations/variations_features.h"

namespace variations {

namespace {

constexpr char kTriggerVariationIdPrefix = 't';
constexpr char kFieldTrialSeparator = '/';

// Prevents an overlay from overriding its own opt-in or kill-switch flags.
bool IsDisallowedOverlayFeatureName(std::string_view feature_name) {
  return feature_name == kEnableExperimentOverlay.name ||
         feature_name == kExperimentOverlayKillSwitch.name;
}

// Returns whether `name` is non-empty printable ASCII without '/'.
bool IsValidOverlayNameBase(std::string_view name) {
  return !name.empty() && std::ranges::all_of(name, [](char c) {
    return base::IsAsciiPrintable(c);
  }) && name.find(kFieldTrialSeparator) == std::string_view::npos;
}

bool IsValidOverlayStudyOrFeatureName(std::string_view name) {
  return IsValidOverlayNameBase(name) &&
         base::FeatureList::IsValidFeatureOrFieldTrialName(name);
}

bool IsValidOverlayGroupName(std::string_view name) {
  return IsValidOverlayNameBase(name) &&
         ProcessedStudy::IsValidExperimentName(name);
}

// Extracts an optional list of non-empty strings at `key`, returning an empty
// vector when absent or `std::nullopt` if malformed.
std::optional<std::vector<std::string>> ExtractStringList(
    const base::DictValue& dict,
    std::string_view key) {
  const base::Value* value = dict.Find(key);
  if (!value) {
    return std::vector<std::string>();
  }
  const base::ListValue* list = value->GetIfList();
  if (!list) {
    return std::nullopt;
  }
  std::vector<std::string> result;
  result.reserve(list->size());
  for (const base::Value& item : *list) {
    const std::string* str = item.GetIfString();
    if (!str || str->empty()) {
      return std::nullopt;
    }
    result.push_back(*str);
  }
  return result;
}

// Extracts unique, valid feature names at `key`, rejecting control flags and
// duplicates (duplicate overrides on one trial fail registration).
std::optional<std::vector<std::string>> ExtractFeatureNameList(
    const base::DictValue& dict,
    std::string_view key) {
  std::optional<std::vector<std::string>> features =
      ExtractStringList(dict, key);
  if (!features.has_value()) {
    return std::nullopt;
  }
  base::flat_set<std::string_view> seen_features;
  for (const std::string& feature : *features) {
    if (!IsValidOverlayStudyOrFeatureName(feature) ||
        IsDisallowedOverlayFeatureName(feature) ||
        !seen_features.insert(feature).second) {
      return std::nullopt;
    }
  }
  return features;
}

}  // namespace

ExperimentOverlayConfig::ExperimentOverlayConfig() = default;
ExperimentOverlayConfig::~ExperimentOverlayConfig() = default;
ExperimentOverlayConfig::ExperimentOverlayConfig(
    const ExperimentOverlayConfig&) = default;
ExperimentOverlayConfig& ExperimentOverlayConfig::operator=(
    const ExperimentOverlayConfig&) = default;
ExperimentOverlayConfig::ExperimentOverlayConfig(ExperimentOverlayConfig&&) =
    default;
ExperimentOverlayConfig& ExperimentOverlayConfig::operator=(
    ExperimentOverlayConfig&&) = default;

base::flat_set<std::string> ExperimentOverlayConfig::GetAllReferencedFeatures()
    const {
  base::flat_set<std::string> features;
  features.insert(enable_features.begin(), enable_features.end());
  features.insert(disable_features.begin(), disable_features.end());
  return features;
}

base::DictValue ExperimentOverlayConfig::ToDict() const {
  base::DictValue dict;
  dict.Set(kKeyVersion, kCurrentPayloadVersion);
  dict.Set(kKeySource, source.empty() ? std::string(kDefaultSource) : source);
  dict.Set(kKeyStudyName, study_name);
  dict.Set(kKeyGroupName, group_name);

  if (!enable_features.empty()) {
    base::ListValue ef_list;
    for (const std::string& feature : enable_features) {
      ef_list.Append(feature);
    }
    dict.Set(kKeyEnableFeatures, std::move(ef_list));
  }

  if (!disable_features.empty()) {
    base::ListValue df_list;
    for (const std::string& feature : disable_features) {
      df_list.Append(feature);
    }
    dict.Set(kKeyDisableFeatures, std::move(df_list));
  }

  if (!params.empty()) {
    base::DictValue params_dict;
    for (const auto& [key, val] : params) {
      params_dict.Set(key, val);
    }
    dict.Set(kKeyParams, std::move(params_dict));
  }

  if (!variation_ids.empty()) {
    base::ListValue vid_list;
    for (const std::string& vid : variation_ids) {
      vid_list.Append(vid);
    }
    dict.Set(kKeyVariationIds, std::move(vid_list));
  }

  if (layer.has_value()) {
    base::DictValue layer_dict;
    layer_dict.Set(kKeyLayerId, static_cast<int>(layer->layer_id));
    layer_dict.Set(kKeyLayerMemberId, static_cast<int>(layer->layer_member_id));
    dict.Set(kKeyLayer, std::move(layer_dict));
  }

  return dict;
}

// static
std::optional<ExperimentOverlayConfig> ExperimentOverlayConfig::FromDict(
    const base::DictValue& dict) {
  // Require an explicit version so newer schemas are not misread as v1.
  if (dict.FindInt(kKeyVersion) != kCurrentPayloadVersion) {
    return std::nullopt;
  }

  const std::string* study_name = dict.FindString(kKeyStudyName);
  const std::string* group_name = dict.FindString(kKeyGroupName);
  if (!study_name || !IsValidOverlayStudyOrFeatureName(*study_name) ||
      !group_name || !IsValidOverlayGroupName(*group_name)) {
    return std::nullopt;
  }

  std::optional<std::vector<std::string>> enable_features =
      ExtractFeatureNameList(dict, kKeyEnableFeatures);
  std::optional<std::vector<std::string>> disable_features =
      ExtractFeatureNameList(dict, kKeyDisableFeatures);
  std::optional<std::vector<std::string>> variation_ids =
      ExtractStringList(dict, kKeyVariationIds);
  if (!enable_features.has_value() || !disable_features.has_value() ||
      !variation_ids.has_value()) {
    return std::nullopt;
  }

  for (const std::string& enabled : *enable_features) {
    if (std::ranges::contains(*disable_features, enabled)) {
      return std::nullopt;
    }
  }

  std::string source = ExperimentOverlayConfig::kDefaultSource;
  if (const base::Value* source_val = dict.Find(kKeySource)) {
    const std::string* source_str = source_val->GetIfString();
    if (!source_str) {
      return std::nullopt;
    }
    if (!source_str->empty()) {
      source = *source_str;
    }
  }

  ExperimentOverlayConfig config;
  config.study_name = *study_name;
  config.group_name = *group_name;
  config.source = std::move(source);
  config.enable_features = *std::move(enable_features);
  config.disable_features = *std::move(disable_features);
  config.variation_ids = *std::move(variation_ids);

  if (const base::Value* params_val = dict.Find(kKeyParams)) {
    const base::DictValue* params_dict = params_val->GetIfDict();
    if (!params_dict) {
      return std::nullopt;
    }
    for (const auto [key, val] : *params_dict) {
      const std::string* val_str = val.GetIfString();
      if (key.empty() || !val_str) {
        return std::nullopt;
      }
      config.params[key] = *val_str;
    }
  }

  base::flat_set<int> seen_variation_ids;
  for (const std::string& raw_vid : config.variation_ids) {
    const std::optional<int> parsed_vid =
        ParseExperimentOverlayVariationId(raw_vid);
    if (!parsed_vid.has_value() ||
        !seen_variation_ids.insert(*parsed_vid).second) {
      return std::nullopt;
    }
  }

  if (const base::Value* layer_val = dict.Find(kKeyLayer)) {
    const base::DictValue* layer_dict = layer_val->GetIfDict();
    if (!layer_dict) {
      return std::nullopt;
    }
    const std::optional<int> layer_id = layer_dict->FindInt(kKeyLayerId);
    const std::optional<int> member_id = layer_dict->FindInt(kKeyLayerMemberId);
    if (!layer_id.has_value() || *layer_id <= 0 || !member_id.has_value() ||
        *member_id <= 0) {
      return std::nullopt;
    }
    config.layer = ExperimentOverlayConfig::LayerMemberSpec{
        .layer_id = static_cast<uint32_t>(*layer_id),
        .layer_member_id = static_cast<uint32_t>(*member_id),
    };
  }

  return config;
}

bool IsExperimentOverlayAllowed(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
      return true;
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE:
      return false;
  }
}

bool IsExperimentOverlayFeatureEnabled() {
  if (!base::FeatureList::GetInstance()) {
    return false;
  }
  return base::FeatureList::IsEnabled(kEnableExperimentOverlay) &&
         !base::FeatureList::IsEnabled(kExperimentOverlayKillSwitch);
}

std::optional<int> ParseExperimentOverlayVariationId(std::string_view raw_id) {
  if (raw_id.empty()) {
    return std::nullopt;
  }
  std::string_view digits = raw_id;
  if (digits.front() == kTriggerVariationIdPrefix) {
    digits.remove_prefix(1);
  }
  // Require ASCII digits so a leading '+' is rejected.
  int parsed_id = 0;
  if (digits.empty() ||
      !std::ranges::all_of(digits,
                           [](char c) { return base::IsAsciiDigit(c); }) ||
      !base::StringToInt(digits, &parsed_id) || parsed_id <= 0) {
    return std::nullopt;
  }
  return parsed_id;
}

}  // namespace variations
