// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
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

std::vector<CompositorFrameTransitionDirective> CreateSaveDirectiveAsVector(
    uint32_t sequence_id,
    CompositorFrameTransitionDirective::Effect effect =
        CompositorFrameTransitionDirective::Effect::kCoverDown) {
  std::vector<CompositorFrameTransitionDirective> result;
  result.emplace_back(sequence_id,
                      CompositorFrameTransitionDirective::Type::kSave, effect);
  return result;
}

std::vector<CompositorFrameTransitionDirective> CreateAnimateDirectiveAsVector(
    uint32_t sequence_id) {
  std::vector<CompositorFrameTransitionDirective> result;
  result.emplace_back(sequence_id,
                      CompositorFrameTransitionDirective::Type::kAnimate);
  return result;
}

}  // namespace

class TestSurfaceAnimationManager : public SurfaceAnimationManager {
 public:
  TestSurfaceAnimationManager() = default;
  ~TestSurfaceAnimationManager() override = default;

  void ValidateStartState(CompositorFrameTransitionDirective::Effect effect) {
    switch (effect) {
      case CompositorFrameTransitionDirective::Effect::kRevealRight:
      case CompositorFrameTransitionDirective::Effect::kRevealLeft:
      case CompositorFrameTransitionDirective::Effect::kRevealUp:
      case CompositorFrameTransitionDirective::Effect::kRevealDown:
      case CompositorFrameTransitionDirective::Effect::kExplode:
      case CompositorFrameTransitionDirective::Effect::kFade:
        EXPECT_EQ(src_opacity(), 1.0f) << static_cast<int>(effect);
        break;
      case CompositorFrameTransitionDirective::Effect::kNone:
        EXPECT_EQ(src_opacity(), 0.0f) << static_cast<int>(effect);
        break;
      default:
        EXPECT_EQ(dst_opacity(), 0.0f) << static_cast<int>(effect);
        break;
    }

    switch (effect) {
      case CompositorFrameTransitionDirective::Effect::kNone:
      case CompositorFrameTransitionDirective::Effect::kFade:
      case CompositorFrameTransitionDirective::Effect::kExplode:
      case CompositorFrameTransitionDirective::Effect::kRevealDown:
      case CompositorFrameTransitionDirective::Effect::kRevealLeft:
      case CompositorFrameTransitionDirective::Effect::kRevealRight:
      case CompositorFrameTransitionDirective::Effect::kRevealUp:
        EXPECT_TRUE(src_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        EXPECT_TRUE(dst_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        break;
      case CompositorFrameTransitionDirective::Effect::kCoverDown:
      case CompositorFrameTransitionDirective::Effect::kCoverLeft:
      case CompositorFrameTransitionDirective::Effect::kCoverRight:
      case CompositorFrameTransitionDirective::Effect::kCoverUp:
        EXPECT_TRUE(src_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        EXPECT_FALSE(dst_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        EXPECT_TRUE(dst_transform().Apply().IsIdentityOr2DTranslation());
        break;
      case CompositorFrameTransitionDirective::Effect::kImplode:
        EXPECT_TRUE(src_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        EXPECT_FALSE(dst_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        EXPECT_TRUE(dst_transform().Apply().IsScaleOrTranslation())
            << static_cast<int>(effect);
        break;
      default:
        break;
    }
  }
  void ValidateEndState(CompositorFrameTransitionDirective::Effect effect) {
    EXPECT_EQ(dst_opacity(), 1.0f);
    EXPECT_TRUE(dst_transform().Apply().IsIdentity());

    switch (effect) {
      case CompositorFrameTransitionDirective::Effect::kRevealRight:
      case CompositorFrameTransitionDirective::Effect::kRevealLeft:
      case CompositorFrameTransitionDirective::Effect::kRevealUp:
      case CompositorFrameTransitionDirective::Effect::kRevealDown:
      case CompositorFrameTransitionDirective::Effect::kExplode:
      case CompositorFrameTransitionDirective::Effect::kFade:
        EXPECT_EQ(src_opacity(), 0.0f) << static_cast<int>(effect);
        break;
      default:
        break;
    }

    switch (effect) {
      case CompositorFrameTransitionDirective::Effect::kRevealDown:
      case CompositorFrameTransitionDirective::Effect::kRevealLeft:
      case CompositorFrameTransitionDirective::Effect::kRevealRight:
      case CompositorFrameTransitionDirective::Effect::kRevealUp:
        EXPECT_FALSE(src_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        EXPECT_TRUE(src_transform().Apply().IsIdentityOr2DTranslation())
            << static_cast<int>(effect);
        break;
      case CompositorFrameTransitionDirective::Effect::kExplode:
        EXPECT_FALSE(src_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        EXPECT_TRUE(src_transform().Apply().IsScaleOrTranslation())
            << static_cast<int>(effect);
        break;
      default:
        break;
    }
  }
};

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
  TestSurfaceAnimationManager manager;
  manager.SetDirectiveFinishedCallback(base::DoNothing());
  EXPECT_FALSE(manager.NeedsBeginFrame());

  manager.ProcessTransitionDirectives({}, storage());

  EXPECT_FALSE(manager.NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, SaveAnimateNeedsBeginFrame) {
  TestSurfaceAnimationManager manager;
  manager.SetDirectiveFinishedCallback(base::DoNothing());
  EXPECT_FALSE(manager.NeedsBeginFrame());

  manager.ProcessTransitionDirectives(CreateSaveDirectiveAsVector(1),
                                      storage());

  storage()->CompleteForTesting();

  manager.ProcessTransitionDirectives(CreateAnimateDirectiveAsVector(2),
                                      storage());

  // Tick curves to set start time.
  manager.NotifyFrameAdvanced(AdvanceTime(base::TimeDelta()));

  EXPECT_TRUE(manager.NeedsBeginFrame());

  manager.NotifyFrameAdvanced(
      AdvanceTime(base::TimeDelta::FromMilliseconds(50)));
  EXPECT_TRUE(manager.NeedsBeginFrame());

  manager.NotifyFrameAdvanced(
      AdvanceTime(base::TimeDelta::FromMilliseconds(500)));
  // We should be at the done state, but still need a frame.
  EXPECT_TRUE(manager.NeedsBeginFrame());

  manager.NotifyFrameAdvanced(
      AdvanceTime(base::TimeDelta::FromMilliseconds(1)));
  EXPECT_FALSE(manager.NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, AnimateWithoutSaveIsNoop) {
  TestSurfaceAnimationManager manager;
  manager.SetDirectiveFinishedCallback(base::DoNothing());
  EXPECT_FALSE(manager.NeedsBeginFrame());

  manager.ProcessTransitionDirectives(CreateAnimateDirectiveAsVector(1),
                                      storage());
  EXPECT_FALSE(manager.NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, SaveTimesOut) {
  TestSurfaceAnimationManager manager;
  manager.SetDirectiveFinishedCallback(base::DoNothing());
  EXPECT_FALSE(manager.NeedsBeginFrame());

  manager.ProcessTransitionDirectives(CreateSaveDirectiveAsVector(1),
                                      storage());
  EXPECT_FALSE(manager.NeedsBeginFrame());

  storage()->ExpireForTesting();

  AdvanceTime(base::TimeDelta::FromSeconds(6));
  manager.ProcessTransitionDirectives(CreateAnimateDirectiveAsVector(2),
                                      storage());
  EXPECT_FALSE(manager.NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, RepeatedSavesAreOk) {
  TestSurfaceAnimationManager manager;
  manager.SetDirectiveFinishedCallback(base::DoNothing());
  EXPECT_FALSE(manager.NeedsBeginFrame());

  uint32_t sequence_id = 1;
  for (int i = 0; i < 200; ++i) {
    manager.ProcessTransitionDirectives(
        CreateSaveDirectiveAsVector(sequence_id), storage());

    EXPECT_FALSE(manager.NeedsBeginFrame());

    ++sequence_id;
    AdvanceTime(base::TimeDelta::FromMilliseconds(50));
  }

  storage()->CompleteForTesting();

  manager.ProcessTransitionDirectives(
      CreateAnimateDirectiveAsVector(sequence_id), storage());

  // Tick curves to set start time.
  manager.NotifyFrameAdvanced(AdvanceTime(base::TimeDelta()));

  EXPECT_TRUE(manager.NeedsBeginFrame());

  manager.NotifyFrameAdvanced(
      AdvanceTime(base::TimeDelta::FromMilliseconds(500)));
  // We're at the done state now.
  EXPECT_TRUE(manager.NeedsBeginFrame());

  manager.NotifyFrameAdvanced(
      AdvanceTime(base::TimeDelta::FromMilliseconds(1)));
  // Now we're idle.
  EXPECT_FALSE(manager.NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, CheckStartEndStates) {
  TestSurfaceAnimationManager manager;
  manager.SetDirectiveFinishedCallback(base::DoNothing());
  EXPECT_FALSE(manager.NeedsBeginFrame());

  CompositorFrameTransitionDirective::Effect effects[] = {
      CompositorFrameTransitionDirective::Effect::kNone,
      CompositorFrameTransitionDirective::Effect::kCoverDown,
      CompositorFrameTransitionDirective::Effect::kCoverLeft,
      CompositorFrameTransitionDirective::Effect::kCoverRight,
      CompositorFrameTransitionDirective::Effect::kCoverUp,
      CompositorFrameTransitionDirective::Effect::kExplode,
      CompositorFrameTransitionDirective::Effect::kFade,
      CompositorFrameTransitionDirective::Effect::kImplode,
      CompositorFrameTransitionDirective::Effect::kRevealDown,
      CompositorFrameTransitionDirective::Effect::kRevealLeft,
      CompositorFrameTransitionDirective::Effect::kRevealRight,
      CompositorFrameTransitionDirective::Effect::kRevealUp};

  uint32_t sequence_id = 1;
  for (auto effect : effects) {
    manager.ProcessTransitionDirectives(
        CreateSaveDirectiveAsVector(sequence_id++, effect), storage());

    storage()->CompleteForTesting();

    manager.ProcessTransitionDirectives(
        CreateAnimateDirectiveAsVector(sequence_id++), storage());

    // Tick curves to set start time.
    manager.NotifyFrameAdvanced(AdvanceTime(base::TimeDelta()));

    manager.ValidateStartState(effect);

    EXPECT_TRUE(manager.NeedsBeginFrame());

    manager.NotifyFrameAdvanced(
        AdvanceTime(base::TimeDelta::FromMilliseconds(250)));
    EXPECT_TRUE(manager.NeedsBeginFrame());

    manager.NotifyFrameAdvanced(
        AdvanceTime(base::TimeDelta::FromMilliseconds(250)));
    // We should be at the done state, but still need a frame.
    EXPECT_TRUE(manager.NeedsBeginFrame());

    manager.NotifyFrameAdvanced(
        AdvanceTime(base::TimeDelta::FromMilliseconds(1)));
    EXPECT_FALSE(manager.NeedsBeginFrame());

    manager.ValidateEndState(effect);
  }
}

}  // namespace viz
