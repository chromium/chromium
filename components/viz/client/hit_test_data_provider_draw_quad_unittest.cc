// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/hit_test_data_provider_draw_quad.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

namespace {

SurfaceId CreateChildSurfaceId(uint32_t id) {
  LocalSurfaceId child_local_surface_id(id, base::UnguessableToken::Create());
  FrameSinkId frame_sink_id(id, 0);
  SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  return child_surface_id;
}

std::unique_ptr<RenderPass> CreateRenderPassWithChildSurface(
    RenderPassId render_pass_id,
    const SurfaceId& child_surface_id,
    const gfx::Rect& rect,
    const gfx::Rect& child_rect,
    const gfx::Transform& render_pass_transform,
    const gfx::Transform& shared_state_transform,
    const base::Optional<SurfaceId>& fallback_child_surface_id =
        base::nullopt) {
  auto pass = RenderPass::Create();
  pass->SetNew(render_pass_id, rect, rect, render_pass_transform);

  auto* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(shared_state_transform, rect, rect, gfx::RRectF(), rect,
                       false, false, 1, SkBlendMode::kSrcOver, 0);

  auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad->SetNew(
      pass->shared_quad_state_list.back(), child_rect, child_rect,
      SurfaceRange(fallback_child_surface_id, child_surface_id), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false, /*ignores_input_event=*/false);

  return pass;
}

}  // namespace

// Test to ensure that hit test data is created correctly from CompositorFrame
// and its RenderPassList. kHitTestAsk is only set for OOPIFs.
TEST(HitTestDataProviderDrawQuad, HitTestDataRenderer) {
  std::unique_ptr<HitTestDataProvider> hit_test_data_provider =
      std::make_unique<HitTestDataProviderDrawQuad>(
          true /* should_ask_for_child_region */,
          true /* root_accepts_events */);

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  // Ensure that a CompositorFrame without a child surface sets kHitTestMine.
  CompositorFrame compositor_frame =
      CompositorFrameBuilder().AddRenderPass(kFrameRect, kFrameRect).Build();
  base::Optional<HitTestRegionList> hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestMine,
            hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_FALSE(hit_test_region_list->regions.size());

  // Ensure that a CompositorFrame with a child surface only set kHitTestAsk
  // for its child surface.
  SurfaceId child_surface_id = CreateChildSurfaceId(2);
  gfx::Rect child_rect(200, 100);
  gfx::Transform render_pass_transform;
  render_pass_transform.Translate(-50, -100);
  render_pass_transform.Skew(2, 3);
  gfx::Transform shared_state_transform;
  shared_state_transform.Translate(-200, -100);
  auto pass = CreateRenderPassWithChildSurface(
      1, child_surface_id, kFrameRect, child_rect, render_pass_transform,
      shared_state_transform);
  compositor_frame =
      CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
  hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestMine,
            hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_EQ(1u, hit_test_region_list->regions.size());
  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestAsk,
            hit_test_region_list->regions[0].flags);
  EXPECT_EQ(child_rect, hit_test_region_list->regions[0].rect);
  gfx::Transform render_pass_transform_inverse;
  EXPECT_TRUE(render_pass_transform.GetInverse(&render_pass_transform_inverse));
  gfx::Transform shared_state_transform_inverse;
  EXPECT_TRUE(
      shared_state_transform.GetInverse(&shared_state_transform_inverse));
  EXPECT_EQ(shared_state_transform_inverse * render_pass_transform_inverse,
            hit_test_region_list->regions[0].transform);
}

// Test to ensure that we skip regions with non-invertible transforms when
// preparing for hit-test data.
TEST(HitTestDataProviderDrawQuad, HitTestDataSkipQuads) {
  std::unique_ptr<HitTestDataProvider> hit_test_data_provider =
      std::make_unique<HitTestDataProviderDrawQuad>(
          true /* should_ask_for_child_region */,
          true /* root_accepts_events */);

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);
  gfx::Rect child_rect(200, 100);

  // A degenerate matrix of all zeros is not invertible.
  gfx::Transform not_invertible_transform;
  not_invertible_transform.matrix().set(0, 0, 0.f);
  not_invertible_transform.matrix().set(1, 1, 0.f);
  not_invertible_transform.matrix().set(2, 2, 0.f);
  not_invertible_transform.matrix().set(3, 3, 0.f);

  gfx::Transform invertible_transform;
  invertible_transform.Translate(-200, -100);

  RenderPassList pass_list;

  // A render pass that has non-invertible transform.
  SurfaceId child_surface_id1 = CreateChildSurfaceId(2);
  auto pass1 = CreateRenderPassWithChildSurface(
      1, child_surface_id1, kFrameRect, child_rect, not_invertible_transform,
      invertible_transform);
  pass_list.push_back(std::move(pass1));

  // A render pass with a draw quad that has non-invertible transform.
  SurfaceId child_surface_id2 = CreateChildSurfaceId(3);
  auto pass2 = CreateRenderPassWithChildSurface(
      2, child_surface_id2, kFrameRect, child_rect, invertible_transform,
      not_invertible_transform);
  pass_list.push_back(std::move(pass2));

  // A render pass and its draw quad both have invertible transforms.
  SurfaceId child_surface_id3 = CreateChildSurfaceId(4);
  auto pass3 = CreateRenderPassWithChildSurface(
      3, child_surface_id3, kFrameRect, child_rect, invertible_transform,
      invertible_transform);
  pass_list.push_back(std::move(pass3));

  auto pass4_root = RenderPass::Create();
  pass4_root->output_rect = kFrameRect;
  pass4_root->id = 5;
  auto* shared_quad_state4_root = pass4_root->CreateAndAppendSharedQuadState();
  gfx::Rect rect4_root(kFrameRect);
  shared_quad_state4_root->SetAll(
      gfx::Transform(), /*quad_layer_rect=*/rect4_root,
      /*visible_quad_layer_rect=*/rect4_root,
      /*rounded_corner_bounds=*/gfx::RRectF(), /*clip_rect=*/rect4_root,
      /*is_clipped=*/false, /*are_contents_opaque=*/false,
      /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
  auto* quad4_root_1 =
      pass4_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
  quad4_root_1->SetNew(shared_quad_state4_root, /*rect=*/rect4_root,
                       /*visible_rect=*/rect4_root, /*render_pass_id=*/1,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  auto* quad4_root_2 =
      pass4_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
  quad4_root_2->SetNew(shared_quad_state4_root, /*rect=*/rect4_root,
                       /*visible_rect=*/rect4_root, /*render_pass_id=*/2,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  auto* quad4_root_3 =
      pass4_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
  quad4_root_3->SetNew(shared_quad_state4_root, /*rect=*/rect4_root,
                       /*visible_rect=*/rect4_root, /*render_pass_id=*/3,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  auto* quad4_root_4 =
      pass4_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
  quad4_root_4->SetNew(shared_quad_state4_root, /*rect=*/rect4_root,
                       /*visible_rect=*/rect4_root, /*render_pass_id=*/4,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  pass_list.push_back(std::move(pass4_root));

  auto compositor_frame =
      CompositorFrameBuilder().SetRenderPassList(std::move(pass_list)).Build();
  base::Optional<HitTestRegionList> hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  // Only pass3 should have a hit-test region that corresponds to
  // child_surface_id3.
  EXPECT_EQ(1u, hit_test_region_list->regions.size());
  EXPECT_EQ(child_surface_id3.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
}

// Test to ensure that browser shouldn't set kHitTestAsk flag for the renderers
// it embeds.
TEST(HitTestDataProviderDrawQuad, HitTestDataBrowser) {
  std::unique_ptr<HitTestDataProvider> hit_test_data_provider =
      std::make_unique<HitTestDataProviderDrawQuad>(
          false /* should_ask_for_child_region */,
          true /* root_accepts_events */);

  constexpr gfx::Rect frame_rect(1024, 768);
  SurfaceId child_surface_id = CreateChildSurfaceId(2);
  constexpr gfx::Rect child_rect(200, 100);
  gfx::Transform render_to_browser_transform;
  render_to_browser_transform.Translate(-200, -100);
  auto pass = CreateRenderPassWithChildSurface(1, child_surface_id, frame_rect,
                                               child_rect, gfx::Transform(),
                                               render_to_browser_transform);
  CompositorFrame compositor_frame =
      CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
  base::Optional<HitTestRegionList> hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  // Browser should be able to receive both mouse and touch events. It embeds
  // one renderer, which should also have mouse and touch flags; the renderer
  // should have child surface flag because it is being embeded, but not ask
  // flag because we shouldn't do asyn targeting for the entire renderer.
  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestMine,
            hit_test_region_list->flags);
  EXPECT_EQ(frame_rect, hit_test_region_list->bounds);
  EXPECT_EQ(1u, hit_test_region_list->regions.size());
  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestChildSurface,
            hit_test_region_list->regions[0].flags);
  EXPECT_EQ(child_rect, hit_test_region_list->regions[0].rect);
  gfx::Transform browser_to_render_transform;
  EXPECT_TRUE(
      render_to_browser_transform.GetInverse(&browser_to_render_transform));
  EXPECT_EQ(browser_to_render_transform,
            hit_test_region_list->regions[0].transform);
}

// Test to ensure that we should set kHitTestIgnore flag for transparent
// windows.
TEST(HitTestDataProviderDrawQuad, HitTestDataTransparent) {
  std::unique_ptr<HitTestDataProvider> hit_test_data_provider =
      std::make_unique<HitTestDataProviderDrawQuad>(
          true /* should_ask_for_child_region */,
          false /* root_accepts_events */);

  constexpr gfx::Rect frame_rect(1024, 768);
  SurfaceId child_surface_id = CreateChildSurfaceId(2);
  constexpr gfx::Rect child_rect(200, 100);
  gfx::Transform child_to_parent_transform;
  child_to_parent_transform.Translate(-200, -100);
  auto pass = CreateRenderPassWithChildSurface(1, child_surface_id, frame_rect,
                                               child_rect, gfx::Transform(),
                                               child_to_parent_transform);
  CompositorFrame compositor_frame =
      CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
  base::Optional<HitTestRegionList> hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestIgnore,
            hit_test_region_list->flags);
  EXPECT_EQ(frame_rect, hit_test_region_list->bounds);
  EXPECT_EQ(1u, hit_test_region_list->regions.size());
  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestAsk,
            hit_test_region_list->regions[0].flags);
  EXPECT_EQ(child_rect, hit_test_region_list->regions[0].rect);
  gfx::Transform parent_to_child_transform;
  EXPECT_TRUE(child_to_parent_transform.GetInverse(&parent_to_child_transform));
  EXPECT_EQ(parent_to_child_transform,
            hit_test_region_list->regions[0].transform);
}

// Test to ensure that we account for shape-rects in hit-test data when it's
// present in the filters of the RenderPass.
TEST(HitTestDataProviderDrawQuad, HitTestDataShapeFilters) {
  std::unique_ptr<HitTestDataProvider> hit_test_data_provider =
      std::make_unique<HitTestDataProviderDrawQuad>(
          true /* should_ask_for_child_region */,
          true /* root_accepts_events */);

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);
  gfx::Rect child_rect(200, 100);
  gfx::Transform invertible_transform;
  invertible_transform.Translate(200, 100);

  RenderPassList pass_list;

  // A render pass that has shape filters.
  SurfaceId child_surface_id1 = CreateChildSurfaceId(2);
  SurfaceId child_surface_id2 = CreateChildSurfaceId(3);

  auto pass1 = RenderPass::Create();
  pass1->SetNew(1, kFrameRect, kFrameRect, invertible_transform);
  // Create a filter with three shapes, the first two are included in
  // surface_quad_1 and the other one intersects sueface_quad_2. These rects are
  // in DIP space.
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateAlphaThresholdFilter(
      {gfx::Rect(101, 51, 25, 25), gfx::Rect(151, 51, 25, 25),
       gfx::Rect(325, 200, 200, 200)},
      0.f, 0.f));
  pass1->filters = filters;
  auto* shared_state_1 = pass1->CreateAndAppendSharedQuadState();
  shared_state_1->SetAll(invertible_transform, kFrameRect, kFrameRect,
                         gfx::RRectF(), kFrameRect, false, false, 1,
                         SkBlendMode::kSrcOver, 0);

  auto* surface_quad_1 = pass1->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad_1->SetNew(
      pass1->shared_quad_state_list.back(), child_rect, child_rect,
      SurfaceRange(child_surface_id1), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false, /*ignores_input_event=*/false);

  gfx::Rect child_rect2(400, 400, 100, 100);
  auto* surface_quad_2 = pass1->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad_2->SetNew(
      pass1->shared_quad_state_list.back(), child_rect2, child_rect2,
      SurfaceRange(child_surface_id2), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false, /*ignores_input_event=*/false);

  pass_list.push_back(std::move(pass1));

  auto compositor_frame = CompositorFrameBuilder()
                              .SetRenderPassList(std::move(pass_list))
                              .SetDeviceScaleFactor(2.f)
                              .Build();
  base::Optional<HitTestRegionList> hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  // Expect three hit-test regions, two from the first draw-quad and the other
  // from the second draw-quad.
  EXPECT_EQ(3u, hit_test_region_list->regions.size());
  EXPECT_EQ(child_surface_id1.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  EXPECT_EQ(gfx::Rect(2, 2, 50, 50), hit_test_region_list->regions[0].rect);
  EXPECT_EQ(child_surface_id1.frame_sink_id(),
            hit_test_region_list->regions[1].frame_sink_id);
  EXPECT_EQ(gfx::Rect(102, 2, 50, 50), hit_test_region_list->regions[1].rect);
  EXPECT_EQ(child_surface_id2.frame_sink_id(),
            hit_test_region_list->regions[2].frame_sink_id);
  EXPECT_EQ(gfx::Rect(450, 400, 50, 100),
            hit_test_region_list->regions[2].rect);

  // Build another CompositorFrame with device-scale-factor=0.5f.
  auto pass2 = RenderPass::Create();
  pass2->SetNew(2, kFrameRect, kFrameRect, invertible_transform);
  // Create a filter with a shapes included in surface_quad_3, in DIP space.
  cc::FilterOperations filters2;
  filters2.Append(cc::FilterOperation::CreateAlphaThresholdFilter(
      {gfx::Rect(600, 200, 300, 300)}, 0.f, 0.f));
  pass2->filters = filters2;
  auto* shared_state_2 = pass2->CreateAndAppendSharedQuadState();
  shared_state_2->SetAll(invertible_transform, kFrameRect, kFrameRect,
                         gfx::RRectF(), kFrameRect, false, false, 1,
                         SkBlendMode::kSrcOver, 0);

  auto* surface_quad_3 = pass2->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad_3->SetNew(
      pass2->shared_quad_state_list.back(), child_rect, child_rect,
      SurfaceRange(child_surface_id1), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false, /*ignores_input_event=*/false);

  pass_list.push_back(std::move(pass2));

  auto compositor_frame_2 = CompositorFrameBuilder()
                                .SetRenderPassList(std::move(pass_list))
                                .SetDeviceScaleFactor(.5f)
                                .Build();
  base::Optional<HitTestRegionList> hit_test_region_list_2 =
      hit_test_data_provider->GetHitTestData(compositor_frame_2);

  // Expect one region included in surface_quad_3.
  EXPECT_EQ(1u, hit_test_region_list_2->regions.size());
  EXPECT_EQ(child_surface_id1.frame_sink_id(),
            hit_test_region_list_2->regions[0].frame_sink_id);
  EXPECT_EQ(gfx::Rect(100, 0, 100, 100),
            hit_test_region_list_2->regions[0].rect);
}

// Test to ensure that render_pass_list caching works correctly.
TEST(HitTestDataProviderDrawQuad, HitTestDataRenderPassListCache) {
  std::unique_ptr<HitTestDataProvider> hit_test_data_provider =
      std::make_unique<HitTestDataProviderDrawQuad>(
          true /* should_ask_for_child_region */,
          true /* root_accepts_events */);

  constexpr gfx::Rect frame_rect(1024, 768);
  constexpr gfx::Rect child_rect(200, 100);
  gfx::Transform invertible_transform;
  invertible_transform.Translate(-200, -100);

  RenderPassList pass_list;

  // A RenderPass that has two SurfaceDrawQuad.
  SurfaceId child_surface_id1 = CreateChildSurfaceId(2);
  SurfaceId child_surface_id2 = CreateChildSurfaceId(3);
  auto pass1 = RenderPass::Create();
  pass1->SetNew(1, frame_rect, frame_rect, invertible_transform);
  auto* shared_state_1 = pass1->CreateAndAppendSharedQuadState();
  shared_state_1->SetAll(invertible_transform, frame_rect, frame_rect,
                         gfx::RRectF(), frame_rect, false, false, 1,
                         SkBlendMode::kSrcOver, 0);
  auto* surface_quad_1 = pass1->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad_1->SetNew(
      pass1->shared_quad_state_list.back(), child_rect, child_rect,
      SurfaceRange(child_surface_id1), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false, /*ignores_input_event=*/false);
  gfx::Rect child_rect2(400, 400, 100, 100);
  auto* surface_quad_2 = pass1->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad_2->SetNew(
      pass1->shared_quad_state_list.back(), child_rect2, child_rect2,
      SurfaceRange(child_surface_id2), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false, /*ignores_input_event=*/false);
  pass_list.push_back(std::move(pass1));

  // A RenderPass that has a RenderPassDrawQuad pointing to pass1, a
  // SurfaceDrawQuad and a RenderPassDrawQuad pointing to pass1.
  auto pass2 = RenderPass::Create();
  pass2->SetNew(4, frame_rect, frame_rect, invertible_transform);
  auto* shared_state_2 = pass2->CreateAndAppendSharedQuadState();
  shared_state_2->SetAll(invertible_transform, frame_rect, frame_rect,
                         gfx::RRectF(), frame_rect, false, false, 1,
                         SkBlendMode::kSrcOver, 0);
  auto* render_pass_quad_1 =
      pass2->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  render_pass_quad_1->SetNew(
      pass2->shared_quad_state_list.back(), child_rect, child_rect,
      /*render_pass_id=*/1, /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
      gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
  SurfaceId child_surface_id3 = CreateChildSurfaceId(4);
  gfx::Rect child_rect3(500, 500, 100, 100);
  auto* surface_quad_3 = pass2->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad_3->SetNew(
      pass2->shared_quad_state_list.back(), child_rect3, child_rect3,
      SurfaceRange(child_surface_id3), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false, /*ignores_input_event=*/false);
  auto* render_pass_quad_2 =
      pass2->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  render_pass_quad_2->SetNew(
      pass2->shared_quad_state_list.back(), child_rect2, child_rect2,
      /*render_pass_id=*/1, /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
      gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
  pass_list.push_back(std::move(pass2));

  // The root RenderPass that has three RenderPassDrawQuad point to pass2.
  auto pass_root = RenderPass::Create();
  pass_root->SetNew(5, frame_rect, frame_rect, invertible_transform);
  auto* shared_state_3 = pass_root->CreateAndAppendSharedQuadState();
  shared_state_3->SetAll(invertible_transform, frame_rect, frame_rect,
                         gfx::RRectF(), frame_rect, false, false, 1,
                         SkBlendMode::kSrcOver, 0);
  auto* render_pass_quad_3 =
      pass_root->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  render_pass_quad_3->SetNew(
      pass_root->shared_quad_state_list.back(), child_rect, child_rect,
      /*render_pass_id=*/4, /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
      gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
  auto* render_pass_quad_4 =
      pass_root->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  render_pass_quad_4->SetNew(
      pass_root->shared_quad_state_list.back(), child_rect2, child_rect2,
      /*render_pass_id=*/4, /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
      gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
  auto* render_pass_quad_5 =
      pass_root->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  render_pass_quad_5->SetNew(
      pass_root->shared_quad_state_list.back(), child_rect, child_rect,
      /*render_pass_id=*/4, /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
      gfx::Vector2dF(1, 10), gfx::PointF(), gfx::RectF(), false, 1.0f);
  pass_list.push_back(std::move(pass_root));

  CompositorFrame compositor_frame =
      CompositorFrameBuilder().SetRenderPassList(std::move(pass_list)).Build();

  base::Optional<HitTestRegionList> hit_test_region_list =
      hit_test_data_provider->GetHitTestData(compositor_frame);

  // We should have the three SurfaceDrawQuad associated HitTestRegion, repeated
  // based on RenderPassDrawQuad.
  EXPECT_EQ(15u, hit_test_region_list->regions.size());
  for (size_t i = 0; i < 15; i += 5) {
    EXPECT_EQ(child_surface_id1.frame_sink_id(),
              hit_test_region_list->regions[i].frame_sink_id);
    EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                  HitTestRegionFlags::kHitTestTouch |
                  HitTestRegionFlags::kHitTestChildSurface |
                  HitTestRegionFlags::kHitTestAsk,
              hit_test_region_list->regions[i].flags);
    EXPECT_EQ(child_rect, hit_test_region_list->regions[i].rect);

    EXPECT_EQ(child_surface_id2.frame_sink_id(),
              hit_test_region_list->regions[i + 1].frame_sink_id);
    EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                  HitTestRegionFlags::kHitTestTouch |
                  HitTestRegionFlags::kHitTestChildSurface |
                  HitTestRegionFlags::kHitTestAsk,
              hit_test_region_list->regions[i + 1].flags);
    EXPECT_EQ(child_rect2, hit_test_region_list->regions[i + 1].rect);

    EXPECT_EQ(child_surface_id3.frame_sink_id(),
              hit_test_region_list->regions[i + 2].frame_sink_id);
    EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                  HitTestRegionFlags::kHitTestTouch |
                  HitTestRegionFlags::kHitTestChildSurface |
                  HitTestRegionFlags::kHitTestAsk,
              hit_test_region_list->regions[i + 2].flags);
    EXPECT_EQ(child_rect3, hit_test_region_list->regions[i + 2].rect);

    EXPECT_EQ(child_surface_id1.frame_sink_id(),
              hit_test_region_list->regions[i + 3].frame_sink_id);
    EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                  HitTestRegionFlags::kHitTestTouch |
                  HitTestRegionFlags::kHitTestChildSurface |
                  HitTestRegionFlags::kHitTestAsk,
              hit_test_region_list->regions[i + 3].flags);
    EXPECT_EQ(child_rect, hit_test_region_list->regions[i + 3].rect);

    EXPECT_EQ(child_surface_id2.frame_sink_id(),
              hit_test_region_list->regions[i + 4].frame_sink_id);
    EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                  HitTestRegionFlags::kHitTestTouch |
                  HitTestRegionFlags::kHitTestChildSurface |
                  HitTestRegionFlags::kHitTestAsk,
              hit_test_region_list->regions[i + 4].flags);
    EXPECT_EQ(child_rect2, hit_test_region_list->regions[i + 4].rect);
  }
}

}  // namespace viz
