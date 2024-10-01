// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/resolved_frame_data.h"

#include <memory>
#include <utility>

#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/offset_tag.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/display/display_resource_provider_software.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/draw_quad_matchers.h"
#include "components/viz/test/test_surface_id_allocator.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace viz {

// Allow test access to ResolvedFrameData internals.
class ResolvedFrameDataTestHelper {
 public:
  explicit ResolvedFrameDataTestHelper(ResolvedFrameData* target)
      : target_(target) {}

  gfx::Rect GetCurrentContainingRect(const OffsetTag& tag) {
    return target_->offset_tag_data_.at(tag).current_containing_rect;
  }

 private:
  raw_ptr<ResolvedFrameData> target_;
};

namespace {

constexpr gfx::Rect kOutputRect(100, 100);
constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

SurfaceId MakeSurfaceId() {
  return SurfaceId(kArbitraryFrameSinkId,
                   LocalSurfaceId(1, 1, base::UnguessableToken::Create()));
}

OffsetTagDefinition MakeOffsetTagDefinition() {
  OffsetTagDefinition tag_def;
  tag_def.tag = OffsetTag::CreateRandom();
  tag_def.provider = SurfaceRange(MakeSurfaceId());
  tag_def.constraints = OffsetTagConstraints(-50.0f, 50.0f, -50.0f, 50.0f);

  return tag_def;
}

CompositorFrame MakeSimpleFrame(const gfx::Rect& damage_rect = kOutputRect) {
  return CompositorFrameBuilder()
      .AddRenderPass(RenderPassBuilder(kOutputRect)
                         .AddSolidColorQuad(kOutputRect, SkColors::kRed)
                         .SetDamageRect(kOutputRect))
      .Build();
}

std::unique_ptr<CompositorRenderPass> BuildRenderPass(int id) {
  auto render_pass = CompositorRenderPass::Create();
  render_pass->SetNew(CompositorRenderPassId::FromUnsafeValue(id), kOutputRect,
                      kOutputRect, gfx::Transform());
  return render_pass;
}

void AddRenderPassQuad(CompositorRenderPass* render_pass,
                       CompositorRenderPassId render_pass_id) {
  auto* sqs = render_pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(gfx::Transform(), kOutputRect, kOutputRect, gfx::MaskFilterInfo(),
              /*clip=*/std::nullopt, /*contents_opaque=*/false, 1,
              SkBlendMode::kSrcOver, /*sorting_context=*/0,
              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  auto* quad =
      render_pass->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
  quad->SetNew(sqs, kOutputRect, kOutputRect, render_pass_id,
               kInvalidResourceId, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1.0f, 1.0f), gfx::PointF(), gfx::RectF(),
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
  gpu::SharedImageManager shared_image_manager_;
  gpu::SyncPointManager sync_point_manager_;
  gpu::Scheduler gpu_scheduler_{&sync_point_manager_};

  DisplayResourceProviderSoftware resource_provider_{
      &shared_bitmap_manager_, &shared_image_manager_, &sync_point_manager_,
      &gpu_scheduler_};
  FrameSinkManagerImpl frame_sink_manager_{
      FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_)};

  TestSurfaceIdAllocator surface_id_{FrameSinkId(1, 1)};
  std::unique_ptr<CompositorFrameSinkSupport> support_;

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
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   AggregatedRenderPassId());

  // The resolved frame should be false after construction.
  EXPECT_FALSE(resolved_frame.is_valid());

  // Resolved frame data should be valid after adding resolved render pass data
  // and have three render passes.
  resolved_frame.UpdateForAggregation(render_pass_id_generator_);
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

  resolved_frame.ResetAfterAggregation();
}

// Constructs a CompositorFrame with two render passes that have the same id.
// Verifies the frame is rejected as invalid.
TEST_F(ResolvedFrameDataTest, DupliateRenderPassIds) {
  auto frame = CompositorFrameBuilder()
                   .AddRenderPass(BuildRenderPass(1))
                   .AddRenderPass(BuildRenderPass(1))
                   .Build();

  Surface* surface = SubmitCompositorFrame(std::move(frame));
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   AggregatedRenderPassId());

  resolved_frame.UpdateForAggregation(render_pass_id_generator_);
  EXPECT_FALSE(resolved_frame.is_valid());

  resolved_frame.ResetAfterAggregation();
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
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   AggregatedRenderPassId());

  resolved_frame.UpdateForAggregation(render_pass_id_generator_);
  EXPECT_FALSE(resolved_frame.is_valid());

  resolved_frame.ResetAfterAggregation();
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
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   AggregatedRenderPassId());

  // RenderPasses have duplicate IDs so the resolved frame should be marked as
  // invalid.
  resolved_frame.UpdateForAggregation(render_pass_id_generator_);
  EXPECT_FALSE(resolved_frame.is_valid());

  resolved_frame.ResetAfterAggregation();
}

// Check GetRectDamage() handles per quad damage correctly.
TEST_F(ResolvedFrameDataTest, RenderPassWithPerQuadDamage) {
  constexpr gfx::Rect pass_damage_rect(80, 80, 10, 10);
  constexpr gfx::Rect quad_damage_rect(10, 10, 20, 20);
  constexpr ResourceId resource_id(1);

  auto frame =
      CompositorFrameBuilder()
          .AddRenderPass(RenderPassBuilder(kOutputRect)
                             .SetDamageRect(pass_damage_rect)
                             .AddSolidColorQuad(kOutputRect, SkColors::kRed)
                             .AddTextureQuad(kOutputRect, resource_id)
                             .SetQuadDamageRect(quad_damage_rect))
          .PopulateResources()
          .Build();

  Surface* surface = SubmitCompositorFrame(std::move(frame));
  EXPECT_EQ(surface->GetActiveFrameIndex(), kFrameIndexStart);
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 1u,
                                   AggregatedRenderPassId());

  resolved_frame.UpdateForAggregation(render_pass_id_generator_);
  ASSERT_TRUE(resolved_frame.is_valid());

  // The damage rect should not include TextureDrawQuad's damage_rect.
  EXPECT_EQ(resolved_frame.GetSurfaceDamage(), pass_damage_rect);

  resolved_frame.ResetAfterAggregation();
}

TEST_F(ResolvedFrameDataTest, MarkAsUsed) {
  Surface* surface = SubmitCompositorFrame(MakeSimpleFrame());
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   AggregatedRenderPassId());

  EXPECT_FALSE(resolved_frame.WasUsedInAggregation());

  // First aggregation.
  resolved_frame.UpdateForAggregation(render_pass_id_generator_);
  EXPECT_TRUE(resolved_frame.WasUsedInAggregation());

  // This is the first frame this aggregation.
  EXPECT_EQ(resolved_frame.GetFrameDamageType(), FrameDamageType::kFull);

  // Reset after aggregation.
  resolved_frame.ResetAfterAggregation();
  EXPECT_FALSE(resolved_frame.WasUsedInAggregation());

  // Don't submit a new frame for the next aggregation.
  resolved_frame.UpdateForAggregation(render_pass_id_generator_);
  EXPECT_TRUE(resolved_frame.WasUsedInAggregation());
  EXPECT_EQ(resolved_frame.GetFrameDamageType(), FrameDamageType::kNone);

  resolved_frame.ResetAfterAggregation();

  // Submit a new frame for the next aggregation.
  SubmitCompositorFrame(MakeSimpleFrame());
  resolved_frame.UpdateForAggregation(render_pass_id_generator_);

  EXPECT_EQ(resolved_frame.GetFrameDamageType(), FrameDamageType::kFrame);

  resolved_frame.ResetAfterAggregation();

  // Submit two new frames before the next aggregation. The damage rect from
  // last frame aggregated
  SubmitCompositorFrame(MakeSimpleFrame());
  SubmitCompositorFrame(MakeSimpleFrame());
  resolved_frame.UpdateForAggregation(render_pass_id_generator_);
  EXPECT_EQ(resolved_frame.GetFrameDamageType(), FrameDamageType::kFull);

  resolved_frame.ResetAfterAggregation();
}

// Verifies that SetFullDamageForNextAggregation()
TEST_F(ResolvedFrameDataTest, SetFullDamageNextAggregation) {
  Surface* surface = SubmitCompositorFrame(MakeSimpleFrame());
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   AggregatedRenderPassId());

  // First aggregation to setup existing state.
  resolved_frame.UpdateForAggregation(render_pass_id_generator_);
  resolved_frame.ResetAfterAggregation();

  resolved_frame.SetFullDamageForNextAggregation();

  // Second aggregation with a new frame that has smaller damage rect.
  constexpr gfx::Rect damage_rect(10, 10);
  SubmitCompositorFrame(MakeSimpleFrame(damage_rect));
  resolved_frame.UpdateForAggregation(render_pass_id_generator_);

  // This is the next frame so normally it would use `damage_rect` for damage.
  // SetFullDamageForNextAggregation() changes that so the full output_rect is
  // damaged.
  EXPECT_EQ(resolved_frame.GetFrameDamageType(), FrameDamageType::kFull);
  EXPECT_EQ(resolved_frame.GetSurfaceDamage(), kOutputRect);

  resolved_frame.ResetAfterAggregation();
}

// Verifies that the ResolvedFrameData will reuse a provided root pass ID
TEST_F(ResolvedFrameDataTest, ReusePreviousRootPassId) {
  Surface* surface = SubmitCompositorFrame(MakeSimpleFrame());
  AggregatedRenderPassId prev_root_pass_id =
      render_pass_id_generator_.GenerateNextId();
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   prev_root_pass_id);

  resolved_frame.UpdateForAggregation(render_pass_id_generator_);

  EXPECT_EQ(resolved_frame.GetRootRenderPassData().remapped_id(),
            prev_root_pass_id);

  resolved_frame.ResetAfterAggregation();
}

// Verify OffsetTag creating a modified copy of original CompositorRenderPasses.
TEST_F(ResolvedFrameDataTest, OffsetTags) {
  auto offset_tag = OffsetTag::CreateRandom();

  OffsetTagDefinition tag_def;
  tag_def.tag = offset_tag;
  tag_def.provider = SurfaceRange(MakeSurfaceId());
  tag_def.constraints = OffsetTagConstraints(-30.0f, 30.0f, -30.0f, 30.0f);

  // Build a frame with one tagged quad and one not tagged quad.
  auto frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(kOutputRect)
                  .AddSolidColorQuad(kOutputRect, SkColors::kRed)
                  .AddSolidColorQuad(gfx::Rect(10, 10), SkColors::kBlack)
                  .SetQuadOffsetTag(offset_tag))
          .AddOffsetTagDefinition(tag_def)
          .Build();

  Surface* surface = SubmitCompositorFrame(std::move(frame));
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   AggregatedRenderPassId());

  constexpr gfx::Vector2dF first_offset(20.0f, -20.0f);
  constexpr gfx::Vector2dF second_offset(20.0f, 0);

  {
    // First aggregation.
    resolved_frame.UpdateForAggregation(render_pass_id_generator_);
    EXPECT_TRUE(resolved_frame.is_valid());

    resolved_frame.UpdateOffsetTags(
        [&first_offset](const OffsetTagDefinition&) { return first_offset; });

    ASSERT_THAT(resolved_frame.GetResolvedPasses(), testing::SizeIs(1));
    const CompositorRenderPass& resolved_render_pass =
        resolved_frame.GetResolvedPasses()[0].render_pass();

    // When SetOffsetTagValues runs it makes a copy of the render pass to update
    // quad positions.
    auto& original_render_pass = surface->GetActiveFrame().render_pass_list[0];
    EXPECT_NE(&resolved_render_pass, original_render_pass.get());

    // Verify that the tagged quad has offset applied and the non-tagged quad
    // doesn't.
    EXPECT_THAT(
        resolved_render_pass.quad_list,
        testing::ElementsAre(
            testing::AllOf(IsSolidColorQuad(SkColors::kRed),
                           HasTransform(gfx::Transform::MakeTranslation({}))),
            testing::AllOf(
                IsSolidColorQuad(SkColors::kBlack),
                HasTransform(gfx::Transform::MakeTranslation(first_offset)))));

    resolved_frame.ResetAfterAggregation();
  }

  {
    // Next aggregation with no updated CompositorFrame.
    resolved_frame.UpdateForAggregation(render_pass_id_generator_);

    // Send the same OffsetTagValues. This should reuse the
    // same modified render passes as the last aggregation.
    resolved_frame.UpdateOffsetTags(
        [&first_offset](const OffsetTagDefinition&) { return first_offset; });

    // TODO: Verify that RebuildRenderPassesForOffsetTags() wasn't called.
    resolved_frame.ResetAfterAggregation();
  }

  {
    // Next aggregation with no updated CompositorFrame.
    resolved_frame.UpdateForAggregation(render_pass_id_generator_);

    // Change the offset. This should require a new copy of the render passes.
    resolved_frame.UpdateOffsetTags(
        [&second_offset](const OffsetTagDefinition&) { return second_offset; });

    const CompositorRenderPass& resolved_render_pass =
        resolved_frame.GetResolvedPasses()[0].render_pass();

    // Verify that the tagged quad has the new offset applied.
    EXPECT_THAT(
        resolved_render_pass.quad_list,
        testing::ElementsAre(
            testing::AllOf(IsSolidColorQuad(SkColors::kRed),
                           HasTransform(gfx::Transform::MakeTranslation({}))),
            testing::AllOf(
                IsSolidColorQuad(SkColors::kBlack),
                HasTransform(gfx::Transform::MakeTranslation(second_offset)))));

    resolved_frame.ResetAfterAggregation();
  }

  {
    // Next aggregation with new CompositorFrame but same OffsetTag values.
    auto new_frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kOutputRect)
                    .AddSolidColorQuad(gfx::Rect(10, 10), SkColors::kBlack)
                    .SetQuadOffsetTag(offset_tag))
            .AddOffsetTagDefinition(tag_def)
            .Build();
    SubmitCompositorFrame(std::move(new_frame));

    resolved_frame.UpdateForAggregation(render_pass_id_generator_);
    EXPECT_TRUE(resolved_frame.is_valid());

    resolved_frame.UpdateOffsetTags(
        [&second_offset](const OffsetTagDefinition&) { return second_offset; });

    ASSERT_THAT(resolved_frame.GetResolvedPasses(), testing::SizeIs(1));
    const CompositorRenderPass& resolved_render_pass =
        resolved_frame.GetResolvedPasses()[0].render_pass();

    // Verify that a new modified copy of render passes has been made that now
    // only contains the modified quad.
    EXPECT_THAT(
        resolved_render_pass.quad_list,
        testing::ElementsAre(testing::AllOf(
            IsSolidColorQuad(SkColors::kBlack),
            HasTransform(gfx::Transform::MakeTranslation(second_offset)))));

    resolved_frame.ResetAfterAggregation();
  }
}

TEST_F(ResolvedFrameDataTest, OffsetTagValueIsClamped) {
  auto offset_tag = OffsetTag::CreateRandom();

  OffsetTagDefinition tag_def;
  tag_def.tag = offset_tag;
  tag_def.provider = SurfaceRange(MakeSurfaceId());
  tag_def.constraints = OffsetTagConstraints(-10.0f, 10.0f, -10.0f, 10.0f);

  auto frame = CompositorFrameBuilder()
                   .AddRenderPass(RenderPassBuilder(kOutputRect)
                                      .AddSolidColorQuad(gfx::Rect(10, 10),
                                                         SkColors::kBlack)
                                      .SetQuadOffsetTag(offset_tag))
                   .AddOffsetTagDefinition(tag_def)
                   .Build();

  Surface* surface = SubmitCompositorFrame(std::move(frame));
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   AggregatedRenderPassId());

  constexpr gfx::Vector2dF offset(20.0f, -20.0f);

  // This offset will be clamped to 10, -10 by constraints.
  constexpr gfx::Vector2dF clamped_offset(10.0f, -10.0f);
  EXPECT_EQ(clamped_offset, tag_def.constraints.Clamp(offset));

  resolved_frame.UpdateForAggregation(render_pass_id_generator_);
  EXPECT_TRUE(resolved_frame.is_valid());

  resolved_frame.UpdateOffsetTags(
      [&offset](const OffsetTagDefinition&) { return offset; });

  ASSERT_THAT(resolved_frame.GetResolvedPasses(), testing::SizeIs(1));
  const CompositorRenderPass& resolved_render_pass =
      resolved_frame.GetResolvedPasses()[0].render_pass();

  // Verify that the tagged quad has clamped offset applied.
  EXPECT_THAT(
      resolved_render_pass.quad_list,
      testing::ElementsAre(testing::AllOf(
          IsSolidColorQuad(SkColors::kBlack),
          HasTransform(gfx::Transform::MakeTranslation(clamped_offset)))));

  resolved_frame.ResetAfterAggregation();
}

TEST_F(ResolvedFrameDataTest, OffsetTagWithCopyRequest) {
  OffsetTagDefinition tag_def = MakeOffsetTagDefinition();

  auto frame = CompositorFrameBuilder()
                   .AddRenderPass(RenderPassBuilder(kOutputRect)
                                      .AddSolidColorQuad(gfx::Rect(10, 10),
                                                         SkColors::kBlack)
                                      .SetQuadOffsetTag(tag_def.tag)
                                      .AddStubCopyOutputRequest())
                   .AddOffsetTagDefinition(tag_def)
                   .Build();

  Surface* surface = SubmitCompositorFrame(std::move(frame));
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   AggregatedRenderPassId());

  resolved_frame.UpdateForAggregation(render_pass_id_generator_);
  EXPECT_TRUE(resolved_frame.is_valid());
  ASSERT_THAT(resolved_frame.GetResolvedPasses(), testing::SizeIs(1));

  resolved_frame.UpdateOffsetTags(
      [](const OffsetTagDefinition&) { return gfx::Vector2dF(1, 1); });

  // The original render pass that is held by the surface should still have a
  // CopyOutputRequest but the copied render pass won't have it.
  EXPECT_THAT(surface->GetActiveFrame().render_pass_list[0]->copy_requests,
              testing::SizeIs(1));
  EXPECT_THAT(resolved_frame.GetResolvedPasses()[0].render_pass().copy_requests,
              testing::IsEmpty());

  // Confirm that taking requests from surface still works.
  Surface::CopyRequestsMap copy_request_map;
  surface->TakeCopyOutputRequests(&copy_request_map);
  EXPECT_THAT(copy_request_map, testing::SizeIs(1));

  resolved_frame.ResetAfterAggregation();
}

// Verify that client provided damage is offset along with quads.
TEST_F(ResolvedFrameDataTest, OffsetTagClientDamageIsOffset) {
  // Submit the first frame without tags to introduce changes/damage against.
  Surface* surface = nullptr;
  {
    auto frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(kOutputRect)
                               .AddSolidColorQuad(kOutputRect, SkColors::kRed))
            .Build();
    surface = SubmitCompositorFrame(std::move(frame));
  }
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   AggregatedRenderPassId());

  {
    resolved_frame.UpdateForAggregation(render_pass_id_generator_);
    resolved_frame.UpdateOffsetTags(
        [](const OffsetTagDefinition&) { return gfx::Vector2dF(); });
    resolved_frame.ResetAfterAggregation();
  }

  auto tag_def = MakeOffsetTagDefinition();

  constexpr gfx::Rect quad_rect(50, 50, 10, 10);

  // The client provided damage includes the quad which had the tag added but
  // also damage outside the quad that won't intersect the offset tag containing
  // rect.
  constexpr gfx::Rect client_damage(50, 50, 20, 20);

  {
    // Build a frame with one added tagged quad and the same not tagged quad.
    auto frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(kOutputRect)
                               .SetDamageRect(client_damage)
                               .AddSolidColorQuad(kOutputRect, SkColors::kRed)
                               .AddSolidColorQuad(quad_rect, SkColors::kBlack)
                               .SetQuadOffsetTag(tag_def.tag))
            .AddOffsetTagDefinition(tag_def)
            .Build();
    SubmitCompositorFrame(std::move(frame));

    resolved_frame.UpdateForAggregation(render_pass_id_generator_);
    resolved_frame.UpdateOffsetTags(
        [](const OffsetTagDefinition&) { return gfx::Vector2dF(20, -20); });

    // Damage is the union of client provided damage 50,50, 20x20 and
    // intersection of tagged quad and client damage which is 70,30 10x10. Note
    // this isn't offsetting the full client damage since some of the damage
    // can't have come from tagged quads.
    EXPECT_EQ(resolved_frame.GetSurfaceDamage(), gfx::Rect(50, 30, 30, 40));

    resolved_frame.ResetAfterAggregation();
  }
}

// Verify damage works correctly when the offset value changes.
TEST_F(ResolvedFrameDataTest, OffsetTagOffsetValueChangedDamage) {
  auto tag_def = MakeOffsetTagDefinition();

  constexpr gfx::Rect quad_rect(50, 50, 10, 10);
  constexpr gfx::Vector2dF first_offset(20.0f, -20.0f);
  constexpr gfx::Vector2dF second_offset(20.0f, 20.0f);

  Surface* surface = nullptr;
  {
    // Build a frame with one added tagged quad and one not tagged quad to
    // submit changes against.
    auto frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(kOutputRect)
                               .SetDamageRect(quad_rect)
                               .AddSolidColorQuad(kOutputRect, SkColors::kRed)
                               .AddSolidColorQuad(quad_rect, SkColors::kBlack)
                               .SetQuadOffsetTag(tag_def.tag))
            .AddOffsetTagDefinition(tag_def)
            .Build();
    surface = SubmitCompositorFrame(std::move(frame));
  }
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   AggregatedRenderPassId());
  {
    resolved_frame.UpdateForAggregation(render_pass_id_generator_);
    resolved_frame.UpdateOffsetTags(
        [&first_offset](const OffsetTagDefinition&) { return first_offset; });

    resolved_frame.ResetAfterAggregation();
  }

  {
    // Next aggregation with no updated CompositorFrame but change the offset
    // value.
    resolved_frame.UpdateForAggregation(render_pass_id_generator_);
    resolved_frame.UpdateOffsetTags(
        [&second_offset](const OffsetTagDefinition&) { return second_offset; });

    // Damage is the intersection of last frame containing rect 70,30 10x10 and
    // this frames containing rect 70,70 10x10.
    EXPECT_EQ(resolved_frame.GetSurfaceDamage(), gfx::Rect(70, 30, 10, 50));

    resolved_frame.ResetAfterAggregation();
  }

  {
    // Submit a frame with offset tag removed from quad.
    auto frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(kOutputRect)
                               .SetDamageRect(quad_rect)
                               .AddSolidColorQuad(kOutputRect, SkColors::kRed)
                               .AddSolidColorQuad(quad_rect, SkColors::kBlack))
            .Build();
    SubmitCompositorFrame(std::move(frame));

    resolved_frame.UpdateForAggregation(render_pass_id_generator_);
    resolved_frame.UpdateOffsetTags(
        [](const OffsetTagDefinition&) { return gfx::Vector2dF(); });

    // Damage is union of client provided damage 50,50, 10x10 and last
    // frames containing rect 70,70 10x10
    EXPECT_EQ(resolved_frame.GetSurfaceDamage(), gfx::Rect(50, 50, 30, 30));

    // This should delete OffsetTagData since the tag wasn't used this frame.
    resolved_frame.ResetAfterAggregation();
  }
}

// Verify damage works correctly when an offset tag is removed from a layer.
TEST_F(ResolvedFrameDataTest, OffsetTagLayerRemovedDamage) {
  auto tag_def = MakeOffsetTagDefinition();
  auto offset_tag = tag_def.tag;

  constexpr gfx::Rect quad_rect1(10, 10, 50, 20);
  constexpr gfx::Rect quad_rect2(20, 20, 50, 20);
  constexpr gfx::Vector2dF offset(0, 50);

  // Submit the first frame with two tagged quads to use a baseline.
  Surface* surface = nullptr;
  {
    auto frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(kOutputRect)
                               .SetDamageRect(kOutputRect)
                               .AddSolidColorQuad(quad_rect1, SkColors::kRed)
                               .SetQuadOffsetTag(offset_tag)
                               .AddSolidColorQuad(quad_rect2, SkColors::kBlack)
                               .SetQuadOffsetTag(offset_tag))
            .AddOffsetTagDefinition(tag_def)
            .Build();
    surface = SubmitCompositorFrame(std::move(frame));
  }
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   AggregatedRenderPassId());

  {
    resolved_frame.UpdateForAggregation(render_pass_id_generator_);
    resolved_frame.UpdateOffsetTags(
        [&offset](const OffsetTagDefinition&) { return offset; });
    resolved_frame.ResetAfterAggregation();
  }

  {
    // Submit a second frame removing the tag from the quad at 20,20 50x20.
    auto frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(kOutputRect)
                               .SetDamageRect(quad_rect2)
                               .AddSolidColorQuad(quad_rect1, SkColors::kRed)
                               .SetQuadOffsetTag(offset_tag)
                               .AddSolidColorQuad(quad_rect2, SkColors::kBlack))
            .AddOffsetTagDefinition(tag_def)
            .Build();
    SubmitCompositorFrame(std::move(frame));

    resolved_frame.UpdateForAggregation(render_pass_id_generator_);
    EXPECT_TRUE(resolved_frame.is_valid());
    resolved_frame.UpdateOffsetTags(
        [&offset](const OffsetTagDefinition&) { return offset; });

    // The client provided damage is 20,20 50x20 which is the entire quad that
    // had the tag removed. However the tagged quad was drawn with an offset at
    // 20,70 50x20 last frame and won't be drawn there this frame. The
    // intersection aka 20,20 50x70 needs to be damaged this frame. The entire
    // previous frames tag containing rect with offset, eg. 10,10 60x30, is
    // added into the damage since viz doesn't know which quad had the tag
    // removed resulting in 10,20 60x70 as the final damage.
    EXPECT_TRUE(
        resolved_frame.GetSurfaceDamage().Contains(gfx::Rect(20, 70, 50, 20)));
    EXPECT_EQ(resolved_frame.GetSurfaceDamage(), gfx::Rect(10, 20, 60, 70));

    resolved_frame.ResetAfterAggregation();
  }
}

// Verify that containing rect is computed correctly for tagged quads that are
// in a non-root render pass with non-trivial transform between the render
// passes.
TEST_F(ResolvedFrameDataTest, OffsetTagContainingRectNonRootRenderPass) {
  auto tag_def = MakeOffsetTagDefinition();

  CompositorRenderPassId root_pass_id{1};
  CompositorRenderPassId child_pass_id{2};

  Surface* surface = nullptr;
  {
    gfx::Transform child_quad_to_child_pass =
        gfx::Transform::MakeTranslation(30, 30);

    // The child render pass is scale by half then translated 10,10.
    gfx::Transform child_pass_to_root_pass = gfx::Transform::MakeScale(0.5);
    child_pass_to_root_pass.PostTranslate(10, 10);

    // Build a frame with tagged quad in non-root render pass.
    auto frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(child_pass_id, kOutputRect)
                    .SetTransformToRootTarget(child_pass_to_root_pass)
                    .AddSolidColorQuad(gfx::Rect(10, 10), SkColors::kBlack)
                    .SetQuadToTargetTransform(child_quad_to_child_pass)
                    .SetQuadOffsetTag(tag_def.tag))
            .AddRenderPass(
                RenderPassBuilder(root_pass_id, kOutputRect)
                    .AddRenderPassQuad(kOutputRect, child_pass_id)
                    .SetQuadToTargetTransform(child_pass_to_root_pass))
            .AddOffsetTagDefinition(tag_def)
            .Build();
    surface = SubmitCompositorFrame(std::move(frame));
  }
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   AggregatedRenderPassId());
  ResolvedFrameDataTestHelper helper(&resolved_frame);

  {
    resolved_frame.UpdateForAggregation(render_pass_id_generator_);
    resolved_frame.UpdateOffsetTags(
        [](const OffsetTagDefinition&) { return gfx::Vector2dF(); });

    // The tagged quad position in the child pass is 30,30 10x10. The transform
    // from child render pass back to root render pass scales by 0.5, resulting
    // in 15,15 5x5 and then translates 10,10 for a final containing rect of
    // 25,25 5x5.
    EXPECT_EQ(helper.GetCurrentContainingRect(tag_def.tag),
              gfx::Rect(25, 25, 5, 5));

    resolved_frame.ResetAfterAggregation();
  }
}

TEST_F(ResolvedFrameDataTest, OffsetTagMaskFilterTranslated) {
  auto tag_def = MakeOffsetTagDefinition();
  auto offset_tag = tag_def.tag;

  constexpr gfx::Rect quad_rect(20, 30, 10, 10);
  constexpr gfx::Vector2dF offset(10, 10);

  gfx::LinearGradient gradient;
  gradient.AddStep(0.0f, 0);
  gradient.AddStep(1.0f, 255);
  gfx::MaskFilterInfo mask_info(gfx::RRectF(gfx::RectF(quad_rect)), gradient);

  Surface* surface = nullptr;
  {
    // The same layer introduces both offset tag and mask filter.
    auto frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(kOutputRect)
                               .SetDamageRect(kOutputRect)
                               .AddSolidColorQuad(quad_rect, SkColors::kRed)
                               .SetQuadMaskFilterInfo(mask_info)
                               .SetQuadOffsetTag(offset_tag))
            .AddOffsetTagDefinition(tag_def)
            .Build();
    surface = SubmitCompositorFrame(std::move(frame));
  }
  ResolvedFrameData resolved_frame(&resource_provider_, surface, 0u,
                                   AggregatedRenderPassId());

  {
    resolved_frame.UpdateForAggregation(render_pass_id_generator_);
    resolved_frame.UpdateOffsetTags(
        [&offset](const OffsetTagDefinition&) { return offset; });

    ASSERT_THAT(resolved_frame.GetResolvedPasses(), testing::SizeIs(1));
    auto& resolved_render_pass =
        resolved_frame.GetResolvedPasses()[0].render_pass();

    auto translated_mask_info = mask_info;
    translated_mask_info.ApplyTransform(
        gfx::Transform::MakeTranslation(offset));

    // Verify that the mask filter is translated.
    EXPECT_THAT(resolved_render_pass.quad_list,
                testing::ElementsAre(
                    testing::AllOf(IsSolidColorQuad(SkColors::kRed),
                                   HasMaskFilterInfo(translated_mask_info))));

    resolved_frame.ResetAfterAggregation();
  }
}

}  // namespace
}  // namespace viz
