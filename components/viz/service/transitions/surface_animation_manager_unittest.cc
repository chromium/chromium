// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/time/time.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_saved_frame_storage.h"
#include "components/viz/service/transitions/surface_animation_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

CompositorFrameTransitionDirective CreateSaveDirective(
    uint32_t sequence_id,
    base::TimeDelta duration) {
  return CompositorFrameTransitionDirective(
      sequence_id, CompositorFrameTransitionDirective::Type::kSave,
      CompositorFrameTransitionDirective::Effect::kCoverDown, duration);
}

CompositorFrameTransitionDirective CreateAnimateDirective(
    uint32_t sequence_id) {
  return CompositorFrameTransitionDirective(
      sequence_id, CompositorFrameTransitionDirective::Type::kAnimate);
}

}  // namespace

class SurfaceAnimationManagerTest : public testing::Test {
 public:
  void SetUp() override {
    current_time_ = base::TimeTicks() + base::TimeDelta::FromDays(1);
    surface_manager_ = frame_sink_manager_.surface_manager();
    support_ = std::make_unique<CompositorFrameSinkSupport>(
        nullptr, &frame_sink_manager_, kArbitraryFrameSinkId, /*is_root=*/true);

    LocalSurfaceId local_surface_id(6, base::UnguessableToken::Create());
    surface_id_ = SurfaceId(kArbitraryFrameSinkId, local_surface_id);
    CompositorFrame frame = MakeDefaultCompositorFrame();
    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
  }

  void TearDown() override {
    if (storage())
      storage()->ExpireForTesting();
  }

  base::TimeTicks AdvanceTime(base::TimeDelta delta) {
    current_time_ += delta;
    return current_time_;
  }

  base::TimeTicks current_time() const { return current_time_; }

  Surface* surface() const {
    Surface* surface = surface_manager_->GetSurfaceForId(surface_id_);
    // Can't ASSERT in a non-void function, so just CHECK instead.
    CHECK(surface);
    return surface;
  }

  SurfaceSavedFrameStorage* storage() const {
    return surface()->GetSurfaceSavedFrameStorage();
  }

 private:
  base::TimeTicks current_time_;

  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl frame_sink_manager_{&shared_bitmap_manager_};
  SurfaceManager* surface_manager_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  SurfaceId surface_id_;
};

TEST_F(SurfaceAnimationManagerTest, DefaultState) {
  SurfaceAnimationManager manager;
  EXPECT_FALSE(manager.NeedsBeginFrame());

  manager.ProcessTransitionDirectives(current_time(), {}, storage());

  EXPECT_FALSE(manager.NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, SaveAnimateNeedsBeginFrame) {
  SurfaceAnimationManager manager;
  EXPECT_FALSE(manager.NeedsBeginFrame());

  manager.ProcessTransitionDirectives(
      current_time(),
      {CreateSaveDirective(1, base::TimeDelta::FromMilliseconds(100))},
      storage());

  storage()->CompleteForTesting();

  manager.ProcessTransitionDirectives(current_time(),
                                      {CreateAnimateDirective(2)}, storage());

  EXPECT_TRUE(manager.NeedsBeginFrame());

  manager.NotifyFrameAdvanced(
      AdvanceTime(base::TimeDelta::FromMilliseconds(50)));
  EXPECT_TRUE(manager.NeedsBeginFrame());

  manager.NotifyFrameAdvanced(
      AdvanceTime(base::TimeDelta::FromMilliseconds(50)));
  // We should be at the done state, but still need a frame.
  EXPECT_TRUE(manager.NeedsBeginFrame());

  manager.NotifyFrameAdvanced(
      AdvanceTime(base::TimeDelta::FromMilliseconds(1)));
  EXPECT_FALSE(manager.NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, AnimateWithoutSaveIsNoop) {
  SurfaceAnimationManager manager;
  EXPECT_FALSE(manager.NeedsBeginFrame());

  manager.ProcessTransitionDirectives(current_time(),
                                      {CreateAnimateDirective(2)}, storage());
  EXPECT_FALSE(manager.NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, SaveTimesOut) {
  SurfaceAnimationManager manager;
  EXPECT_FALSE(manager.NeedsBeginFrame());

  manager.ProcessTransitionDirectives(
      current_time(),
      {CreateSaveDirective(1, base::TimeDelta::FromMilliseconds(100))},
      storage());
  EXPECT_FALSE(manager.NeedsBeginFrame());

  storage()->ExpireForTesting();

  manager.ProcessTransitionDirectives(
      AdvanceTime(base::TimeDelta::FromSeconds(6)), {CreateAnimateDirective(2)},
      storage());
  EXPECT_FALSE(manager.NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, RepeatedSavesAreOk) {
  SurfaceAnimationManager manager;
  EXPECT_FALSE(manager.NeedsBeginFrame());

  uint32_t sequence_id = 1;
  for (int i = 0; i < 200; ++i) {
    manager.ProcessTransitionDirectives(
        current_time(),
        {CreateSaveDirective(sequence_id,
                             base::TimeDelta::FromMilliseconds(100))},
        storage());

    EXPECT_FALSE(manager.NeedsBeginFrame());

    ++sequence_id;
    AdvanceTime(base::TimeDelta::FromMilliseconds(50));
  }

  storage()->CompleteForTesting();

  manager.ProcessTransitionDirectives(
      current_time(), {CreateAnimateDirective(sequence_id)}, storage());
  EXPECT_TRUE(manager.NeedsBeginFrame());

  manager.NotifyFrameAdvanced(
      AdvanceTime(base::TimeDelta::FromMilliseconds(100)));
  // We're at the done state now.
  EXPECT_TRUE(manager.NeedsBeginFrame());

  manager.NotifyFrameAdvanced(
      AdvanceTime(base::TimeDelta::FromMilliseconds(1)));
  // Now we're idle.
  EXPECT_FALSE(manager.NeedsBeginFrame());
}

}  // namespace viz
