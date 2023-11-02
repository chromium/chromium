// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_SUBTREE_CAPTURE_ID_ALLOCATOR_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_SUBTREE_CAPTURE_ID_ALLOCATOR_H_

#include <cstdint>

#include "components/viz/common/surfaces/subtree_capture_id.h"

namespace viz {

// Generates SubtreeCaptureId's by incrementally increasing the subtree_id's.
class VIZ_COMMON_EXPORT SubtreeCaptureIdAllocator {
 public:
  SubtreeCaptureIdAllocator() = default;
  SubtreeCaptureIdAllocator(const SubtreeCaptureIdAllocator&) = delete;
  SubtreeCaptureIdAllocator& operator=(const SubtreeCaptureIdAllocator&) =
      delete;
  ~SubtreeCaptureIdAllocator() = default;

  SubtreeCaptureId NextSubtreeCaptureId() {
    return SubtreeCaptureId(next_subtree_id_++);
  }

 private:
  uint32_t next_subtree_id_ = 1u;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_SUBTREE_CAPTURE_ID_ALLOCATOR_H_
