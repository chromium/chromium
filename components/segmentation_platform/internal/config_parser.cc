// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/config_parser.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/segmentation_platform/public/config.h"

namespace segmentation_platform {
namespace {

constexpr char kSegmentationKey[] = "segmentation_key";
constexpr char kSegmentationUmaName[] = "segmentation_uma_name";
constexpr char kSegmentIds[] = "segments";
constexpr char kSegmentUmaName[] = "segment_uma_name";
constexpr char kSegmentSelectionTTL[] = "segment_selection_ttl_days";
constexpr char kUnknownSegmentSelectionTTL[] =
    "unknown_segment_selection_ttl_days";

}  // namespace

std::unique_ptr<Config> ParseConfigFromString(const std::string& config_str) {
  auto value_with_error =
      base::JSONReader::ReadAndReturnValueWithError(config_str);
  if (!value_with_error.has_value()) {
    VLOG(1) << "Config failed to parse: " << config_str
            << ". with error: " << value_with_error.error().message;
    return nullptr;
  }
  if (!value_with_error.value().is_dict()) {
    return nullptr;
  }
  const base::Value::Dict& config_dict = value_with_error.value().GetDict();
  const std::string* key = config_dict.FindString(kSegmentationKey);
  const std::string* uma_name = config_dict.FindString(kSegmentationUmaName);
  const base::Value::Dict* segments = config_dict.FindDict(kSegmentIds);
  const std::optional<int> selection_ttl_days =
      config_dict.FindInt(kSegmentSelectionTTL);
  const std::optional<int> unknown_selection_ttl_days =
      config_dict.FindInt(kUnknownSegmentSelectionTTL);

  if (!key || !uma_name || !segments || !selection_ttl_days) {
    return nullptr;
  }
  auto config = std::make_unique<Config>();
  config->segmentation_key = *key;
  config->segmentation_uma_name = *uma_name;
  config->segment_selection_ttl = base::Days(*selection_ttl_days);
  if (unknown_selection_ttl_days) {
    config->unknown_selection_ttl = base::Days(*unknown_selection_ttl_days);
  }

  for (const auto segment_id : *segments) {
    int segment = 0;
    if (!base::StringToInt(segment_id.first, &segment)) {
      return nullptr;
    }
    const base::Value::Dict& segment_dict = segment_id.second.GetDict();
    const std::string* segment_uma_name =
        segment_dict.FindString(kSegmentUmaName);
    if (!segment_uma_name) {
      return nullptr;
    }

    config->segments.insert(
        {static_cast<proto::SegmentId>(segment),
         std::make_unique<Config::SegmentMetadata>(*segment_uma_name)});
  }

  return config;
}

base::flat_set<proto::SegmentId> GetAllSegmentIdsFromConfigs(
    const std::vector<std::unique_ptr<Config>>& configs) {
  base::flat_set<proto::SegmentId> all_segment_ids;
  for (const auto& config : configs) {
    for (const auto& segment_id : config->segments) {
      all_segment_ids.insert(segment_id.first);
    }
  }
  return all_segment_ids;
}

}  // namespace segmentation_platform
