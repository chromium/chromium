// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/direct_layer_tree_frame_sink.h"

#include <memory>

#include "base/test/test_mock_time_task_runner.h"
#include "cc/test/fake_layer_tree_frame_sink_client.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

class TestDirectLayerTreeFrameSink : public DirectLayerTreeFrameSink {
 public:
  using DirectLayerTreeFrameSink::DirectLayerTreeFrameSink;
};

class TestCompositorFrameSinkSupportManager
    : public CompositorFrameSinkSupportManager {
 public:
  explicit TestCompositorFrameSinkSupportManager(
      FrameSinkManagerImpl* frame_sink_manager)
      : frame_sink_manager_(frame_sink_manager) {}
  ~TestCompositorFrameSinkSupportManager() override = default;

  std::unique_ptr<CompositorFrameSinkSupport> CreateCompositorFrameSinkSupport(
      mojom::CompositorFrameSinkClient* client,
      const FrameSinkId& frame_sink_id,
      bool is_root,
      bool needs_sync_points) override {
    return std::make_unique<CompositorFrameSinkSupport>(
        client, frame_sink_manager_, frame_sink_id, is_root, needs_sync_points);
  }

 private:
  FrameSinkManagerImpl* const frame_sink_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorFrameSinkSupportManager);
};

class DirectLayerTreeFrameSinkTest : public testing::Test {
 public:
  DirectLayerTreeFrameSinkTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>(
            base::TestMockTimeTaskRunner::Type::kStandalone)),
        display_size_(1920, 1080),
        display_rect_(display_size_),
        frame_sink_manager_(&bitmap_manager_),
        support_manager_(&frame_sink_manager_),
        context_provider_(TestContextProvider::Create()) {
    auto display_output_surface = FakeOutputSurface::Create3d();
    display_output_surface_ = display_output_surface.get();

    begin_frame_source_ = std::make_unique<BackToBackBeginFrameSource>(
        std::make_unique<DelayBasedTimeSource>(task_runner_.get()));

    int max_frames_pending = 2;
    auto scheduler = std::make_unique<DisplayScheduler>(
        begin_frame_source_.get(), task_runner_.get(), max_frames_pending);

    display_ = std::make_unique<Display>(
        &bitmap_manager_, RendererSettings(), kArbitraryFrameSinkId,
        std::move(display_output_surface), std::move(scheduler), task_runner_);
    layer_tree_frame_sink_ = std::make_unique<TestDirectLayerTreeFrameSink>(
        kArbitraryFrameSinkId, &support_manager_, &frame_sink_manager_,
        display_.get(), nullptr /* display_client */, context_provider_,
        nullptr, task_runner_, &gpu_memory_buffer_manager_,
        false /* use surface layer to create hit test data */);
    layer_tree_frame_sink_->BindToClient(&layer_tree_frame_sink_client_);
    display_->Resize(display_size_);
    display_->SetVisible(true);

    EXPECT_FALSE(
        layer_tree_frame_sink_client_.did_lose_layer_tree_frame_sink_called());
  }

  ~DirectLayerTreeFrameSinkTest() override {
    layer_tree_frame_sink_->DetachFromClient();
  }

  void SwapBuffersWithDamage(const gfx::Rect& damage_rect) {
    auto frame = CompositorFrameBuilder()
                     .AddRenderPass(display_rect_, damage_rect)
                     .Build();
    layer_tree_frame_sink_->SubmitCompositorFrame(
        std::move(frame), /*hit_test_data_changed=*/false,
        /*show_hit_test_borders=*/false);
  }

  void SendRenderPassList(RenderPassList* pass_list) {
    auto frame = CompositorFrameBuilder()
                     .SetRenderPassList(std::move(*pass_list))
                     .Build();
    pass_list->clear();
    layer_tree_frame_sink_->SubmitCompositorFrame(
        std::move(frame), /*hit_test_data_changed=*/false,
        /*show_hit_test_borders=*/false);
  }

  void SetUp() override {
    // Draw the first frame to start in an "unlocked" state.
    SwapBuffersWithDamage(display_rect_);

    EXPECT_EQ(0u, display_output_surface_->num_sent_frames());
    task_runner_->RunUntilIdle();
    EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  const gfx::Size display_size_;
  const gfx::Rect display_rect_;
  TestSharedBitmapManager bitmap_manager_;
  FrameSinkManagerImpl frame_sink_manager_;
  TestCompositorFrameSinkSupportManager support_manager_;
  TestGpuMemoryBufferManager gpu_memory_buffer_manager_;

  scoped_refptr<TestContextProvider> context_provider_;
  FakeOutputSurface* display_output_surface_ = nullptr;
  std::unique_ptr<BackToBackBeginFrameSource> begin_frame_source_;
  std::unique_ptr<Display> display_;
  cc::FakeLayerTreeFrameSinkClient layer_tree_frame_sink_client_;
  std::unique_ptr<TestDirectLayerTreeFrameSink> layer_tree_frame_sink_;
};

TEST_F(DirectLayerTreeFrameSinkTest, DamageTriggersSwapBuffers) {
  SwapBuffersWithDamage(display_rect_);
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(2u, display_output_surface_->num_sent_frames());
}

TEST_F(DirectLayerTreeFrameSinkTest, NoDamageDoesNotTriggerSwapBuffers) {
  SwapBuffersWithDamage(gfx::Rect());
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, display_output_surface_->num_sent_frames());
}

// Test that hit_test_region_list are created correctly for the browser.
TEST_F(DirectLayerTreeFrameSinkTest, HitTestRegionList) {
  RenderPassList pass_list;

  // Add a DrawQuad that is not a SurfaceDrawQuad. |hit_test_region_list_|
  // shouldn't have any child regions.
  auto pass1 = RenderPass::Create();
  pass1->output_rect = display_rect_;
  pass1->id = 1;
  auto* shared_quad_state1 = pass1->CreateAndAppendSharedQuadState();
  gfx::Rect rect1(display_rect_);
  shared_quad_state1->SetAll(
      gfx::Transform(), rect1 /* quad_layer_rect */,
      rect1 /* visible_quad_layer_rect */,
      gfx::RRectF() /* rounded_corner_bounds */, rect1 /*clip_rect */,
      false /* is_clipped */, false /* are_contents_opaque */,
      0.5f /* opacity */, SkBlendMode::kSrcOver, 0 /* sorting_context_id */);
  auto* quad1 = pass1->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  quad1->SetNew(shared_quad_state1, rect1 /* rect */, rect1 /* visible_rect */,
                SK_ColorBLACK, false /* force_anti_aliasing_off */);
  pass_list.push_back(std::move(pass1));
  SendRenderPassList(&pass_list);
  task_runner_->RunUntilIdle();

  const auto* hit_test_region_list =
      frame_sink_manager_.hit_test_manager()->GetActiveHitTestRegionList(
          display_.get(), display_->CurrentSurfaceId().frame_sink_id());
  EXPECT_TRUE(hit_test_region_list);
  EXPECT_EQ(display_rect_, hit_test_region_list->bounds);
  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestMine,
            hit_test_region_list->flags);
  EXPECT_FALSE(hit_test_region_list->regions.size());

  // Add a SurfaceDrawQuad to one render pass, and add a SolidColorDrawQuad to
  // another render pass, and add a SurfaceDrawQuad with a transform that's not
  // invertible. |hit_test_region_list_| should contain one child
  // region corresponding to that first SurfaceDrawQuad.
  const SurfaceId child_surface_id(
      FrameSinkId(1, 1), LocalSurfaceId(1, base::UnguessableToken::Create()));
  auto pass2 = RenderPass::Create();
  pass2->output_rect = display_rect_;
  pass2->id = 2;
  auto* shared_quad_state2 = pass2->CreateAndAppendSharedQuadState();
  gfx::Rect rect2 = gfx::Rect(20, 20);
  gfx::Transform transform2;
  transform2.Translate(-200, -100);
  shared_quad_state2->SetAll(
      transform2, rect2 /* quad_layer_rect */,
      rect2 /* visible_quad_layer_rect */,
      gfx::RRectF() /* rounded_corner_bounds */, rect2 /*clip_rect */,
      false /* is_clipped */, false /* are_contents_opaque */,
      0.5f /* opacity */, SkBlendMode::kSrcOver, 0 /* sorting_context_id */);
  auto* quad2 = pass2->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
  quad2->SetNew(shared_quad_state2, rect2 /* rect */, rect2 /* visible_rect */,
                SurfaceRange(base::nullopt, child_surface_id), SK_ColorBLACK,
                false /* stretch_content_to_fill_bounds */,
                false /* ignores_input_event */);
  pass_list.push_back(std::move(pass2));

  auto pass3 = RenderPass::Create();
  pass3->output_rect = display_rect_;
  pass3->id = 3;
  auto* shared_quad_state3 = pass3->CreateAndAppendSharedQuadState();
  gfx::Rect rect3(display_rect_);
  shared_quad_state3->SetAll(
      gfx::Transform(), rect3 /* quad_layer_rect */,
      rect3 /* visible_quad_layer_rect */,
      gfx::RRectF() /* rounded_corner_bounds */, rect3 /*clip_rect */,
      false /* is_clipped */, false /* are_contents_opaque */,
      0.5f /* opacity */, SkBlendMode::kSrcOver, 0 /* sorting_context_id */);
  auto* quad3 = pass3->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  quad3->SetNew(shared_quad_state3, rect3 /* rect */, rect3 /* visible_rect */,
                SK_ColorBLACK, false /* force_anti_aliasing_off */);
  pass_list.push_back(std::move(pass3));

  const SurfaceId child_surface_id4(
      FrameSinkId(4, 1), LocalSurfaceId(1, base::UnguessableToken::Create()));
  auto pass4 = RenderPass::Create();
  pass4->output_rect = display_rect_;
  pass4->id = 4;
  auto* shared_quad_state4 = pass4->CreateAndAppendSharedQuadState();
  gfx::Rect rect4(display_rect_);
  // A degenerate matrix of all zeros is not invertible.
  gfx::Transform transform4;
  transform4.matrix().set(0, 0, 0.f);
  transform4.matrix().set(1, 1, 0.f);
  transform4.matrix().set(2, 2, 0.f);
  transform4.matrix().set(3, 3, 0.f);
  shared_quad_state4->SetAll(
      transform4, rect4 /* quad_layer_rect */,
      rect4 /* visible_quad_layer_rect */,
      gfx::RRectF() /* rounded_corner_bounds */, rect4 /*clip_rect */,
      false /* is_clipped */, false /* are_contents_opaque */,
      0.5f /* opacity */, SkBlendMode::kSrcOver, 0 /* sorting_context_id */);
  auto* quad4 = pass4->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
  quad4->SetNew(shared_quad_state4, rect4 /* rect */, rect4 /* visible_rect */,
                SurfaceRange(base::nullopt, child_surface_id4), SK_ColorBLACK,
                false /* stretch_content_to_fill_bounds */,
                false /* ignores_input_event */);
  pass_list.push_back(std::move(pass4));

  auto pass5_root = RenderPass::Create();
  pass5_root->output_rect = display_rect_;
  pass5_root->id = 5;
  auto* shared_quad_state5_root = pass5_root->CreateAndAppendSharedQuadState();
  gfx::Rect rect5_root(display_rect_);
  shared_quad_state5_root->SetAll(
      gfx::Transform(), /*quad_layer_rect=*/rect5_root,
      /*visible_quad_layer_rect=*/rect5_root,
      gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect5_root,
      /*is_clipped=*/false, /*are_contents_opaque=*/false,
      /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
  auto* quad5_root_1 =
      pass5_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
  quad5_root_1->SetNew(shared_quad_state5_root, /*rect=*/rect5_root,
                       /*visible_rect=*/rect5_root, /*render_pass_id=*/2,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  auto* quad5_root_2 =
      pass5_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
  quad5_root_2->SetNew(shared_quad_state5_root, /*rect=*/rect5_root,
                       /*visible_rect=*/rect5_root, /*render_pass_id=*/3,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  auto* quad5_root_3 =
      pass5_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
  quad5_root_3->SetNew(shared_quad_state5_root, /*rect=*/rect5_root,
                       /*visible_rect=*/rect5_root, /*render_pass_id=*/4,
                       /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                       gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                       1.0f);
  pass_list.push_back(std::move(pass5_root));

  SendRenderPassList(&pass_list);
  task_runner_->RunUntilIdle();

  const auto* hit_test_region_list1 =
      frame_sink_manager_.hit_test_manager()->GetActiveHitTestRegionList(
          display_.get(), display_->CurrentSurfaceId().frame_sink_id());
  EXPECT_TRUE(hit_test_region_list1);
  EXPECT_EQ(display_rect_, hit_test_region_list1->bounds);
  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestMine,
            hit_test_region_list1->flags);
  EXPECT_EQ(1u, hit_test_region_list1->regions.size());
  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list1->regions[0].frame_sink_id);
  EXPECT_EQ(HitTestRegionFlags::kHitTestMouse |
                HitTestRegionFlags::kHitTestTouch |
                HitTestRegionFlags::kHitTestChildSurface,
            hit_test_region_list1->regions[0].flags);
  EXPECT_EQ(gfx::Rect(20, 20), hit_test_region_list1->regions[0].rect);
  gfx::Transform transform2_inverse;
  EXPECT_TRUE(transform2.GetInverse(&transform2_inverse));
  EXPECT_EQ(transform2_inverse, hit_test_region_list1->regions[0].transform);
}

// Test that hit_test_region_list are not sent if there have been no changes to
// hit-test data.
TEST_F(DirectLayerTreeFrameSinkTest, HitTestRegionListDuplicate) {
  RenderPassList pass_list;

  // No child regions. Expect the second submission to not be sent.
  {
    auto pass1 = RenderPass::Create();
    pass1->output_rect = display_rect_;
    pass1->id = 1;
    auto* shared_quad_state1 = pass1->CreateAndAppendSharedQuadState();
    gfx::Rect rect1(display_rect_);
    shared_quad_state1->SetAll(
        gfx::Transform(), /*quad_layer_rect=*/rect1,
        /*visible_quad_layer_rect=*/rect1,
        gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect1,
        /*is_clipped=*/false, /*are_contents_opaque=*/false,
        /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
    auto* quad1 = pass1->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    quad1->SetNew(shared_quad_state1, /*rect=*/rect1,
                  /*visible_rect=*/rect1, SK_ColorBLACK,
                  /*force_anti_aliasing_off=*/false);
    pass_list.push_back(std::move(pass1));
    SendRenderPassList(&pass_list);
    task_runner_->RunUntilIdle();
    const uint64_t pass1_index = frame_sink_manager_.hit_test_manager()
                                     ->submit_hit_test_region_list_index();

    auto pass2 = RenderPass::Create();
    pass2->output_rect = display_rect_;
    pass2->id = 2;
    auto* shared_quad_state2 = pass2->CreateAndAppendSharedQuadState();
    gfx::Rect rect2(display_rect_);
    shared_quad_state2->SetAll(
        gfx::Transform(), /*quad_layer_rect=*/rect2,
        /*visible_quad_layer_rect=*/rect2,
        gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect2,
        /*is_clipped=*/false, /*are_contents_opaque=*/false,
        /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
    auto* quad2 = pass2->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    quad2->SetNew(shared_quad_state2, /*rect=*/rect2,
                  /*visible_rect=*/rect2, SK_ColorBLACK,
                  /*force_anti_aliasing_off=*/false);
    pass_list.push_back(std::move(pass2));
    SendRenderPassList(&pass_list);
    task_runner_->RunUntilIdle();
    const uint64_t pass2_index = frame_sink_manager_.hit_test_manager()
                                     ->submit_hit_test_region_list_index();

    EXPECT_EQ(pass1_index, pass2_index);
  }

  // Child region. The second submission changes the transform. The fourth
  // submission changes the rect. Expect the first, second and fourth
  // submissions to be sent.
  {
    // First submission.
    const SurfaceId child_surface_id(
        FrameSinkId(1, 1), LocalSurfaceId(1, base::UnguessableToken::Create()));
    auto pass3_0 = RenderPass::Create();
    pass3_0->output_rect = display_rect_;
    pass3_0->id = 3;
    auto* shared_quad_state3_0 = pass3_0->CreateAndAppendSharedQuadState();
    gfx::Rect rect3_0 = gfx::Rect(20, 20);
    gfx::Transform transform3_0;
    transform3_0.Translate(-200, -100);
    shared_quad_state3_0->SetAll(
        transform3_0, /*quad_layer_rect=*/rect3_0,
        /*visible_quad_layer_rect=*/rect3_0,
        gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect3_0,
        /*is_clipped=*/false, /*are_contents_opaque=*/false,
        /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
    auto* quad3_0 = pass3_0->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
    quad3_0->SetNew(shared_quad_state3_0, /*rect=*/rect3_0,
                    /*visible_rect=*/rect3_0,
                    SurfaceRange(base::nullopt, child_surface_id),
                    SK_ColorBLACK,
                    /*stretch_content_to_fill_bounds=*/false,
                    /*ignores_input_event=*/false);
    pass_list.push_back(std::move(pass3_0));

    auto pass3_1 = RenderPass::Create();
    pass3_1->output_rect = display_rect_;
    pass3_1->id = 4;
    auto* shared_quad_state3_1 = pass3_1->CreateAndAppendSharedQuadState();
    gfx::Rect rect3_1(display_rect_);
    shared_quad_state3_1->SetAll(
        gfx::Transform(), /*quad_layer_rect=*/rect3_1,
        /*visible_quad_layer_rect=*/rect3_1,
        gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect3_1,
        /*is_clipped=*/false, /*are_contents_opaque=*/false,
        /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
    auto* quad3_1 =
        pass3_1->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    quad3_1->SetNew(shared_quad_state3_1, /*rect=*/rect3_1,
                    /*visible_rect=*/rect3_1, SK_ColorBLACK,
                    /*force_anti_aliasing_off=*/false);
    pass_list.push_back(std::move(pass3_1));

    auto pass3_root = RenderPass::Create();
    pass3_root->output_rect = display_rect_;
    pass3_root->id = 5;
    auto* shared_quad_state3_root =
        pass3_root->CreateAndAppendSharedQuadState();
    gfx::Rect rect3_root(display_rect_);
    shared_quad_state3_root->SetAll(
        gfx::Transform(), /*quad_layer_rect=*/rect3_root,
        /*visible_quad_layer_rect=*/rect3_root,
        gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect3_root,
        /*is_clipped=*/false, /*are_contents_opaque=*/false,
        /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
    auto* quad3_root_1 =
        pass3_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
    quad3_root_1->SetNew(shared_quad_state3_root, /*rect=*/rect3_root,
                         /*visible_rect=*/rect3_root, /*render_pass_id=*/3,
                         /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                         gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                         false, 1.0f);
    auto* quad3_root_2 =
        pass3_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
    quad3_root_2->SetNew(shared_quad_state3_root, /*rect=*/rect3_root,
                         /*visible_rect=*/rect3_root, /*render_pass_id=*/4,
                         /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                         gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                         false, 1.0f);
    pass_list.push_back(std::move(pass3_root));

    SendRenderPassList(&pass_list);
    task_runner_->RunUntilIdle();

    const uint64_t pass3_index = frame_sink_manager_.hit_test_manager()
                                     ->submit_hit_test_region_list_index();

    // Second submission.
    auto pass4_0 = RenderPass::Create();
    pass4_0->output_rect = display_rect_;
    pass4_0->id = 5;
    auto* shared_quad_state4_0 = pass4_0->CreateAndAppendSharedQuadState();
    gfx::Rect rect4_0 = gfx::Rect(20, 20);
    gfx::Transform transform4_0;
    transform4_0.Translate(-199, -100);
    shared_quad_state4_0->SetAll(
        transform4_0, /*quad_layer_rect=*/rect4_0,
        /*visible_quad_layer_rect=*/rect4_0,
        gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect4_0,
        /*is_clipped=*/false, /*are_contents_opaque=*/false,
        /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
    auto* quad4_0 = pass4_0->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
    quad4_0->SetNew(shared_quad_state4_0, /*rect=*/rect4_0,
                    /*visible_rect=*/rect4_0,
                    SurfaceRange(base::nullopt, child_surface_id),
                    SK_ColorBLACK,
                    /*stretch_content_to_fill_bounds=*/false,
                    /*ignores_input_event=*/false);
    pass_list.push_back(std::move(pass4_0));

    auto pass4_1 = RenderPass::Create();
    pass4_1->output_rect = display_rect_;
    pass4_1->id = 6;
    auto* shared_quad_state4_1 = pass4_1->CreateAndAppendSharedQuadState();
    gfx::Rect rect4_1(display_rect_);
    shared_quad_state4_1->SetAll(
        gfx::Transform(), /*quad_layer_rect=*/rect4_1,
        /*visible_quad_layer_rect=*/rect4_1,
        gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect4_1,
        /*is_clipped=*/false, /*are_contents_opaque=*/false,
        /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
    auto* quad4_1 =
        pass4_1->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    quad4_1->SetNew(shared_quad_state4_1, /*rect=*/rect4_1,
                    /*visible_rect=*/rect4_1, SK_ColorBLACK,
                    /*force_anti_aliasing_off=*/false);
    pass_list.push_back(std::move(pass4_1));

    auto pass4_root = RenderPass::Create();
    pass4_root->output_rect = display_rect_;
    pass4_root->id = 5;
    auto* shared_quad_state4_root =
        pass4_root->CreateAndAppendSharedQuadState();
    gfx::Rect rect4_root(display_rect_);
    shared_quad_state4_root->SetAll(
        gfx::Transform(), /*quad_layer_rect=*/rect4_root,
        /*visible_quad_layer_rect=*/rect4_root,
        gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect4_root,
        /*is_clipped=*/false, /*are_contents_opaque=*/false,
        /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
    auto* quad4_root_1 =
        pass4_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
    quad4_root_1->SetNew(shared_quad_state4_root, /*rect=*/rect4_root,
                         /*visible_rect=*/rect4_root, /*render_pass_id=*/5,
                         /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                         gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                         false, 1.0f);
    auto* quad4_root_2 =
        pass4_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
    quad4_root_2->SetNew(shared_quad_state4_root, /*rect=*/rect4_root,
                         /*visible_rect=*/rect4_root, /*render_pass_id=*/6,
                         /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                         gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                         false, 1.0f);
    pass_list.push_back(std::move(pass4_root));

    SendRenderPassList(&pass_list);
    task_runner_->RunUntilIdle();

    const uint64_t pass4_index = frame_sink_manager_.hit_test_manager()
                                     ->submit_hit_test_region_list_index();

    EXPECT_FALSE(pass3_index == pass4_index);

    // Third submission.
    auto pass5_0 = RenderPass::Create();
    pass5_0->output_rect = display_rect_;
    pass5_0->id = 7;
    auto* shared_quad_state5_0 = pass5_0->CreateAndAppendSharedQuadState();
    gfx::Rect rect5_0 = gfx::Rect(20, 20);
    gfx::Transform transform5_0;
    transform5_0.Translate(-199, -100);
    shared_quad_state5_0->SetAll(
        transform5_0, /*quad_layer_rect=*/rect5_0,
        /*visible_quad_layer_rect=*/rect5_0,
        gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect5_0,
        /*is_clipped=*/false, /*are_contents_opaque=*/false,
        /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
    auto* quad5_0 = pass5_0->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
    quad5_0->SetNew(shared_quad_state5_0, /*rect=*/rect5_0,
                    /*visible_rect=*/rect5_0,
                    SurfaceRange(base::nullopt, child_surface_id),
                    SK_ColorBLACK,
                    /*stretch_content_to_fill_bounds=*/false,
                    /*ignores_input_event=*/false);
    pass_list.push_back(std::move(pass5_0));

    auto pass5_1 = RenderPass::Create();
    pass5_1->output_rect = display_rect_;
    pass5_1->id = 8;
    auto* shared_quad_state5_1 = pass5_1->CreateAndAppendSharedQuadState();
    gfx::Rect rect5_1(display_rect_);
    shared_quad_state5_1->SetAll(
        gfx::Transform(), /*quad_layer_rect=*/rect5_1,
        /*visible_quad_layer_rect=*/rect5_1,
        gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect5_1,
        /*is_clipped=*/false, /*are_contents_opaque=*/false,
        /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
    auto* quad5_1 =
        pass5_1->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    quad5_1->SetNew(shared_quad_state5_1, /*rect=*/rect5_1,
                    /*visible_rect=*/rect5_1, SK_ColorBLACK,
                    /*force_anti_aliasing_off=*/false);
    pass_list.push_back(std::move(pass5_1));

    auto pass5_root = RenderPass::Create();
    pass5_root->output_rect = display_rect_;
    pass5_root->id = 5;
    auto* shared_quad_state5_root =
        pass5_root->CreateAndAppendSharedQuadState();
    gfx::Rect rect5_root(display_rect_);
    shared_quad_state5_root->SetAll(
        gfx::Transform(), /*quad_layer_rect=*/rect5_root,
        /*visible_quad_layer_rect=*/rect5_root,
        gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect5_root,
        /*is_clipped=*/false, /*are_contents_opaque=*/false,
        /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
    auto* quad5_root_1 =
        pass5_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
    quad5_root_1->SetNew(shared_quad_state5_root, /*rect=*/rect5_root,
                         /*visible_rect=*/rect5_root, /*render_pass_id=*/7,
                         /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                         gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                         false, 1.0f);
    auto* quad5_root_2 =
        pass5_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
    quad5_root_2->SetNew(shared_quad_state5_root, /*rect=*/rect5_root,
                         /*visible_rect=*/rect5_root, /*render_pass_id=*/8,
                         /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                         gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                         false, 1.0f);
    pass_list.push_back(std::move(pass5_root));

    SendRenderPassList(&pass_list);
    task_runner_->RunUntilIdle();

    const uint64_t pass5_index = frame_sink_manager_.hit_test_manager()
                                     ->submit_hit_test_region_list_index();

    EXPECT_FALSE(pass3_index == pass5_index);
    EXPECT_EQ(pass5_index, pass5_index);

    // Fourth submission.
    auto pass6_0 = RenderPass::Create();
    pass6_0->output_rect = display_rect_;
    pass6_0->id = 9;
    auto* shared_quad_state6_0 = pass6_0->CreateAndAppendSharedQuadState();
    gfx::Rect rect6_0 = gfx::Rect(21, 20);
    gfx::Transform transform6_0;
    transform6_0.Translate(-199, -100);
    shared_quad_state6_0->SetAll(
        transform6_0, /*quad_layer_rect=*/rect6_0,
        /*visible_quad_layer_rect=*/rect6_0,
        gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect6_0,
        /*is_clipped=*/false, /*are_contents_opaque=*/false,
        /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
    auto* quad6_0 = pass6_0->quad_list.AllocateAndConstruct<SurfaceDrawQuad>();
    quad6_0->SetNew(shared_quad_state6_0, /*rect=*/rect6_0,
                    /*visible_rect=*/rect6_0,
                    SurfaceRange(base::nullopt, child_surface_id),
                    SK_ColorBLACK,
                    /*stretch_content_to_fill_bounds=*/false,
                    /*ignores_input_event=*/false);
    pass_list.push_back(std::move(pass6_0));

    auto pass6_1 = RenderPass::Create();
    pass6_1->output_rect = display_rect_;
    pass6_1->id = 10;
    auto* shared_quad_state6_1 = pass6_1->CreateAndAppendSharedQuadState();
    gfx::Rect rect6_1(display_rect_);
    shared_quad_state6_1->SetAll(
        gfx::Transform(), /*quad_layer_rect=*/rect6_1,
        /*visible_quad_layer_rect=*/rect6_1,
        gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect6_1,
        /*is_clipped=*/false, /*are_contents_opaque=*/false,
        /*opacity=*/0.5f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
    auto* quad6_1 =
        pass6_1->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    quad6_1->SetNew(shared_quad_state6_1, /*rect=*/rect6_1,
                    /*visible_rect=*/rect6_1, SK_ColorBLACK,
                    /*force_anti_aliasing_off=*/false);
    pass_list.push_back(std::move(pass6_1));

    auto pass6_root = RenderPass::Create();
    pass6_root->output_rect = display_rect_;
    pass6_root->id = 6;
    auto* shared_quad_state6_root =
        pass6_root->CreateAndAppendSharedQuadState();
    gfx::Rect rect6_root(display_rect_);
    shared_quad_state6_root->SetAll(
        gfx::Transform(), /*quad_layer_rect=*/rect6_root,
        /*visible_quad_layer_rect=*/rect6_root,
        gfx::RRectF() /* rounded_corner_bounds */, /*clip_rect=*/rect6_root,
        /*is_clipped=*/false, /*are_contents_opaque=*/false,
        /*opacity=*/0.6f, SkBlendMode::kSrcOver, /*sorting_context_id=*/0);
    auto* quad6_root_1 =
        pass6_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
    quad6_root_1->SetNew(shared_quad_state6_root, /*rect=*/rect6_root,
                         /*visible_rect=*/rect6_root, /*render_pass_id=*/9,
                         /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                         gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                         false, 1.0f);
    auto* quad6_root_2 =
        pass6_root->quad_list.AllocateAndConstruct<RenderPassDrawQuad>();
    quad6_root_2->SetNew(shared_quad_state6_root, /*rect=*/rect6_root,
                         /*visible_rect=*/rect6_root, /*render_pass_id=*/10,
                         /*mask_resource_id=*/0, gfx::RectF(), gfx::Size(),
                         gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                         false, 1.0f);
    pass_list.push_back(std::move(pass6_root));

    SendRenderPassList(&pass_list);
    task_runner_->RunUntilIdle();

    const uint64_t pass6_index = frame_sink_manager_.hit_test_manager()
                                     ->submit_hit_test_region_list_index();

    EXPECT_FALSE(pass5_index == pass6_index);
  }
}

}  // namespace
}  // namespace viz
