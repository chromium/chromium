// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_damage_tracker.h"

#include <utility>

#include "base/test/null_task_runner.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display_resource_provider_software.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {
constexpr FrameSinkId kRootFrameSinkId(3, 3);
constexpr FrameSinkId kChildFrameSinkId(4, 4);
}  // namespace

class DisplayDamageTrackerTest : public testing::Test {
 public:
  DisplayDamageTrackerTest()
      : manager_(FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_)),
        resource_provider_(&shared_bitmap_manager_,
                           &shared_image_manager_,
                           &sync_point_manager_,
                           &gpu_scheduler_),
        aggregator_(manager_.surface_manager(), &resource_provider_, false),
        root_client_(&manager_, kRootFrameSinkId),
        task_runner_(base::MakeRefCounted<base::NullTaskRunner>()),
        fake_begin_frame_source_(0.f, false),
        damage_tracker_(
            std::make_unique<DisplayDamageTracker>(manager_.surface_manager(),
                                                   &aggregator_)) {
    manager_.RegisterFrameSinkId(kRootFrameSinkId, false);
    manager_.RegisterBeginFrameSource(&fake_begin_frame_source_,
                                      kRootFrameSinkId);
    damage_tracker_->SetNewRootSurface(root_client_.surface_id());
  }

  ~DisplayDamageTrackerTest() override {
    manager_.UnregisterBeginFrameSource(&fake_begin_frame_source_);
  }

 protected:
  class Client {
   public:
    Client(FrameSinkManagerImpl* manager, FrameSinkId frame_sink_id)
        : frame_sink_id_(frame_sink_id),
          support_(
              std::make_unique<CompositorFrameSinkSupport>(&client_,
                                                           manager,
                                                           frame_sink_id,
                                                           /*is_root=*/true)) {
      MakeNewSurfaceId();
      support_->SetNeedsBeginFrame(true);
    }

    SurfaceId MakeNewSurfaceId() {
      id_allocator_.GenerateId();
      local_surface_id_ = id_allocator_.GetCurrentLocalSurfaceId();
      return SurfaceId(frame_sink_id_, local_surface_id_);
    }

    SurfaceId surface_id() {
      return SurfaceId(frame_sink_id_, local_surface_id_);
    }

    void SetUpAutoNeedsBeginFrame() {
      support_->SetNeedsBeginFrame(false);
      support_->SetAutoNeedsBeginFrame();
    }

    void SubmitCompositorFrame(const BeginFrameId& frame_id) {
      CompositorRenderPassList pass_list;
      auto pass = CompositorRenderPass::Create();
      pass->output_rect = gfx::Rect(0, 0, 100, 100);
      pass->damage_rect = gfx::Rect(10, 10, 1, 1);
      pass->id = CompositorRenderPassId{1u};
      pass_list.push_back(std::move(pass));

      BeginFrameAck ack;
      ack.frame_id = frame_id;
      ack.has_damage = true;

      CompositorFrame frame = CompositorFrameBuilder()
                                  .SetRenderPassList(std::move(pass_list))
                                  .SetBeginFrameAck(ack)
                                  .Build();

      support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
    }

    MockCompositorFrameSinkClient client_;
    FrameSinkId frame_sink_id_;
    LocalSurfaceId local_surface_id_;
    std::unique_ptr<CompositorFrameSinkSupport> support_;
    ParentLocalSurfaceIdAllocator id_allocator_;
  };

  void TickBeginFrame() {
    // Ack previous surfaces
    damage_tracker_->RunDrawCallbacks();

    last_begin_frame_args_ =
        fake_begin_frame_source_.CreateBeginFrameArgs(BEGINFRAME_FROM_HERE);
    fake_begin_frame_source_.TestOnBeginFrame(last_begin_frame_args_);
  }

  ServerSharedBitmapManager shared_bitmap_manager_;
  gpu::SharedImageManager shared_image_manager_;
  gpu::SyncPointManager sync_point_manager_;
  gpu::Scheduler gpu_scheduler_{&sync_point_manager_};
  FrameSinkManagerImpl manager_;
  DisplayResourceProviderSoftware resource_provider_;
  SurfaceAggregator aggregator_;
  Client root_client_;
  scoped_refptr<base::NullTaskRunner> task_runner_;
  FakeExternalBeginFrameSource fake_begin_frame_source_;
  std::unique_ptr<DisplayDamageTracker> damage_tracker_;
  BeginFrameArgs last_begin_frame_args_;
};

TEST_F(DisplayDamageTrackerTest, Basic) {
  TickBeginFrame();

  // Check that we don't have root surface
  EXPECT_TRUE(damage_tracker_->IsRootSurfaceValid());
  EXPECT_TRUE(damage_tracker_->root_frame_missing());
  EXPECT_FALSE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));

  // Submit root surface and check that we have root surface and no pending
  // surfaces
  root_client_.SubmitCompositorFrame(last_begin_frame_args_.frame_id);

  EXPECT_FALSE(damage_tracker_->root_frame_missing());
  EXPECT_FALSE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));

  // Begin next frame
  TickBeginFrame();

  EXPECT_TRUE(damage_tracker_->IsRootSurfaceValid());
  EXPECT_FALSE(damage_tracker_->root_frame_missing());
  EXPECT_TRUE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));

  // Submit CF and check that we have root surface and no pending surfaces
  root_client_.SubmitCompositorFrame(last_begin_frame_args_.frame_id);
  EXPECT_FALSE(damage_tracker_->root_frame_missing());
  EXPECT_FALSE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));
}

TEST_F(DisplayDamageTrackerTest, Resize) {
  EXPECT_TRUE(damage_tracker_->root_frame_missing());

  // Submit initial frame
  TickBeginFrame();
  root_client_.SubmitCompositorFrame(last_begin_frame_args_.frame_id);

  // Expect no damage because of resize
  EXPECT_FALSE(
      damage_tracker_->expecting_root_surface_damage_because_of_resize());

  // Resize display and expect there is root frame, but with expected damage
  damage_tracker_->DisplayResized();
  EXPECT_TRUE(
      damage_tracker_->expecting_root_surface_damage_because_of_resize());
  EXPECT_FALSE(damage_tracker_->root_frame_missing());

  // Submit next frame
  TickBeginFrame();
  root_client_.SubmitCompositorFrame(last_begin_frame_args_.frame_id);

  // Expecting no damage
  EXPECT_FALSE(
      damage_tracker_->expecting_root_surface_damage_because_of_resize());
  EXPECT_FALSE(damage_tracker_->root_frame_missing());
}

TEST_F(DisplayDamageTrackerTest, NewRoot) {
  EXPECT_TRUE(damage_tracker_->root_frame_missing());

  // Submit initial frame
  TickBeginFrame();
  root_client_.SubmitCompositorFrame(last_begin_frame_args_.frame_id);

  // Root shouldn't be missing and no pending surfaces
  EXPECT_FALSE(damage_tracker_->root_frame_missing());
  EXPECT_FALSE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));

  // Set new surface and expect root is missing
  damage_tracker_->SetNewRootSurface(root_client_.MakeNewSurfaceId());
  EXPECT_TRUE(damage_tracker_->root_frame_missing());
  EXPECT_FALSE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));

  // Submit new root
  TickBeginFrame();
  root_client_.SubmitCompositorFrame(last_begin_frame_args_.frame_id);

  // Check that root is in place and no surface pending.
  EXPECT_FALSE(damage_tracker_->root_frame_missing());
  EXPECT_FALSE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));
}

TEST_F(DisplayDamageTrackerTest, TwoSurfaces) {
  Client embedded_client(&manager_, kChildFrameSinkId);
  manager_.RegisterFrameSinkHierarchy(kRootFrameSinkId, kChildFrameSinkId);

  EXPECT_TRUE(damage_tracker_->root_frame_missing());

  // Submit initial frame
  TickBeginFrame();
  root_client_.SubmitCompositorFrame(last_begin_frame_args_.frame_id);
  embedded_client.SubmitCompositorFrame(last_begin_frame_args_.frame_id);

  // Expect no pending surfaces
  EXPECT_FALSE(damage_tracker_->root_frame_missing());
  EXPECT_FALSE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));

  // Begin next frame and check submission in order embedded_client, root_client
  TickBeginFrame();
  EXPECT_TRUE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));

  embedded_client.SubmitCompositorFrame(last_begin_frame_args_.frame_id);
  EXPECT_TRUE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));

  root_client_.SubmitCompositorFrame(last_begin_frame_args_.frame_id);
  EXPECT_FALSE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));

  // Begin next frame and check submission in order root_client, embedded_client
  TickBeginFrame();
  EXPECT_TRUE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));

  root_client_.SubmitCompositorFrame(last_begin_frame_args_.frame_id);
  EXPECT_TRUE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));

  embedded_client.SubmitCompositorFrame(last_begin_frame_args_.frame_id);
  EXPECT_FALSE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));

  manager_.UnregisterFrameSinkHierarchy(kRootFrameSinkId, kChildFrameSinkId);
}

TEST_F(DisplayDamageTrackerTest, UnsolicitedFrameUnderAutoNeedsBeginFrame) {
  root_client_.SetUpAutoNeedsBeginFrame();

  // Ensure that when submitting the unsolicited frame below, a new
  // BeginFrameArgs is used.
  TickBeginFrame();

  // Submit an unsolicited frame.
  root_client_.SubmitCompositorFrame(
      BeginFrameAck::CreateManualAckWithDamage().frame_id);

  // Send ACK for the frame.
  damage_tracker_->RunDrawCallbacks();

  EXPECT_TRUE(damage_tracker_->IsRootSurfaceValid());
  EXPECT_FALSE(damage_tracker_->root_frame_missing());

  // Verify that the corresponding surface is not considered as pending.
  EXPECT_FALSE(damage_tracker_->HasPendingSurfaces(last_begin_frame_args_));
}

}  // namespace viz
