// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/resolved_frame_data.h"

#include <memory>
#include <utility>
#include <vector>

#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_surface_id_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

TEST(ResolvedFrameDataTest, ResolvedRenderPassData) {
  constexpr gfx::Rect output_rect(100, 100);
  TestSurfaceIdAllocator surface_id(FrameSinkId(1, 1));

  ServerSharedBitmapManager shared_bitmap_manager;
  FrameSinkManagerImpl frame_sink_manager(&shared_bitmap_manager);
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &frame_sink_manager, surface_id.frame_sink_id(),
      /*is_root=*/true);

  {
    // Create a CompositorFrame and submit it to |surface_id| so there is a
    // fully populated Surface with an active CompositorFrame.
    CompositorRenderPassList render_passes;
    for (int i = 0; i < 3; ++i) {
      auto render_pass = CompositorRenderPass::Create();
      render_pass->SetNew(CompositorRenderPassId::FromUnsafeValue(i + 100),
                          output_rect, output_rect, gfx::Transform());
      render_passes.push_back(std::move(render_pass));
    }

    auto frame = CompositorFrameBuilder()
                     .SetRenderPassList(std::move(render_passes))
                     .Build();
    support->SubmitCompositorFrame(surface_id.local_surface_id(),
                                   std::move(frame));
  }

  Surface* surface =
      frame_sink_manager.surface_manager()->GetSurfaceForId(surface_id);
  EXPECT_TRUE(surface);
  EXPECT_TRUE(surface->HasActiveFrame());

  ResolvedFrameData resolved_frame(surface_id, surface);

  // The resolved frame should be false immediately.
  EXPECT_FALSE(resolved_frame.is_valid());

  std::vector<ResolvedPassData> resolved_passes;
  for (auto& render_pass : surface->GetActiveFrame().render_pass_list) {
    resolved_passes.emplace_back();
    resolved_passes.back().render_pass = render_pass.get();
  }

  // Resolved frame data should be valid after adding resolved render pass data.
  resolved_frame.UpdateResolvedPassData(std::move(resolved_passes));
  EXPECT_TRUE(resolved_frame.is_valid());

  // Looking up ResolvedPassData by CompositorRenderPassId should work.
  for (auto& render_pass : surface->GetActiveFrame().render_pass_list) {
    ResolvedPassData& resolved_pass =
        resolved_frame.GetRenderPassDataById(render_pass->id);
    EXPECT_EQ(resolved_pass.render_pass, render_pass.get());
  }

  // Check invalidation also works.
  resolved_frame.SetInvalid();
  EXPECT_FALSE(resolved_frame.is_valid());
}

TEST(ResolvedFrameDataTest, MarkAsUsed) {
  TestSurfaceIdAllocator surface_id(FrameSinkId(1, 1));
  ResolvedFrameData resolved_frame(surface_id, nullptr);

  EXPECT_TRUE(resolved_frame.MarkAsUsed());
  EXPECT_FALSE(resolved_frame.MarkAsUsed());
  EXPECT_FALSE(resolved_frame.MarkAsUsed());

  // MarkAsUsed() was called so return true.
  EXPECT_TRUE(resolved_frame.CheckIfUsedAndReset());

  // MarkAsUsed() wasn't called so return false.
  EXPECT_FALSE(resolved_frame.CheckIfUsedAndReset());

  // First usage after reset returns true then false again.
  EXPECT_TRUE(resolved_frame.MarkAsUsed());
  EXPECT_FALSE(resolved_frame.MarkAsUsed());
}

}  // namespace
}  // namespace viz
