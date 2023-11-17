// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_SUBTREE_CAPTURE_ID_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_SUBTREE_CAPTURE_ID_H_

#include <cstdint>
#include <string>
#include <utility>

#include "base/token.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// A SubtreeCaptureId uniquely identifies a layer subtree within a
// CompositorFrameSink, which can be captured independently from the root
// CompositorFrameSink by the FrameSinkVideoCapturer.
//
// For aura::Window capture, use the SubtreeCaptureIdAllocator to allocate a
// valid instance of this class. For Element level capture, use the base::Token
// associated with the element undergoing capture to construct this class.
class VIZ_COMMON_EXPORT SubtreeCaptureId {
 public:
  constexpr SubtreeCaptureId() = default;
  constexpr explicit SubtreeCaptureId(base::Token subtree_id)
      : subtree_id_(std::move(subtree_id)) {}
  constexpr SubtreeCaptureId(const SubtreeCaptureId&) = default;
  SubtreeCaptureId& operator=(const SubtreeCaptureId&) = default;
  ~SubtreeCaptureId() = default;

  constexpr bool is_valid() const { return !subtree_id_.is_zero(); }
  constexpr const base::Token& subtree_id() const { return subtree_id_; }

  friend std::strong_ordering operator<=>(const SubtreeCaptureId&,
                                          const SubtreeCaptureId&) = default;

  std::string ToString() const;

 private:
  base::Token subtree_id_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_SUBTREE_CAPTURE_ID_H_
