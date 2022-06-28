// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_saved_frame.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {
namespace {

constexpr gfx::Rect kQuadLayerRect{0, 0, 20, 20};
constexpr gfx::Rect kVisibleLayerRect{5, 5, 10, 10};

std::vector<CompositorFrameTransitionDirective::SharedElement>
CreateSharedElements(const std::vector<CompositorRenderPassId>& render_passes) {
  std::vector<CompositorFrameTransitionDirective::SharedElement> elements(
      render_passes.size());
  for (size_t i = 0; i < render_passes.size(); i++)
    elements[i].render_pass_id = render_passes[i];
  return elements;
}

class SurfaceSavedFrameTest : public testing::Test {
 public:
  void SetUp() override {
    constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);
    support_ = std::make_unique<CompositorFrameSinkSupport>(
        nullptr, &frame_sink_manager_, kArbitraryFrameSinkId, /*is_root=*/true);
    LocalSurfaceId local_surface_id(6, base::UnguessableToken::Create());
    surface_id_ = SurfaceId(kArbitraryFrameSinkId, local_surface_id);
  }

  void DirectiveComplete(uint32_t sequence_id) {
    last_complete_id_ = sequence_id;
    run_loop_.Quit();
  }

  std::unique_ptr<SurfaceSavedFrame> CreateSavedFrame(
      CompositorFrameTransitionDirective directive) {
    return std::make_unique<SurfaceSavedFrame>(
        directive,
        base::BindRepeating(&SurfaceSavedFrameTest::DirectiveComplete,
                            base::Unretained(this)));
  }

  Surface* GetSurface() {
    return frame_sink_manager_.surface_manager()->GetSurfaceForId(surface_id_);
  }

  void CancelAllCopyRequests() {
    for (auto& pass :
         GetSurface()->GetActiveOrInterpolatedFrame().render_pass_list) {
      pass->copy_requests.clear();
    }
    run_loop_.Run();
  }

  SharedQuadState* AddSharedQuadState(CompositorRenderPass& render_pass) {
    auto* sqs = render_pass.CreateAndAppendSharedQuadState();
    gfx::Transform quad_to_target_transform;
    sqs->SetAll(quad_to_target_transform, kQuadLayerRect, kVisibleLayerRect,
                gfx::MaskFilterInfo(), kQuadLayerRect, true, 0.9f,
                SkBlendMode::kClear, 2);
    return sqs;
  }

  CompositorRenderPassDrawQuad* AddRenderPassDrawQuad(
      CompositorRenderPass& render_pass) {
    auto* pass_quad =
        render_pass.CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
    pass_quad->SetAll(render_pass.shared_quad_state_list.front(),
                      kQuadLayerRect, kVisibleLayerRect, true,
                      CompositorRenderPassId(1), ResourceId(4),
                      gfx::RectF(0, 0, 0.4f, 0.4f), gfx::Size(20, 20),
                      gfx::Vector2dF(0.4f, 0.4f), gfx::PointF(0.2f, 0.2f),
                      gfx::RectF(0, 0, 0.4f, 0.4f), true, 0.5f, true);
    return pass_quad;
  }

  SolidColorDrawQuad* AddSolidColorDrawQuad(CompositorRenderPass& render_pass) {
    auto* solid_color_quad =
        render_pass.CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    solid_color_quad->SetAll(render_pass.shared_quad_state_list.front(),
                             kQuadLayerRect, kVisibleLayerRect, true,
                             SkColors::kBlack, true);
    return solid_color_quad;
  }

 protected:
  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl frame_sink_manager_{
      FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_)};
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  SurfaceId surface_id_;

  uint32_t last_complete_id_ = 0u;
  base::RunLoop run_loop_;
};

// No interpolated frame if there are no shared elements.
TEST_F(SurfaceSavedFrameTest, OnlyRootSnapshotNoSharedPass) {
  auto frame = CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .AddDefaultRenderPass()
                   .Build();
  support_->SubmitCompositorFrame(surface_id_.local_surface_id(),
                                  std::move(frame));

  const uint32_t sequence_id = 2u;
  CompositorFrameTransitionDirective directive(
      sequence_id, CompositorFrameTransitionDirective::Type::kSave, false,
      CompositorFrameTransitionDirective::Effect::kCoverDown);
  auto saved_frame = CreateSavedFrame(directive);
  saved_frame->RequestCopyOfOutput(GetSurface());
  EXPECT_FALSE(GetSurface()->HasInterpolatedFrame());

  CancelAllCopyRequests();
  EXPECT_EQ(last_complete_id_, sequence_id);
  EXPECT_FALSE(GetSurface()->HasInterpolatedFrame());
}

// No interpolated frame if there are no shared elements with valid render pass.
TEST_F(SurfaceSavedFrameTest, OnlyRootSnapshotNullSharedPass) {
  auto frame = CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .AddDefaultRenderPass()
                   .Build();
  support_->SubmitCompositorFrame(surface_id_.local_surface_id(),
                                  std::move(frame));

  const uint32_t sequence_id = 2u;
  CompositorFrameTransitionDirective directive(
      sequence_id, CompositorFrameTransitionDirective::Type::kSave, false,
      CompositorFrameTransitionDirective::Effect::kCoverDown,
      CompositorFrameTransitionDirective::TransitionConfig(),
      CreateSharedElements({CompositorRenderPassId(0u)}));
  auto saved_frame = CreateSavedFrame(directive);
  saved_frame->RequestCopyOfOutput(GetSurface());
  EXPECT_FALSE(GetSurface()->HasInterpolatedFrame());

  CancelAllCopyRequests();
  EXPECT_EQ(last_complete_id_, sequence_id);
  EXPECT_FALSE(GetSurface()->HasInterpolatedFrame());
}

// Remove only shared element quads from the root pass and add a copy pass.
// RP0 [] -> RP1[SolidColor, RP0]
// Shared elements : [RP0]
TEST_F(SurfaceSavedFrameTest, RemoveSharedElementQuadOnly) {
  auto frame = CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .AddDefaultRenderPass()
                   .AddDefaultRenderPass()
                   .Build();
  auto* original_root_pass = frame.render_pass_list.back().get();
  const auto shared_pass_id = frame.render_pass_list.at(0)->id;
  const auto non_shared_pass_id = frame.render_pass_list.at(1)->id;
  AddSharedQuadState(*original_root_pass);

  auto* shared_pass_quad = AddRenderPassDrawQuad(*original_root_pass);
  shared_pass_quad->render_pass_id = shared_pass_id;

  AddSolidColorDrawQuad(*original_root_pass);

  auto* non_shared_pass_quad = AddRenderPassDrawQuad(*original_root_pass);
  non_shared_pass_quad->render_pass_id = non_shared_pass_id;

  support_->SubmitCompositorFrame(surface_id_.local_surface_id(),
                                  std::move(frame));

  const uint32_t sequence_id = 2u;
  CompositorFrameTransitionDirective directive(
      sequence_id, CompositorFrameTransitionDirective::Type::kSave, false,
      CompositorFrameTransitionDirective::Effect::kCoverDown,
      CompositorFrameTransitionDirective::TransitionConfig(),
      CreateSharedElements({shared_pass_id}));
  auto saved_frame = CreateSavedFrame(directive);
  saved_frame->RequestCopyOfOutput(GetSurface());
  EXPECT_TRUE(GetSurface()->HasInterpolatedFrame());

  const auto& interpolated_frame = GetSurface()->GetActiveOrInterpolatedFrame();
  const auto& render_passes = interpolated_frame.render_pass_list;

  // 3 passes from the original frame and another pass to copy the root render
  // pass.
  ASSERT_EQ(render_passes.size(), 4u);

  // The first pass is the original shared element.
  EXPECT_EQ(render_passes[0]->id, shared_pass_id);
  EXPECT_EQ(render_passes[0]->copy_requests.size(), 1u);
  EXPECT_EQ(render_passes[0]->quad_list.size(), 0u);

  // The second pass is the non-shared element pass.
  EXPECT_EQ(render_passes[1]->id, non_shared_pass_id);
  EXPECT_EQ(render_passes[1]->copy_requests.size(), 0u);
  EXPECT_EQ(render_passes[1]->quad_list.size(), 0u);

  // The third pass is the clean root pass for copy.
  EXPECT_NE(render_passes[2]->id, original_root_pass->id);
  EXPECT_NE(render_passes[2]->id, shared_pass_id);
  EXPECT_EQ(render_passes[2]->quad_list.size(), 2u);
  EXPECT_EQ(render_passes[2]->quad_list.ElementAt(0)->material,
            DrawQuad::Material::kSolidColor);
  const auto* copied_non_shared_quad =
      CompositorRenderPassDrawQuad::MaterialCast(
          render_passes[2]->quad_list.ElementAt(1));
  EXPECT_EQ(copied_non_shared_quad->render_pass_id, non_shared_pass_id);
  EXPECT_EQ(render_passes[2]->copy_requests.size(), 1u);

  // The last pass is the original root pass.
  EXPECT_EQ(render_passes[3]->id, original_root_pass->id);
  EXPECT_EQ(render_passes[3]->quad_list.size(), 3u);
  EXPECT_EQ(render_passes[3]->copy_requests.size(), 0u);

  CancelAllCopyRequests();
  EXPECT_EQ(last_complete_id_, sequence_id);
  EXPECT_FALSE(GetSurface()->HasInterpolatedFrame());
}

// Removed shared element and tainted quads.
// RP0 [] -> RP1 [RP0] -> RP2 [RP1]
// Shared elements : [RP0]
TEST_F(SurfaceSavedFrameTest, SharedElementNestedInNonSharedElementPass) {
  auto frame = CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .AddDefaultRenderPass()
                   .AddDefaultRenderPass()
                   .Build();

  const auto shared_pass_id = frame.render_pass_list.at(0)->id;
  auto* parent_pass = frame.render_pass_list.at(1).get();
  AddSharedQuadState(*parent_pass);
  auto* parent_pass_quad = AddRenderPassDrawQuad(*parent_pass);
  parent_pass_quad->render_pass_id = shared_pass_id;

  auto* original_root_pass = frame.render_pass_list.back().get();
  AddSharedQuadState(*original_root_pass);
  auto* root_shared_pass_quad = AddRenderPassDrawQuad(*original_root_pass);
  root_shared_pass_quad->render_pass_id = parent_pass->id;

  support_->SubmitCompositorFrame(surface_id_.local_surface_id(),
                                  std::move(frame));

  const uint32_t sequence_id = 2u;
  CompositorFrameTransitionDirective directive(
      sequence_id, CompositorFrameTransitionDirective::Type::kSave, false,
      CompositorFrameTransitionDirective::Effect::kCoverDown,
      CompositorFrameTransitionDirective::TransitionConfig(),
      CreateSharedElements({shared_pass_id}));
  auto saved_frame = CreateSavedFrame(directive);
  saved_frame->RequestCopyOfOutput(GetSurface());
  EXPECT_TRUE(GetSurface()->HasInterpolatedFrame());

  const auto& interpolated_frame = GetSurface()->GetActiveOrInterpolatedFrame();
  const auto& render_passes = interpolated_frame.render_pass_list;

  // 3 passes from the original frame and 2 clean passes, one for the render
  // pass embedding the shared element and 1 additional pass for the root.
  ASSERT_EQ(render_passes.size(), 5u);

  // The first pass is the shared element.
  EXPECT_EQ(render_passes[0]->id, shared_pass_id);
  EXPECT_EQ(render_passes[0]->quad_list.size(), 0u);
  EXPECT_EQ(render_passes[0]->copy_requests.size(), 1u);

  // The second pass is the clean pass for render pass including shared element.
  EXPECT_NE(render_passes[1]->id, original_root_pass->id);
  EXPECT_NE(render_passes[1]->id, parent_pass->id);
  EXPECT_NE(render_passes[1]->id, shared_pass_id);
  EXPECT_EQ(render_passes[1]->quad_list.size(), 0u);
  EXPECT_EQ(render_passes[1]->copy_requests.size(), 0u);

  // The third pass is the original pass for render pass including shared
  // element.
  EXPECT_EQ(render_passes[2]->id, parent_pass->id);
  EXPECT_EQ(render_passes[2]->quad_list.size(), 1u);
  EXPECT_EQ(render_passes[2]->copy_requests.size(), 0u);

  // The fourth pass is the clean root pass for copy.
  EXPECT_NE(render_passes[3]->id, original_root_pass->id);
  EXPECT_NE(render_passes[3]->id, parent_pass->id);
  EXPECT_NE(render_passes[3]->id, shared_pass_id);
  EXPECT_EQ(render_passes[3]->quad_list.size(), 1u);
  EXPECT_EQ(CompositorRenderPassDrawQuad::MaterialCast(
                render_passes[3]->quad_list.front())
                ->render_pass_id,
            render_passes[1]->id);
  EXPECT_EQ(render_passes[3]->copy_requests.size(), 1u);

  // The last pass is the original root pass.
  EXPECT_EQ(render_passes[4]->id, original_root_pass->id);
  EXPECT_EQ(render_passes[4]->quad_list.size(), 1u);
  EXPECT_EQ(render_passes[4]->copy_requests.size(), 0u);

  CancelAllCopyRequests();
  EXPECT_EQ(last_complete_id_, sequence_id);
  EXPECT_FALSE(GetSurface()->HasInterpolatedFrame());
}

// Add multiple render passes for shared elements nested inside each other.
// RP0 [] -> RP1 [RP0] -> RP2 [RP1]
// Shared elements : [RP0, RP1]
TEST_F(SurfaceSavedFrameTest, SharedElementNestedInSharedElementPass) {
  auto frame = CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .AddDefaultRenderPass()
                   .AddDefaultRenderPass()
                   .Build();

  const auto child_shared_pass_id = frame.render_pass_list.at(0)->id;
  auto* parent_shared_pass = frame.render_pass_list.at(1).get();
  AddSharedQuadState(*parent_shared_pass);
  auto* parent_shared_pass_quad = AddRenderPassDrawQuad(*parent_shared_pass);
  parent_shared_pass_quad->render_pass_id = child_shared_pass_id;

  auto* original_root_pass = frame.render_pass_list.back().get();
  AddSharedQuadState(*original_root_pass);
  auto* root_shared_pass_quad = AddRenderPassDrawQuad(*original_root_pass);
  root_shared_pass_quad->render_pass_id = parent_shared_pass->id;

  support_->SubmitCompositorFrame(surface_id_.local_surface_id(),
                                  std::move(frame));

  const uint32_t sequence_id = 2u;
  CompositorFrameTransitionDirective directive(
      sequence_id, CompositorFrameTransitionDirective::Type::kSave, false,
      CompositorFrameTransitionDirective::Effect::kCoverDown,
      CompositorFrameTransitionDirective::TransitionConfig(),
      CreateSharedElements({child_shared_pass_id, parent_shared_pass->id}));
  auto saved_frame = CreateSavedFrame(directive);
  saved_frame->RequestCopyOfOutput(GetSurface());
  EXPECT_TRUE(GetSurface()->HasInterpolatedFrame());

  const auto& interpolated_frame = GetSurface()->GetActiveOrInterpolatedFrame();
  const auto& render_passes = interpolated_frame.render_pass_list;

  // 3 passes from the original frame and 2 additional passes to copy the parent
  // shared element and root pass.
  ASSERT_EQ(render_passes.size(), 5u);

  // The first pass is the child shared element.
  EXPECT_EQ(render_passes[0]->id, child_shared_pass_id);
  EXPECT_EQ(render_passes[0]->quad_list.size(), 0u);
  EXPECT_EQ(render_passes[0]->copy_requests.size(), 1u);

  // The second pass is the clean pass for parent shared element.
  EXPECT_NE(render_passes[1]->id, original_root_pass->id);
  EXPECT_NE(render_passes[1]->id, parent_shared_pass->id);
  EXPECT_NE(render_passes[1]->id, child_shared_pass_id);
  EXPECT_EQ(render_passes[1]->quad_list.size(), 0u);
  EXPECT_EQ(render_passes[1]->copy_requests.size(), 1u);

  // The third pass is the original pass for parent shared element.
  EXPECT_EQ(render_passes[2]->id, parent_shared_pass->id);
  EXPECT_EQ(render_passes[2]->quad_list.size(), 1u);
  EXPECT_EQ(render_passes[2]->copy_requests.size(), 0u);

  // The fourth pass is the clean root pass for copy.
  EXPECT_NE(render_passes[3]->id, original_root_pass->id);
  EXPECT_NE(render_passes[3]->id, parent_shared_pass->id);
  EXPECT_NE(render_passes[3]->id, child_shared_pass_id);
  EXPECT_EQ(render_passes[3]->quad_list.size(), 0u);
  EXPECT_EQ(render_passes[3]->copy_requests.size(), 1u);

  // The last pass is the original root pass.
  EXPECT_EQ(render_passes[4]->id, original_root_pass->id);
  EXPECT_EQ(render_passes[4]->quad_list.size(), 1u);
  EXPECT_EQ(render_passes[4]->copy_requests.size(), 0u);

  CancelAllCopyRequests();
  EXPECT_EQ(last_complete_id_, sequence_id);
  EXPECT_FALSE(GetSurface()->HasInterpolatedFrame());
}

}  // namespace
}  // namespace viz
