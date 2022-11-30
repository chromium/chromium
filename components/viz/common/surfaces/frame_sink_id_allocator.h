// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_FRAME_SINK_ID_ALLOCATOR_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_FRAME_SINK_ID_ALLOCATOR_H_

#include "components/viz/common/surfaces/frame_sink_id.h"

#include "components/viz/common/viz_common_export.h"

namespace viz {

// This class generates FrameSinkId with a fixed client_id and an
// incrementally-increasing sink_id.
class VIZ_COMMON_EXPORT FrameSinkIdAllocator {
 public:
  constexpr explicit FrameSinkIdAllocator(uint32_t client_id)
      : client_id_(client_id), next_sink_id_(1u) {}

  FrameSinkIdAllocator(const FrameSinkIdAllocator&) = delete;
  FrameSinkIdAllocator& operator=(const FrameSinkIdAllocator&) = delete;

  FrameSinkId NextFrameSinkId() {
    return FrameSinkId(client_id_, next_sink_id_++);
  }

  static const FrameSinkId& InvalidFrameSinkId();

 private:
  const uint32_t client_id_;
  uint32_t next_sink_id_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_FRAME_SINK_ID_ALLOCATOR_H_
