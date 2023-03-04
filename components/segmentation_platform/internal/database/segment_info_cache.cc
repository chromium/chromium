// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/segment_info_cache.h"

#include <memory>

#include "base/functional/callback.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {

SegmentInfoCache::SegmentInfoCache() = default;

SegmentInfoCache::~SegmentInfoCache() = default;

absl::optional<SegmentInfo> SegmentInfoCache::GetSegmentInfo(
    SegmentId segment_id) const {
  auto it = segment_info_cache_.find(segment_id);
  return (it == segment_info_cache_.end()) ? absl::nullopt
                                           : absl::make_optional(it->second);
}

std::unique_ptr<SegmentInfoCache::SegmentInfoList>
SegmentInfoCache::GetSegmentInfoForSegments(
    const base::flat_set<SegmentId>& segment_ids) const {
  std::unique_ptr<SegmentInfoCache::SegmentInfoList> segments_found =
      std::make_unique<SegmentInfoCache::SegmentInfoList>();
  for (SegmentId target : segment_ids) {
    absl::optional<SegmentInfo> info = GetSegmentInfo(target);
    if (info.has_value()) {
      segments_found->emplace_back(
          std::make_pair(target, std::move(info.value())));
    }
  }
  return segments_found;
}

void SegmentInfoCache::UpdateSegmentInfo(
    SegmentId segment_id,
    absl::optional<SegmentInfo> segment_info) {
  if (segment_info.has_value()) {
    segment_info_cache_[segment_id] = std::move(segment_info.value());
  } else {
    segment_info_cache_.erase(segment_info_cache_.find(segment_id));
  }
}

}  // namespace segmentation_platform