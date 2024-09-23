// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_ID_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_ID_H_

#include <stdint.h>

#include <functional>
#include <limits>
#include <map>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/types/id_type.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

struct ResourceIdTypeMarker {};

// Note that if you need to generate new ResourceIds, please use
// ResourceIdGenerator below, since it will skip generating reserved ids.
using ResourceId = base::IdTypeU32<ResourceIdTypeMarker>;
using ResourceIdSet = base::flat_set<ResourceId>;
inline constexpr ResourceId kInvalidResourceId(0);
inline constexpr uint32_t kNumReservedResourceIds = 3000u;
inline constexpr ResourceId kVizReservedRangeStartId(
    std::numeric_limits<uint32_t>::max() - kNumReservedResourceIds);

class VIZ_COMMON_EXPORT ResourceIdGenerator {
 public:
  explicit ResourceIdGenerator(uint32_t start_id)
      : start_id_(start_id), next_id_(start_id) {
    DCHECK_GT(start_id, kInvalidResourceId.GetUnsafeValue());
    DCHECK_LT(start_id, kVizReservedRangeStartId.GetUnsafeValue());
  }
  ResourceIdGenerator() = default;

  ResourceId GenerateNextId() {
    if (ResourceId(next_id_) >= kVizReservedRangeStartId)
      next_id_ = start_id_;
    ResourceId result(next_id_++);
    DCHECK_LT(result, kVizReservedRangeStartId);
    return result;
  }

  // For testing code. This returns the next value without incrementing it. Note
  // that because this is for testing, it does not consider the reserved range.
  ResourceId PeekNextValueForTesting() const { return ResourceId(next_id_); }

 private:
  uint32_t start_id_ = 1u;
  uint32_t next_id_ = 1u;
};

struct VIZ_COMMON_EXPORT ResourceIdHasher {
  size_t operator()(const ResourceId& resource_id) const {
    return std::hash<uint32_t>()(resource_id.GetUnsafeValue());
  }
};

// ReservedResourceIdTracker is used for keeping track of refcounts on viz
// reserved resources and allocating unused resource IDs. Since the resource ID
// space for reserved resources is much smaller than the client resource ID
// space, we keep track of which resource IDs are used individually, so we can
// allocate resource IDs that are no longer used and avoid reusing IDs that are
// still in use.
class VIZ_COMMON_EXPORT ReservedResourceIdTracker {
 public:
  ReservedResourceIdTracker();
  ~ReservedResourceIdTracker();

  [[nodiscard]] ResourceId AllocId(int initial_ref_count);
  void RefId(ResourceId id, int count);

  // Unrefs the given `id`. Returns true if there are no more refs to this
  // resource id.
  bool UnrefId(ResourceId id, int count);

  void set_next_id_for_test(uint32_t next_id) { next_id_ = next_id; }

 private:
  uint32_t next_id_ = kVizReservedRangeStartId.GetUnsafeValue();
  std::map<ResourceId, int> id_ref_counts_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_ID_H_
