// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/resolved_frame_data.h"

#include <memory>
#include <utility>

#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/display/display_resource_provider_software.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_surface_id_allocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

constexpr gfx::Rect kOutputRect(100, 100);

std::unique_ptr<CompositorRenderPass> BuildRenderPass(int id) {
  auto render_pass = CompositorRenderPass::Create();
  render_pass->SetNew(CompositorRenderPassId::FromUnsafeValue(id), kOutputRect,
                      kOutputRect, gfx::Transform());
  return render_pass;
}

TextureDrawQuad* AddTextureQuad(CompositorRenderPass* render_pass,
                                ResourceId resource_id) {
  auto* sqs = render_pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(gfx::Transform(), kOutputRect, kOutputRect, gfx::MaskFilterInfo(),
              absl::nullopt, /*are_contents_opaque=*/false, 1,
              SkBlendMode::kSrcOver, 0);
  auto* quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  const float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  const gfx::PointF uv_top_left(0.0f, 0.0f);
  const gfx::PointF uv_bottom_right(1.0f, 1.0f);
  quad->SetNew(sqs, kOutputRect, kOutputRect, /*needs_blending=*/false,
               resource_id, /*premultiplied_alpha=*/false, uv_top_left,
               uv_bottom_right, SK_ColorTRANSPARENT, vertex_opacity,
               /*y_flipped=*/false, /*nearest_neighbor=*/false,
               /*secure_output_only=*/false, gfx::ProtectedVideoType::kClear);

  return quad;
}

void AddRenderPassQuad(CompositorRenderPass* render_pass,
                       CompositorRenderPassId render_pass_id) {
  auto* sqs = render_pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(gfx::Transform(), kOutputRect, kOutputRect, gfx::MaskFilterInfo(),
              absl::nullopt, /*are_contents_opaque=*/false, 1,
              SkBlendMode::kSrcOver, 0);
  auto* quad =
      render_pass->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
  quad->SetNew(sqs, kOutputRect, kOutputRect, render_pass_id,
               kInvalidResourceId, gfx::RectF(), gfx::Size(), gfx::Vector2dF(),
               gfx::PointF(), gfx::RectF(),
               /*force_anti_aliasing_off=*/false,
               /*backdrop_filter_quality=*/1.0f);
}

class ResolvedFrameDataTest : public testing::Test {
 public:
  ResolvedFrameDataTest()
      : support_(std::make_unique<CompositorFrameSinkSupport>(
            nullptr,
            &frame_sink_manager_,
            surface_id_.frame_sink_id(),
            /*is_root=*/true)) {}

 protected:
  // Submits a CompositorFrame so there is a fully populated surface with an
  // active CompositorFrame. Returns the corresponding surface.
  Surface* SubmitCompositorFrame(CompositorFrame frame) {
    support_->SubmitCompositorFrame(surface_id_.local_surface_id(),
                                    std::move(frame));

    Surface* surface =
        frame_sink_manager_.surface_manager()->GetSurfaceForId(surface_id_);
    EXPECT_TRUE(surface);
    EXPECT_TRUE(surface->HasActiveFrame());
    return surface;
  }

  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl frame_sink_manager_{
      FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_)};

  TestSurfaceIdAllocator surface_id_{FrameSinkId(1, 1)};
  std::unique_ptr<CompositorFrameSinkSupport> support_;

  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher>
      child_to_parent_map_;
  AggregatedRenderPassId::Generator render_pass_id_generator_;
};

// Submits a CompositorFrame with three valid render passes then checks that
// ResolvedPassData is valid and has the correct data.
TEST_F(ResolvedFrameDataTest, UpdateActiveFrame) {
  auto frame = CompositorFrameBuilder()
                   .AddRenderPass(BuildRenderPass(101))
                   .AddRenderPass(BuildRenderPass(102))
                   .AddRenderPass(BuildRenderPass(103))
                   .Build();

  Surface* surface = SubmitCompositorFrame(std::move(frame));
  ResolvedFrameData resolved_frame(surface_id_, surface);

  // The resolved frame should be false after construction.
  EXPECT_FALSE(resolved_frame.is_valid());

  // Resolved frame data should be valid after adding resolved render pass data
  // and have three render passes.
  resolved_frame.UpdateForActiveFrame(child_to_parent_map_,
                                      render_pass_id_generator_);
  EXPECT_TRUE(resolved_frame.is_valid());
  EXPECT_THAT(resolved_frame.GetResolvedPasses(), testing::SizeIs(3));

  // Looking up ResolvedPassData by CompositorRenderPassId should work.
  for (auto& render_pass : surface->GetActiveFrame().render_pass_list) {
    const ResolvedPassData& resolved_pass =
        resolved_frame.GetRenderPassDataById(render_pass->id);
    EXPECT_EQ(&resolved_pass.render_pass(), render_pass.get());
  }

  // Check invalidation also works.
  resolved_frame.SetInvalid();
  EXPECT_FALSE(resolved_frame.is_valid());
}

// Constructs a CompositorFrame with two render passes that have the same id.
// Verifies the frame is rejected as invalid.
TEST_F(ResolvedFrameDataTest, DupliateRenderPassIds) {
  auto frame = CompositorFrameBuilder()
                   .AddRenderPass(BuildRenderPass(1))
                   .AddRenderPass(BuildRenderPass(1))
                   .Build();

  Surface* surface = SubmitCompositorFrame(std::move(frame));
  ResolvedFrameData resolved_frame(surface_id_, surface);

  resolved_frame.UpdateForActiveFrame(child_to_parent_map_,
                                      render_pass_id_generator_);
  EXPECT_FALSE(resolved_frame.is_valid());
}

// Constructs a CompositorFrame with render pass that tries to embed itself
// forming a cycle. Verifies the frame is rejected as invalid.
TEST_F(ResolvedFrameDataTest, RenderPassIdsSelfCycle) {
  // Create a CompositorFrame and submit it to |surface_id| so there is a
  // fully populated Surface with an active CompositorFrame.
  auto render_pass = BuildRenderPass(1);
  AddRenderPassQuad(render_pass.get(), render_pass->id);

  auto frame =
      CompositorFrameBuilder().AddRenderPass(std::move(render_pass)).Build();

  Surface* surface = SubmitCompositorFrame(std::move(frame));
  ResolvedFrameData resolved_frame(surface_id_, surface);

  resolved_frame.UpdateForActiveFrame(child_to_parent_map_,
                                      render_pass_id_generator_);
  EXPECT_FALSE(resolved_frame.is_valid());
}

// Constructs a CompositorFrame with two render pass that tries to embed each
// other forming a cycle. Verifies the frame is rejected as invalid.
TEST_F(ResolvedFrameDataTest, RenderPassIdsCycle) {
  // Create a CompositorFrame and submit it to |surface_id| so there is a
  // fully populated Surface with an active CompositorFrame.
  auto render_pass1 = BuildRenderPass(1);
  auto render_pass2 = BuildRenderPass(2);
  AddRenderPassQuad(render_pass1.get(), render_pass2->id);
  AddRenderPassQuad(render_pass2.get(), render_pass1->id);

  auto frame = CompositorFrameBuilder()
                   .AddRenderPass(std::move(render_pass1))
                   .AddRenderPass(std::move(render_pass2))
                   .Build();
  Surface* surface = SubmitCompositorFrame(std::move(frame));
  ResolvedFrameData resolved_frame(surface_id_, surface);

  // RenderPasses have duplicate IDs so the resolved frame should be marked as
  // invalid.
  resolved_frame.UpdateForActiveFrame(child_to_parent_map_,
                                      render_pass_id_generator_);
  EXPECT_FALSE(resolved_frame.is_valid());
}

// Check GetRectDamage() handles per quad damage correctly.
TEST_F(ResolvedFrameDataTest, RenderPassWithPerQuadDamage) {
  constexpr gfx::Rect pass_damage_rect(80, 80, 10, 10);
  constexpr gfx::Rect quad_damage_rect(10, 10, 20, 20);

  auto render_pass = BuildRenderPass(1);
  render_pass->damage_rect = pass_damage_rect;
  render_pass->has_per_quad_damage = true;

  constexpr ResourceId resource_id(1);
  TextureDrawQuad* quad = AddTextureQuad(render_pass.get(), resource_id);
  quad->damage_rect = quad_damage_rect;

  auto frame = CompositorFrameBuilder()
                   .AddRenderPass(std::move(render_pass))
                   .PopulateResources()
                   .Build();

  Surface* surface = SubmitCompositorFrame(std::move(frame));
  ResolvedFrameData resolved_frame(surface_id_, surface);

  child_to_parent_map_[resource_id] = resource_id;
  resolved_frame.UpdateForActiveFrame(child_to_parent_map_,
                                      render_pass_id_generator_);
  ASSERT_TRUE(resolved_frame.is_valid());

  // GetDamageRect() should be the union of render pass and quad damage if
  // `include_per_quad_damage` is true, otherwise just render pass damage.
  constexpr gfx::Rect full_damage_rect(10, 10, 80, 80);
  EXPECT_EQ(resolved_frame.GetDamageRect(/*include_per_quad_damage=*/true),
            full_damage_rect);
  EXPECT_EQ(resolved_frame.GetDamageRect(/*include_per_quad_damage=*/false),
            pass_damage_rect);
}

TEST_F(ResolvedFrameDataTest, MarkAsUsed) {
  ResolvedFrameData resolved_frame(surface_id_, nullptr);

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
