// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/config.h"
#include <memory>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace segmentation_platform {

Config::SegmentMetadata::SegmentMetadata(const std::string& uma_name)
    : uma_name(uma_name) {}

Config::SegmentMetadata::SegmentMetadata(
    const std::string& uma_name,
    std::unique_ptr<DefaultModelProvider> default_model)
    : uma_name(uma_name), default_provider(std::move(default_model)) {}

Config::SegmentMetadata::SegmentMetadata(SegmentMetadata&& other) = default;

Config::SegmentMetadata::~SegmentMetadata() = default;

bool Config::SegmentMetadata::operator==(const SegmentMetadata& other) const {
  return other.uma_name == uma_name;
}

Config::Config() = default;

Config::~Config() = default;

void Config::AddSegmentId(proto::SegmentId segment_id) {
  AddSegmentId(segment_id, nullptr);
}

void Config::AddSegmentId(
    proto::SegmentId segment_id,
    std::unique_ptr<DefaultModelProvider> default_provider) {
  auto it =
      segments.insert({segment_id, std::make_unique<SegmentMetadata>(
                                       SegmentIdToHistogramVariant(segment_id),
                                       std::move(default_provider))});
  DCHECK(it.second);
}

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
