// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_CACHE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_CACHE_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {

using proto::SegmentId;
using proto::SegmentInfo;

// Represents a cache layer wrapped over the DB layer that stores
// used SegmentId and SegmentInfo to be cached, inorder to decrease
// the time to read from DB in consecutive calls
class SegmentInfoCache {
 public:
  using SegmentInfoList = std::vector<std::pair<SegmentId, proto::SegmentInfo>>;

  explicit SegmentInfoCache(bool cache_enabled);
  ~SegmentInfoCache();

  // Disallow copy/assign.
  SegmentInfoCache(const SegmentInfoCache&) = delete;
  SegmentInfoCache& operator=(const SegmentInfoCache&) = delete;

  // Returns segment info for a `segment_id`, if present in cache.
  absl::optional<SegmentInfo> GetSegmentInfo(SegmentId segment_id) const;

  // Returns list of segment info for list of `segment_ids` that are
  // present in cache and adds the remaining list of `segment_ids` not
  // found in cache to `ids_missing_from_cache`.
  std::unique_ptr<SegmentInfoList> GetSegmentInfoForSegments(
      const base::flat_set<SegmentId>& segment_ids,
      base::flat_set<SegmentId>& ids_missing_from_cache) const;

  // Updates cache with `segment_info` for a `segment_id`.
  // It also deletes the entry from cache if `segment_info` is null.
  void UpdateSegmentInfo(SegmentId segment_id,
                         absl::optional<SegmentInfo> segment_info);

 private:
  base::flat_map<SegmentId, absl::optional<SegmentInfo>> segment_info_cache_;

  // Flag representing if cache is enabled or not.
  const bool cache_enabled_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SEGMENT_INFO_CACHE_H_
