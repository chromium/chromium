// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/segment_info_cache.h"

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

SegmentInfoCache::SegmentInfoCache() = default;

SegmentInfoCache::~SegmentInfoCache() = default;

const SegmentInfo* SegmentInfoCache::GetSegmentInfo(
    SegmentId segment_id,
    ModelSource model_source) const {
  auto it = segment_info_cache_.find(std::make_pair(segment_id, model_source));
  return (it == segment_info_cache_.end()) ? nullptr : &it->second;
}

std::unique_ptr<SegmentInfoCache::SegmentInfoList>
SegmentInfoCache::GetSegmentInfoForSegments(
    const base::flat_set<SegmentId>& segment_ids,
    ModelSource model_source) const {
  std::unique_ptr<SegmentInfoCache::SegmentInfoList> segments_found =
      std::make_unique<SegmentInfoCache::SegmentInfoList>();
  for (SegmentId target : segment_ids) {
    auto it = segment_info_cache_.find(std::make_pair(target, model_source));
    if (it != segment_info_cache_.end()) {
      segments_found->emplace_back(std::make_pair(target, &it->second));
    }
  }
  return segments_found;
}

std::unique_ptr<SegmentInfoCache::SegmentInfoList>
SegmentInfoCache::GetSegmentInfoForBothModels(
    const base::flat_set<SegmentId>& segment_ids) const {
  auto server_model_segments_found =
      GetSegmentInfoForSegments(segment_ids, ModelSource::SERVER_MODEL_SOURCE);
  auto default_model_segments_found =
      GetSegmentInfoForSegments(segment_ids, ModelSource::DEFAULT_MODEL_SOURCE);
  // Move the contents of second list into first one.
  std::move(std::begin(*default_model_segments_found),
            std::end(*default_model_segments_found),
            std::back_inserter(*server_model_segments_found));
  return server_model_segments_found;
}

void SegmentInfoCache::UpdateSegmentInfo(
    SegmentId segment_id,
    ModelSource model_source,
    std::optional<SegmentInfo> segment_info) {
  if (segment_info.has_value()) {
    segment_info->set_model_source(model_source);
    segment_info_cache_[std::make_pair(segment_id, model_source)] =
        std::move(segment_info.value());
  } else {
    auto iter =
        segment_info_cache_.find(std::make_pair(segment_id, model_source));
    if (iter == segment_info_cache_.end()) {
      return;
    }
    segment_info_cache_.erase(iter);
  }
}

}  // namespace segmentation_platform