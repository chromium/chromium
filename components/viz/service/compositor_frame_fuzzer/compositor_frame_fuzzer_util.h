// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_
#define COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_

#include <memory>
#include <vector>

#include "base/memory/read_only_shared_memory_region.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/service/compositor_frame_fuzzer/compositor_frame_fuzzer.pb.h"

namespace viz {

struct FuzzedBitmap {
  FuzzedBitmap(const SharedBitmapId& id,
               const gfx::Size& size,
               base::ReadOnlySharedMemoryRegion shared_region);
  ~FuzzedBitmap();

  FuzzedBitmap(FuzzedBitmap&& other) noexcept;
  FuzzedBitmap& operator=(FuzzedBitmap&& other) = default;

  SharedBitmapId id;
  gfx::Size size;
  base::ReadOnlySharedMemoryRegion shared_region;
};

struct FuzzedData {
  FuzzedData();
  ~FuzzedData();

  FuzzedData(FuzzedData&& other) noexcept;
  FuzzedData& operator=(FuzzedData&& other) = default;

  CompositorFrame frame;
  std::vector<FuzzedBitmap> allocated_bitmaps;
};

// Builds a CompositorFrame with a root RenderPass matching the protobuf
// specification in compositor_frame_fuzzer.proto, and allocates associated
// resources.
//
// May elide quads in an attempt to impose a cap on memory allocated to bitmaps
// over the frame's lifetime (from texture allocations to intermediate bitmaps
// used to draw the final frame).
//
// If necessary, performs minimal correction to ensure submission to a
// CompositorFrameSink will not cause validation errors on deserialization.
FuzzedData BuildFuzzedCompositorFrame(
    const proto::RenderPass& render_pass_spec);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_
