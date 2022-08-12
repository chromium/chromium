// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/config.h"
#include <memory>

#include "base/strings/strcat.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

Config::SegmentMetadata::SegmentMetadata(const std::string& uma_name)
    : uma_name(uma_name) {}

Config::SegmentMetadata::SegmentMetadata(
    const std::string& uma_name,
    std::unique_ptr<ModelProvider> default_model)
    : uma_name(uma_name), default_provider(std::move(default_model)) {}

Config::SegmentMetadata::SegmentMetadata(SegmentMetadata&& other) = default;

Config::SegmentMetadata::~SegmentMetadata() = default;

bool Config::SegmentMetadata::operator==(const SegmentMetadata& other) const {
  return other.uma_name == uma_name;
}

Config::Config() = default;

Config::~Config() = default;

std::string Config::GetSegmentationFilterName() const {
  return base::StrCat({"Segmentation_", segmentation_uma_name});
}

std::string Config::GetSegmentUmaName(proto::SegmentId segment) const {
  std::string name = "Other";
  auto it = segments.find(segment);
  if (it == segments.end()) {
    return "Other";
  }
  return it->second->uma_name;
}

}  // namespace segmentation_platform
