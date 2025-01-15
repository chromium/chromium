// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_
#define COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_

#include <memory>
#include <vector>

#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/service/compositor_frame_fuzzer/compositor_frame_fuzzer.pb.h"

namespace viz {

struct FuzzedBitmap {
  FuzzedBitmap(const gfx::Size& size,
               scoped_refptr<gpu::ClientSharedImage> shared_image,
               gpu::SyncToken sync_token);
  ~FuzzedBitmap();

  FuzzedBitmap(FuzzedBitmap&& other) noexcept;
  FuzzedBitmap& operator=(FuzzedBitmap&& other) = default;

  gfx::Size size;
  scoped_refptr<gpu::ClientSharedImage> shared_image;
  gpu::SyncToken sync_token;
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
    const proto::CompositorRenderPass& render_pass_spec);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_COMPOSITOR_FRAME_FUZZER_UTIL_H_
