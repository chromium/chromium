// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_saved_frame_storage.h"
#include "components/viz/service/transitions/surface_animation_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {
namespace {

#define EXPECT_BETWEEN(lower, value, upper) \
  EXPECT_LT(value, upper);                  \
  EXPECT_GT(value, lower);

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

class SurfaceAnimationManagerTest : public testing::Test {
 public:
  void SetUp() override {
    current_time_ = base::TimeTicks() + base::Days(1);
    surface_manager_ = frame_sink_manager_.surface_manager();
    support_ = std::make_unique<CompositorFrameSinkSupport>(
        nullptr, &frame_sink_manager_, kArbitraryFrameSinkId, /*is_root=*/true);

    LocalSurfaceId local_surface_id(6, base::UnguessableToken::Create());
    surface_id_ = SurfaceId(kArbitraryFrameSinkId, local_surface_id);
    CompositorFrame frame = MakeDefaultCompositorFrame();
    support_->SubmitCompositorFrame(local_surface_id, std::move(frame));

    manager_.emplace(&shared_bitmap_manager_);
    manager_->SetDirectiveFinishedCallback(base::DoNothing());
    manager_->UpdateFrameTime(current_time_);
  }

  void TearDown() override {
    storage()->ExpireForTesting();
    manager_.reset();
  }

  base::TimeTicks AdvanceTime(base::TimeDelta delta) {
    current_time_ += delta;
    return current_time_;
  }

  Surface* surface() {
    Surface* surface = surface_manager_->GetSurfaceForId(surface_id_);
    // Can't ASSERT in a non-void function, so just CHECK instead.
    CHECK(surface);
    return surface;
  }

  SurfaceSavedFrameStorage* storage() {
    return manager().GetSurfaceSavedFrameStorageForTesting();
  }

  void ValidateStartState(CompositorFrameTransitionDirective::Effect effect) {
    switch (effect) {
      case CompositorFrameTransitionDirective::Effect::kRevealRight:
      case CompositorFrameTransitionDirective::Effect::kRevealLeft:
      case CompositorFrameTransitionDirective::Effect::kRevealUp:
      case CompositorFrameTransitionDirective::Effect::kRevealDown:
      case CompositorFrameTransitionDirective::Effect::kExplode:
      case CompositorFrameTransitionDirective::Effect::kFade:
        EXPECT_EQ(manager().root_animation_.src_opacity(), 1.0f)
            << static_cast<int>(effect);
        break;
      case CompositorFrameTransitionDirective::Effect::kNone:
        EXPECT_EQ(manager().root_animation_.src_opacity(), 0.0f)
            << static_cast<int>(effect);
        break;
      default:
        EXPECT_EQ(manager().root_animation_.dst_opacity(), 0.0f)
            << static_cast<int>(effect);
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
        EXPECT_TRUE(
            manager().root_animation_.src_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        EXPECT_TRUE(
            manager().root_animation_.dst_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        break;
      case CompositorFrameTransitionDirective::Effect::kCoverDown:
      case CompositorFrameTransitionDirective::Effect::kCoverLeft:
      case CompositorFrameTransitionDirective::Effect::kCoverRight:
      case CompositorFrameTransitionDirective::Effect::kCoverUp:
        EXPECT_TRUE(
            manager().root_animation_.src_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        EXPECT_FALSE(
            manager().root_animation_.dst_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        EXPECT_TRUE(manager()
                        .root_animation_.dst_transform()
                        .Apply()
                        .IsIdentityOr2DTranslation());
        break;
      case CompositorFrameTransitionDirective::Effect::kImplode:
        EXPECT_TRUE(
            manager().root_animation_.src_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        EXPECT_FALSE(
            manager().root_animation_.dst_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        EXPECT_TRUE(manager()
                        .root_animation_.dst_transform()
                        .Apply()
                        .IsScaleOrTranslation())
            << static_cast<int>(effect);
        break;
      default:
        break;
    }
  }

  void ValidateEndState(CompositorFrameTransitionDirective::Effect effect) {
    EXPECT_EQ(manager().root_animation_.dst_opacity(), 1.0f);
    EXPECT_TRUE(manager().root_animation_.dst_transform().Apply().IsIdentity());

    switch (effect) {
      case CompositorFrameTransitionDirective::Effect::kRevealRight:
      case CompositorFrameTransitionDirective::Effect::kRevealLeft:
      case CompositorFrameTransitionDirective::Effect::kRevealUp:
      case CompositorFrameTransitionDirective::Effect::kRevealDown:
      case CompositorFrameTransitionDirective::Effect::kExplode:
      case CompositorFrameTransitionDirective::Effect::kFade:
        EXPECT_EQ(manager().root_animation_.src_opacity(), 0.0f)
            << static_cast<int>(effect);
        break;
      default:
        break;
    }

    switch (effect) {
      case CompositorFrameTransitionDirective::Effect::kRevealDown:
      case CompositorFrameTransitionDirective::Effect::kRevealLeft:
      case CompositorFrameTransitionDirective::Effect::kRevealRight:
      case CompositorFrameTransitionDirective::Effect::kRevealUp:
        EXPECT_FALSE(
            manager().root_animation_.src_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        EXPECT_TRUE(manager()
                        .root_animation_.src_transform()
                        .Apply()
                        .IsIdentityOr2DTranslation())
            << static_cast<int>(effect);
        break;
      case CompositorFrameTransitionDirective::Effect::kExplode:
        EXPECT_FALSE(
            manager().root_animation_.src_transform().Apply().IsIdentity())
            << static_cast<int>(effect);
        EXPECT_TRUE(manager()
                        .root_animation_.src_transform()
                        .Apply()
                        .IsScaleOrTranslation())
            << static_cast<int>(effect);
        break;
      default:
        break;
    }
  }

  SurfaceAnimationManager& manager() { return *manager_; }

 protected:
  base::TimeTicks current_time_;

  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl frame_sink_manager_{
      FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_)};
  raw_ptr<SurfaceManager> surface_manager_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  SurfaceId surface_id_;

  absl::optional<SurfaceAnimationManager> manager_;
};

TEST_F(SurfaceAnimationManagerTest, DefaultState) {
  EXPECT_FALSE(manager().NeedsBeginFrame());

  manager().ProcessTransitionDirectives({}, surface());

  EXPECT_FALSE(manager().NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, SaveAnimateNeedsBeginFrame) {
  EXPECT_FALSE(manager().NeedsBeginFrame());

  manager().ProcessTransitionDirectives(CreateSaveDirectiveAsVector(1),
                                        surface());

  storage()->CompleteForTesting();

  manager().ProcessTransitionDirectives(CreateAnimateDirectiveAsVector(2),
                                        surface());

  // Tick curves to set start time.
  manager().UpdateFrameTime(AdvanceTime(base::TimeDelta()));
  manager().NotifyFrameAdvanced();

  EXPECT_TRUE(manager().NeedsBeginFrame());

  manager().UpdateFrameTime(AdvanceTime(base::Milliseconds(50)));
  manager().NotifyFrameAdvanced();
  EXPECT_TRUE(manager().NeedsBeginFrame());

  manager().UpdateFrameTime(AdvanceTime(base::Milliseconds(500)));
  manager().NotifyFrameAdvanced();
  // We should be at the done state, but still need a frame.
  EXPECT_TRUE(manager().NeedsBeginFrame());

  manager().UpdateFrameTime(AdvanceTime(base::Milliseconds(1)));
  manager().NotifyFrameAdvanced();
  EXPECT_FALSE(manager().NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, AnimateWithoutSaveIsNoop) {
  EXPECT_FALSE(manager().NeedsBeginFrame());

  manager().ProcessTransitionDirectives(CreateAnimateDirectiveAsVector(1),
                                        surface());
  EXPECT_FALSE(manager().NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, SaveTimesOut) {
  EXPECT_FALSE(manager().NeedsBeginFrame());

  manager().ProcessTransitionDirectives(CreateSaveDirectiveAsVector(1),
                                        surface());
  EXPECT_FALSE(manager().NeedsBeginFrame());

  storage()->ExpireForTesting();

  AdvanceTime(base::Seconds(6));
  manager().ProcessTransitionDirectives(CreateAnimateDirectiveAsVector(2),
                                        surface());
  EXPECT_FALSE(manager().NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, RepeatedSavesAreOk) {
  EXPECT_FALSE(manager().NeedsBeginFrame());

  uint32_t sequence_id = 1;
  for (int i = 0; i < 200; ++i) {
    manager().ProcessTransitionDirectives(
        CreateSaveDirectiveAsVector(sequence_id), surface());

    EXPECT_FALSE(manager().NeedsBeginFrame());

    ++sequence_id;
    manager().UpdateFrameTime(AdvanceTime(base::Milliseconds(50)));
  }

  storage()->CompleteForTesting();

  manager().ProcessTransitionDirectives(
      CreateAnimateDirectiveAsVector(sequence_id), surface());

  // Tick curves to set start time.
  manager().UpdateFrameTime(AdvanceTime(base::TimeDelta()));
  manager().NotifyFrameAdvanced();

  EXPECT_TRUE(manager().NeedsBeginFrame());

  manager().UpdateFrameTime(AdvanceTime(base::Milliseconds(500)));
  manager().NotifyFrameAdvanced();
  // We're at the done state now.
  EXPECT_TRUE(manager().NeedsBeginFrame());

  manager().UpdateFrameTime(AdvanceTime(base::Milliseconds(1)));
  manager().NotifyFrameAdvanced();
  // Now we're idle.
  EXPECT_FALSE(manager().NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, CheckStartEndStates) {
  EXPECT_FALSE(manager().NeedsBeginFrame());

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
    manager().ProcessTransitionDirectives(
        CreateSaveDirectiveAsVector(sequence_id++, effect), surface());

    storage()->CompleteForTesting();

    manager().ProcessTransitionDirectives(
        CreateAnimateDirectiveAsVector(sequence_id++), surface());

    // Tick curves to set start time.
    manager().UpdateFrameTime(AdvanceTime(base::TimeDelta()));
    manager().NotifyFrameAdvanced();

    ValidateStartState(effect);

    EXPECT_TRUE(manager().NeedsBeginFrame());

    manager().UpdateFrameTime(AdvanceTime(base::Milliseconds(200)));
    manager().NotifyFrameAdvanced();
    EXPECT_TRUE(manager().NeedsBeginFrame());

    manager().UpdateFrameTime(AdvanceTime(base::Milliseconds(200)));
    manager().NotifyFrameAdvanced();
    // We should be at the done state, but still need a frame.
    EXPECT_TRUE(manager().NeedsBeginFrame());

    manager().UpdateFrameTime(AdvanceTime(base::Milliseconds(1)));
    manager().NotifyFrameAdvanced();
    EXPECT_FALSE(manager().NeedsBeginFrame());

    ValidateEndState(effect);
  }
}

}  // namespace viz
