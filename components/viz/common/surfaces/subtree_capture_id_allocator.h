// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_SUBTREE_CAPTURE_ID_ALLOCATOR_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_SUBTREE_CAPTURE_ID_ALLOCATOR_H_

#include <cstdint>

#include "base/sequence_checker.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"

namespace viz {

// Generates `SubtreeCaptureId`s by incrementally increasing the
// `next_subtree_id_`. This method of ID allocation is used for
// aura::Window capture, where IDs need to be process-unique due to the
// potential for Ash layers to move between FrameSinkIds. All IDs allocated by
// this method have zero as the `high` value.
class VIZ_COMMON_EXPORT SubtreeCaptureIdAllocator {
 public:
  SubtreeCaptureIdAllocator() = default;
  SubtreeCaptureIdAllocator(const SubtreeCaptureIdAllocator&) = delete;
  SubtreeCaptureIdAllocator& operator=(const SubtreeCaptureIdAllocator&) =
      delete;
  ~SubtreeCaptureIdAllocator() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  SubtreeCaptureId NextSubtreeCaptureId() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return SubtreeCaptureId(base::Token(0u, next_subtree_id_++));
  }

 private:
  uint64_t next_subtree_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 1u;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_SUBTREE_CAPTURE_ID_ALLOCATOR_H_
