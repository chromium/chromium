// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/test/scheduler_test_common.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/pending_copy_output_request.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "components/viz/test/test_surface_id_allocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
namespace {

constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);
constexpr bool kIsRoot = true;
const uint64_t kBeginFrameSourceId = 1337;

class SurfaceTest : public testing::Test {
 public:
  SurfaceTest()
      : frame_sink_manager_(
            FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_)) {}

 protected:
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl frame_sink_manager_;
};

// Supports testing features::OnBeginFrameAcks, which changes the expectations
// of what IPCs are sent to the CompositorFrameSinkClient. When enabled
// OnBeginFrame also handles ReturnResources as well as
// DidReceiveCompositorFrameAck.
class OnBeginFrameAcksSurfaceTest : public SurfaceTest,
                                    public testing::WithParamInterface<bool> {
 public:
  OnBeginFrameAcksSurfaceTest();
  ~OnBeginFrameAcksSurfaceTest() override = default;

  bool BeginFrameAcksEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

OnBeginFrameAcksSurfaceTest::OnBeginFrameAcksSurfaceTest() {
  if (BeginFrameAcksEnabled()) {
    scoped_feature_list_.InitAndEnableFeature(features::kOnBeginFrameAcks);
  } else {
    scoped_feature_list_.InitAndDisableFeature(features::kOnBeginFrameAcks);
  }
}

TEST_P(OnBeginFrameAcksSurfaceTest, PresentationCallback) {
  constexpr gfx::Size kSurfaceSize(300, 300);
  constexpr gfx::Rect kDamageRect(0, 0);
  const LocalSurfaceId local_surface_id(6, base::UnguessableToken::Create());

  MockCompositorFrameSinkClient client;
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &client, &frame_sink_manager_, kArbitraryFrameSinkId, kIsRoot);
  if (BeginFrameAcksEnabled()) {
    support->SetWantsBeginFrameAcks();
  }
  uint32_t frame_token = kInvalidFrameToken;
  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(gfx::Rect(kSurfaceSize), kDamageRect)
            .SetBeginFrameSourceId(kBeginFrameSourceId)
            .Build();
    frame_token = frame.metadata.frame_token;
    ASSERT_NE(frame_token, kInvalidFrameToken);
    EXPECT_CALL(client, DidReceiveCompositorFrameAck(testing::_))
        .Times(BeginFrameAcksEnabled() ? 0 : 1);
    support->SubmitCompositorFrame(local_surface_id, std::move(frame));
    testing::Mock::VerifyAndClearExpectations(&client);
  }

  {
    // Replaces previous frame. The previous frame with token 1 will be
    // discarded.
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(gfx::Rect(kSurfaceSize), kDamageRect)
            .SetBeginFrameSourceId(kBeginFrameSourceId)
            .Build();
    EXPECT_CALL(client, DidReceiveCompositorFrameAck(testing::_))
        .Times(BeginFrameAcksEnabled() ? 0 : 1);
    support->SubmitCompositorFrame(local_surface_id, std::move(frame));
    ASSERT_EQ(1u, support->timing_details().size());
    EXPECT_EQ(frame_token, support->timing_details().begin()->first);
    testing::Mock::VerifyAndClearExpectations(&client);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         OnBeginFrameAcksSurfaceTest,
                         testing::Bool(),
                         [](auto& info) {
                           return info.param ? "BeginFrameAcks"
                                             : "CompositoFrameAcks";
                         });

TEST_F(SurfaceTest, SurfaceIds) {
  for (size_t i = 0; i < 3; ++i) {
    ParentLocalSurfaceIdAllocator allocator;
    allocator.GenerateId();
    LocalSurfaceId id1 = allocator.GetCurrentLocalSurfaceId();
    allocator.GenerateId();
    LocalSurfaceId id2 = allocator.GetCurrentLocalSurfaceId();
    EXPECT_NE(id1, id2);
  }
}

void TestCopyResultCallback(bool* called,
                            base::OnceClosure finished,
                            std::unique_ptr<CopyOutputResult> result) {
  *called = true;
  std::move(finished).Run();
}

// Test that CopyOutputRequests can outlive the current frame and be
// aggregated on the next frame.
TEST_F(SurfaceTest, CopyRequestLifetime) {
  SurfaceManager* surface_manager = frame_sink_manager_.surface_manager();
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &frame_sink_manager_, kArbitraryFrameSinkId, kIsRoot);

  LocalSurfaceId local_surface_id(6, base::UnguessableToken::Create());
  SurfaceId surface_id(kArbitraryFrameSinkId, local_surface_id);
  CompositorFrame frame = MakeDefaultCompositorFrame(kBeginFrameSourceId);
  support->SubmitCompositorFrame(local_surface_id, std::move(frame));
  Surface* surface = surface_manager->GetSurfaceForId(surface_id);
  ASSERT_TRUE(surface);

  bool copy_called = false;
  base::RunLoop copy_runloop;
  support->RequestCopyOfOutput(PendingCopyOutputRequest{
      local_surface_id, SubtreeCaptureId(),
      std::make_unique<CopyOutputRequest>(
          CopyOutputRequest::ResultFormat::RGBA,
          CopyOutputRequest::ResultDestination::kSystemMemory,
          base::BindOnce(&TestCopyResultCallback, &copy_called,
                         copy_runloop.QuitClosure()))});
  surface->TakeCopyOutputRequestsFromClient();
  EXPECT_TRUE(surface_manager->GetSurfaceForId(surface_id));
  EXPECT_FALSE(copy_called);

  int max_frame = 3, start_id = 200;
  for (int i = 0; i < max_frame; ++i) {
    frame = CompositorFrameBuilder().Build();
    frame.render_pass_list.push_back(CompositorRenderPass::Create());
    frame.render_pass_list.back()->id =
        CompositorRenderPassId{i * 3 + start_id};
    frame.render_pass_list.push_back(CompositorRenderPass::Create());
    frame.render_pass_list.back()->id =
        CompositorRenderPassId{i * 3 + start_id + 1};
    frame.render_pass_list.push_back(CompositorRenderPass::Create());
    frame.render_pass_list.back()->SetNew(
        CompositorRenderPassId{i * 3 + start_id + 2}, gfx::Rect(0, 0, 20, 20),
        gfx::Rect(), gfx::Transform());
    support->SubmitCompositorFrame(local_surface_id, std::move(frame));
  }

  CompositorRenderPassId last_pass_id{(max_frame - 1) * 3 + start_id + 2};
  // The copy request should stay on the Surface until TakeCopyOutputRequests
  // is called.
  EXPECT_FALSE(copy_called);
  EXPECT_EQ(
      1u,
      surface->GetActiveFrame().render_pass_list.back()->copy_requests.size());

  Surface::CopyRequestsMap copy_requests;
  surface->TakeCopyOutputRequests(&copy_requests);
  EXPECT_EQ(1u, copy_requests.size());
  // Last (root) pass should receive copy request.
  ASSERT_EQ(1u, copy_requests.count(last_pass_id));
  EXPECT_FALSE(copy_called);
  copy_requests.clear();  // Deleted requests will auto-send an empty result.
  copy_runloop.Run();
  EXPECT_TRUE(copy_called);
}

// Verify activate referenced surfaces is correct when there are two surface
// references to overlapping surface ranges. In particular the two surface
// ranges are (S1, S1) and (S1, S2). When both S1 and S2 activate active
// referenced surfaces should include both S1 and S2. See
// https://crbug.com/1275605 for more context.
TEST_F(SurfaceTest, ActiveSurfaceReferencesWithOverlappingReferences) {
  constexpr gfx::Rect output_rect(100, 100);
  SurfaceManager* surface_manager = frame_sink_manager_.surface_manager();

  auto root_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &frame_sink_manager_, kArbitraryFrameSinkId,
      /*is_root=*/true);
  TestSurfaceIdAllocator root_surface_id(kArbitraryFrameSinkId);

  auto child_support1 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &frame_sink_manager_, FrameSinkId(2, 1), /*is_root=*/false);
  TestSurfaceIdAllocator child_surface_id1(child_support1->frame_sink_id());

  auto child_support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &frame_sink_manager_, FrameSinkId(3, 1), /*is_root=*/false);
  TestSurfaceIdAllocator child_surface_id2(child_support2->frame_sink_id());

  // Submit a root frame with two SurfaceDrawQuads. The first SurfaceDrawQuad
  // embeds |child_support1|. The second SurfaceDrawQuad embeds |child_support2|
  // but has |child_support1| as a fallback which mimics a navigating renderer.
  // Note that |old_surface_range| and |navigation_surface_range| overlap.
  SurfaceRange old_surface_range(child_surface_id1, child_surface_id1);
  SurfaceRange navigation_surface_range(child_surface_id1, child_surface_id2);
  auto root_render_pass =
      RenderPassBuilder(CompositorRenderPassId{1}, output_rect)
          .AddSurfaceQuad(output_rect, old_surface_range)
          .AddSurfaceQuad(output_rect, navigation_surface_range)
          .Build();

  {
    CompositorFrame frame = MakeCompositorFrame(root_render_pass->DeepCopy());
    EXPECT_THAT(
        frame.metadata.referenced_surfaces,
        testing::ElementsAre(old_surface_range, navigation_surface_range));

    root_support->SubmitCompositorFrame(root_surface_id.local_surface_id(),
                                        std::move(frame));
  }

  Surface* root_surface = surface_manager->GetSurfaceForId(root_surface_id);
  ASSERT_TRUE(root_surface);

  // No active references yet because no child surfaces have been submitted yet.
  EXPECT_THAT(root_surface->active_referenced_surfaces(), testing::IsEmpty());

  // Submit something to the second child surface and verify it's now included
  // in active referenced surfaces.
  child_support2->SubmitCompositorFrame(
      child_surface_id2.local_surface_id(),
      MakeDefaultCompositorFrame(kBeginFrameSourceId));
  EXPECT_TRUE(surface_manager->GetSurfaceForId(child_surface_id2));
  EXPECT_THAT(root_surface->active_referenced_surfaces(),
              testing::ElementsAre(child_surface_id2));

  // Submit something to the first child surface and verify both are in active
  // surface references. Note this order of activation is "backwards" as
  // normally the |child_surface_id1| would have activated first if the browser
  // is navigating away from it but if the first renderer is slow to produce
  // content the order can be reversed.
  child_support1->SubmitCompositorFrame(
      child_surface_id1.local_surface_id(),
      MakeDefaultCompositorFrame(kBeginFrameSourceId));
  EXPECT_TRUE(surface_manager->GetSurfaceForId(child_surface_id1));
  EXPECT_THAT(root_surface->active_referenced_surfaces(),
              testing::ElementsAre(child_surface_id1, child_surface_id2));

  // Resubmit root frame with the same SurfaceDrawQuads and verify active
  // surface references are unchanged.
  root_support->SubmitCompositorFrame(
      root_surface_id.local_surface_id(),
      MakeCompositorFrame(std::move(root_render_pass)));
  EXPECT_THAT(root_surface->active_referenced_surfaces(),
              testing::ElementsAre(child_surface_id1, child_surface_id2));

  // Submit a new root frame without the reference to the first child surface
  // and verify |child_surface_id1| is no longer part of active referenced
  // surfaces.
  {
    SurfaceRange post_navigation_surface_range(child_surface_id2,
                                               child_surface_id2);
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{1}, output_rect)
                    .AddSurfaceQuad(output_rect, post_navigation_surface_range))
            .Build();

    root_support->SubmitCompositorFrame(root_surface_id.local_surface_id(),
                                        std::move(frame));
  }

  EXPECT_THAT(root_surface->active_referenced_surfaces(),
              testing::ElementsAre(child_surface_id2));
}

TEST_F(SurfaceTest, PendingCopySurfaceIncludedInActiveReferencedSurfaces) {
  SurfaceManager* surface_manager = frame_sink_manager_.surface_manager();

  gfx::Rect rect(5, 5);

  auto support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &frame_sink_manager_, kArbitraryFrameSinkId,
      /*is_root=*/false);

  TestSurfaceIdAllocator allocator(kArbitraryFrameSinkId);
  SurfaceId prev_id = allocator.Get();
  allocator.Increment();
  SurfaceId curr_id = allocator.Get();

  {
    CompositorFrame frame =
        MakeCompositorFrame(RenderPassBuilder(CompositorRenderPassId{1}, rect)
                                .AddSolidColorQuad(rect, SkColors::kBlue)
                                .AddSolidColorQuad(rect, SkColors::kBlue)
                                .Build());
    support->SubmitCompositorFrame(prev_id.local_surface_id(),
                                   std::move(frame));
  }
  {
    CompositorFrame frame =
        MakeCompositorFrame(RenderPassBuilder(CompositorRenderPassId{2}, rect)
                                .AddSolidColorQuad(rect, SkColors::kBlue)
                                .AddSolidColorQuad(rect, SkColors::kBlue)
                                .Build());
    frame.metadata.screenshot_destination =
        blink::SameDocNavigationScreenshotDestinationToken(
            base::UnguessableToken::Create());
    support->SubmitCompositorFrame(curr_id.local_surface_id(),
                                   std::move(frame));
  }

  auto* curr_surface = surface_manager->GetSurfaceForId(curr_id);
  ASSERT_TRUE(curr_surface);
  ASSERT_THAT(curr_surface->active_referenced_surfaces(),
              ::testing::UnorderedElementsAre(prev_id));

  curr_surface->ResetPendingCopySurfaceId();
  ASSERT_TRUE(curr_surface->active_referenced_surfaces().empty());
}

// Parameterized by whether we should enable kDrawImmediatelyWhenInteractive.
class ImmediateActivationSurfaceTest
    : public SurfaceTest,
      public testing::WithParamInterface<bool> {
 public:
  ImmediateActivationSurfaceTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kDrawImmediatelyWhenInteractive);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kDrawImmediatelyWhenInteractive);
    }
  }

  void SetUp() override {
    SurfaceTest::SetUp();
    now_src_ = std::make_unique<base::SimpleTestTickClock>();
    frame_sink_manager_.surface_manager()->SetTickClockForTesting(
        now_src_.get());
  }

  base::TimeTicks Now() { return now_src_->NowTicks(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, ImmediateActivationSurfaceTest, testing::Bool());

// Checks that submitting a compositor frame with a dependency always results in
// activation dependencies if we have no interaction.
TEST_P(ImmediateActivationSurfaceTest, WithNoInteraction) {
  constexpr gfx::Rect output_rect(100, 100);
  SurfaceManager* surface_manager = frame_sink_manager_.surface_manager();

  auto root_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &frame_sink_manager_, kArbitraryFrameSinkId,
      /*is_root=*/true);
  TestSurfaceIdAllocator root_surface_id(kArbitraryFrameSinkId);

  auto child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &frame_sink_manager_, FrameSinkId(2, 1), /*is_root=*/false);
  TestSurfaceIdAllocator child_surface_id(child_support->frame_sink_id());

  // Submit a root frame with one SurfaceDrawQuad. The SurfaceDrawQuad embeds
  // |child_support| as it would with an OOPIF.
  SurfaceRange surface_range(child_surface_id);
  auto root_render_pass =
      RenderPassBuilder(CompositorRenderPassId{1}, output_rect)
          .AddSurfaceQuad(output_rect, surface_range)
          .Build();

  {
    CompositorFrame frame = MakeCompositorFrame(root_render_pass->DeepCopy());
    frame.metadata.activation_dependencies.push_back(child_surface_id);
    frame.metadata.deadline =
        FrameDeadline(Now(), 4u, BeginFrameArgs::DefaultInterval(), false);
    EXPECT_THAT(frame.metadata.referenced_surfaces,
                testing::ElementsAre(surface_range));
    root_support->SubmitCompositorFrame(root_surface_id.local_surface_id(),
                                        std::move(frame));
  }

  Surface* surface = surface_manager->GetSurfaceForId(root_surface_id);
  EXPECT_FALSE(surface->activation_dependencies().empty());
}

// Checks that submitting a compositor frame with a dependency only results in
// activation dependencies if we haven't enabled immediate activation.
TEST_P(ImmediateActivationSurfaceTest, WithInteraction) {
  constexpr gfx::Rect output_rect(100, 100);
  SurfaceManager* surface_manager = frame_sink_manager_.surface_manager();

  auto root_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &frame_sink_manager_, kArbitraryFrameSinkId,
      /*is_root=*/true);
  TestSurfaceIdAllocator root_surface_id(kArbitraryFrameSinkId);

  auto child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &frame_sink_manager_, FrameSinkId(2, 1), /*is_root=*/false);
  TestSurfaceIdAllocator child_surface_id(child_support->frame_sink_id());

  // Submit a root frame with one SurfaceDrawQuad. The SurfaceDrawQuad embeds
  // |child_support| as it would with an OOPIF.
  SurfaceRange surface_range(child_surface_id);
  auto root_render_pass =
      RenderPassBuilder(CompositorRenderPassId{1}, output_rect)
          .AddSurfaceQuad(output_rect, surface_range)
          .Build();

  {
    CompositorFrame frame = MakeCompositorFrame(root_render_pass->DeepCopy());
    frame.metadata.activation_dependencies.push_back(child_surface_id);
    frame.metadata.deadline =
        FrameDeadline(Now(), 4u, BeginFrameArgs::DefaultInterval(), false);
    frame.metadata.is_handling_interaction = true;
    EXPECT_THAT(frame.metadata.referenced_surfaces,
                testing::ElementsAre(surface_range));
    root_support->SubmitCompositorFrame(root_surface_id.local_surface_id(),
                                        std::move(frame));
  }

  Surface* surface = surface_manager->GetSurfaceForId(root_surface_id);
  EXPECT_EQ(surface->activation_dependencies().empty(), GetParam());
}

}  // namespace
}  // namespace viz
