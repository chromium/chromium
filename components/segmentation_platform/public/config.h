// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONFIG_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONFIG_H_

#include <string>

#include "base/time/time.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace segmentation_platform {

// Contains various finch configuration params used by the segmentation
// platform.
struct Config {
  Config();
  ~Config();

  Config(const Config& other);
  Config& operator=(const Config& other);

  // The key is used to distinguish between different types of segmentation
  // usages. Currently it is mainly used by the segment selector to find the
  // discrete mapping and writing results to prefs.
  std::string segmentation_key;

  // Time to live for a segment selection. Segment selection can't be changed
  // before this duration.
  base::TimeDelta segment_selection_ttl;

  // List of segment ids that the current config requires to be available.
  std::vector<optimization_guide::proto::OptimizationTarget> segment_ids;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_CONFIG_H_
