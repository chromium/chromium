// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/config_holder.h"

#include "components/segmentation_platform/internal/config_parser.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

ConfigHolder::ConfigHolder(std::vector<std::unique_ptr<Config>> configs)
    : configs_(std::move(configs)),
      all_segment_ids_(GetAllSegmentIdsFromConfigs(configs_)) {
  for (const auto& config : configs_) {
    if (metadata_utils::ConfigUsesLegacyOutput(config.get())) {
      legacy_output_segmentation_keys_.insert(config->segmentation_key);
      for (const auto& segment_id : config->segments) {
        legacy_output_segment_ids_.insert(segment_id.first);
      }
    } else {
      // Non legacy segment IDs must have a 1:1 relationship with segmentation
      // keys. These checks ensure that.
      CHECK(config->segments.size() <= 1)
          << "segmentation_key: " << config->segmentation_key
          << " must not have multiple segments.";

      for (const auto& segment_id : config->segments) {
        CHECK(!segmentation_key_by_segment_id_.contains(segment_id.first))
            << "segment_id: " << proto::SegmentId_Name(segment_id.first)
            << " was found in two segmentation keys: "
            << segmentation_key_by_segment_id_.at(segment_id.first) << " and "
            << config->segmentation_key;

        segmentation_key_by_segment_id_.insert(
            {segment_id.first, config->segmentation_key});
      }

      non_legacy_segmentation_keys_.insert(config->segmentation_key);
    }
  }
}

ConfigHolder::~ConfigHolder() = default;

std::optional<std::string> ConfigHolder::GetKeyForSegmentId(
    proto::SegmentId segment_id) const {
  auto key_for_updated_segment =
      segmentation_key_by_segment_id_.find(segment_id);
  if (key_for_updated_segment == segmentation_key_by_segment_id_.end()) {
    return std::nullopt;
  }
  return key_for_updated_segment->second;
}

const Config* ConfigHolder::GetConfigForSegmentId(
    proto::SegmentId segment_id) const {
  for (const auto& config : configs_) {
    auto it = config->segments.find(segment_id);
    if (it != config->segments.end()) {
      return config.get();
    }
  }
  return nullptr;
}

bool ConfigHolder::IsLegacySegmentationKey(
    const std::string& segmentation_key) const {
  return legacy_output_segmentation_keys_.contains(segmentation_key);
}

Config* ConfigHolder::GetConfigForSegmentationKey(
    const std::string& segmentation_key) const {
  for (const auto& config : configs_) {
    if (config->segmentation_key == segmentation_key) {
      return config.get();
    }
  }
  return nullptr;
}

}  // namespace segmentation_platform
