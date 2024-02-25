// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_CACHE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_CACHE_H_

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

using proto::ModelSource;
using proto::SegmentId;
using proto::SegmentInfo;

// Represents a cache layer wrapped over the DB layer that stores
// used SegmentId and SegmentInfo to be cached, inorder to decrease
// the time to read from DB in consecutive calls
class SegmentInfoCache {
 public:
  using SegmentInfoList =
      std::vector<std::pair<SegmentId, const proto::SegmentInfo*>>;

  SegmentInfoCache();
  ~SegmentInfoCache();

  // Disallow copy/assign.
  SegmentInfoCache(const SegmentInfoCache&) = delete;
  SegmentInfoCache& operator=(const SegmentInfoCache&) = delete;

  // Returns an optional SegmentInfo for a `segment_id` based on `model_source`.
  const SegmentInfo* GetSegmentInfo(SegmentId segment_id,
                                    ModelSource model_source) const;

  // Returns list of segment info for list of `segment_ids` found in the cache
  // based on `model_source`. If segment info is not found for a segment id,
  // nothing is returned for it.
  std::unique_ptr<SegmentInfoList> GetSegmentInfoForSegments(
      const base::flat_set<SegmentId>& segment_ids,
      ModelSource model_source) const;

  // Returns list of segment info for list of `segment_ids` found in the cache
  // for both server and default model segments. If segment info is not found
  // for a segment id, nothing is returned for it.
  std::unique_ptr<SegmentInfoList> GetSegmentInfoForBothModels(
      const base::flat_set<SegmentId>& segment_ids) const;

  // Updates cache with `segment_info` for a `segment_id` based on
  // `model_source`. It deletes the entry in cache if `segment_info` is nullopt.
  void UpdateSegmentInfo(SegmentId segment_id,
                         ModelSource model_source,
                         std::optional<SegmentInfo> segment_info);

 private:
  // Map storing SegmentInfo for a SegmentId and ModelSource.
  base::flat_map<std::pair<SegmentId, ModelSource>, SegmentInfo>
      segment_info_cache_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_CACHE_H_
