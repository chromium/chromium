// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_ID_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_ID_H_

#include <stdint.h>

#include <functional>
#include <limits>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/types/id_type.h"

namespace viz {

struct ResourceIdTypeMarker {};

// Note that if you need to generate new ResourceIds, please use
// ResourceIdGenerator below, since it will skip generating reserved ids.
using ResourceId = base::IdTypeU32<ResourceIdTypeMarker>;
using ResourceIdSet = base::flat_set<ResourceId>;
constexpr ResourceId kInvalidResourceId(0);
constexpr ResourceId kVizReservedRangeStartId(
    std::numeric_limits<uint32_t>::max() - 3000u);

class ResourceIdGenerator {
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

struct ResourceIdHasher {
  size_t operator()(const ResourceId& resource_id) const {
    return std::hash<uint32_t>()(resource_id.GetUnsafeValue());
  }
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_ID_H_
