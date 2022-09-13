// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/segment_info_cache.h"

#include <memory>

#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {

SegmentInfoCache::SegmentInfoCache(bool cache_enabled)
    : cache_enabled_(cache_enabled) {}

SegmentInfoCache::~SegmentInfoCache() = default;

absl::optional<SegmentInfo> SegmentInfoCache::GetSegmentInfo(
    SegmentId segment_id) const {
  if (!cache_enabled_) {
    return absl::nullopt;
  }
  auto it = segment_info_cache_.find(segment_id);
  return it == segment_info_cache_.end() ? absl::nullopt : it->second;
}

std::unique_ptr<SegmentInfoCache::SegmentInfoList>
SegmentInfoCache::GetSegmentInfoForSegments(
    const base::flat_set<SegmentId>& segment_ids,
    base::flat_set<SegmentId>& ids_missing_from_cache) const {
  std::unique_ptr<SegmentInfoCache::SegmentInfoList> segments_so_far =
      std::make_unique<SegmentInfoCache::SegmentInfoList>();

  if (!cache_enabled_) {
    ids_missing_from_cache.insert(segment_ids.begin(), segment_ids.end());
    return segments_so_far;
  }

  for (SegmentId target : segment_ids) {
    absl::optional<SegmentInfo> info = GetSegmentInfo(target);
    if (info != absl::nullopt) {
      segments_so_far->emplace_back(std::make_pair(target, info.value()));
    } else {
      ids_missing_from_cache.insert(target);
    }
  }
  return segments_so_far;
}

void SegmentInfoCache::UpdateSegmentInfo(
    SegmentId segment_id,
    absl::optional<SegmentInfo> segment_info) {
  if (!cache_enabled_) {
    return;
  }
  if (segment_info.has_value()) {
    segment_info_cache_[segment_id] = std::move(segment_info);
  } else {
    segment_info_cache_.erase(segment_id);
  }
}

}  // namespace segmentation_platform