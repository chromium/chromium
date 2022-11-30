// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_surface_id_allocator.h"

#include "base/unguessable_token.h"

namespace viz {

TestSurfaceIdAllocator::TestSurfaceIdAllocator(const FrameSinkId& frame_sink_id)
    : surface_id_(frame_sink_id,
                  LocalSurfaceId(kInitialParentSequenceNumber,
                                 kInitialChildSequenceNumber,
                                 base::UnguessableToken::Create())) {}

void TestSurfaceIdAllocator::Increment() {
  LocalSurfaceId incremented(local_surface_id().parent_sequence_number() + 1,
                             local_surface_id().child_sequence_number(),
                             local_surface_id().embed_token());
  surface_id_ = SurfaceId(frame_sink_id(), incremented);
}

}  // namespace viz
