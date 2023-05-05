// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/config_holder.h"
#include "components/segmentation_platform/internal/config_parser.h"

namespace segmentation_platform {

ConfigHolder::ConfigHolder(std::vector<std::unique_ptr<Config>> configs)
    : configs_(std::move(configs)),
      all_segment_ids_(GetAllSegmentIdsFromConfigs(configs_)) {}

ConfigHolder::~ConfigHolder() = default;

}  // namespace segmentation_platform
