// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_CONFIG_HOLDER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_CONFIG_HOLDER_H_

#include <memory>
#include <vector>
#include "base/containers/flat_set.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

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

 private:
  const std::vector<std::unique_ptr<Config>> configs_;
  const base::flat_set<proto::SegmentId> all_segment_ids_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_CONFIG_HOLDER_H_
