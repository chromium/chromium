// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/frame_sink_id_allocator.h"

namespace viz {

constexpr FrameSinkId g_invalid_frame_sink_id;

// static
const FrameSinkId& FrameSinkIdAllocator::InvalidFrameSinkId() {
  return g_invalid_frame_sink_id;
}

}  // namespace viz
