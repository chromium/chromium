// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_CONFIG_HOLDER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_CONFIG_HOLDER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

// Holds all Configs active in the service.
class ConfigHolder {
 public:
  explicit ConfigHolder(std::vector<std::unique_ptr<Config>> configs);
  ~ConfigHolder();

  ConfigHolder(const ConfigHolder&) = delete;
  ConfigHolder& operator=(const ConfigHolder&) = delete;

  const std::vector<std::unique_ptr<Config>>& configs() const {
    return configs_;
  }

  const base::flat_set<proto::SegmentId>& all_segment_ids() const {
    return all_segment_ids_;
  }

  const std::set<std::string> non_legacy_segmentation_keys() const {
    return non_legacy_segmentation_keys_;
  }

  const base::flat_set<proto::SegmentId>& legacy_output_segment_ids() const {
    return legacy_output_segment_ids_;
  }

  // Returns segmentation key for the given `segment_id`. Only if the Config
  // supports output configs in metadata.
  std::optional<std::string> GetKeyForSegmentId(
      proto::SegmentId segment_id) const;

  // Returns config for the given `segment id`.
  const Config* GetConfigForSegmentId(proto::SegmentId segment_id) const;

  // Returns true if the Config is legacy, does not support output config and
  // uses discrete mapping.
  bool IsLegacySegmentationKey(const std::string& segmentation_key) const;

  // Returns the config for provided segmentation key, null if not found.
  Config* GetConfigForSegmentationKey(
      const std::string& segmentation_key) const;

 private:
  // All the active Config(s) in the service.
  const std::vector<std::unique_ptr<Config>> configs_;

  // All segment IDs needed for all active Config(s).
  const base::flat_set<proto::SegmentId> all_segment_ids_;

  // All segment IDs that doesn't support output config in metadata.
  base::flat_set<proto::SegmentId> legacy_output_segment_ids_;

  // List of segmentation keys with exactly one segment ID and supports output
  // config.
  std::set<std::string> non_legacy_segmentation_keys_;

  // List of segmentation keys for Config(s) that doesn't support output configs
  // in metadata.
  std::set<std::string> legacy_output_segmentation_keys_;

  // Map from segment ID to the segmentation key that makes use of it.
  std::map<proto::SegmentId, std::string> segmentation_key_by_segment_id_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_CONFIG_HOLDER_H_
