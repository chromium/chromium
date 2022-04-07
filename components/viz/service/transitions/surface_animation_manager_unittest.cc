// Copyright 2021 The Chromium Authors. All rights reserved.
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
                      CompositorFrameTransitionDirective::Type::kSave, false,
                      effect);
  return result;
}

std::vector<CompositorFrameTransitionDirective> CreateAnimateDirectiveAsVector(
    uint32_t sequence_id) {
  std::vector<CompositorFrameTransitionDirective> result;
  result.emplace_back(sequence_id,
                      CompositorFrameTransitionDirective::Type::kAnimate);
  return result;
}

std::vector<CompositorFrameTransitionDirective::SharedElement>
CreateSharedElements(
    const CompositorRenderPassList& render_pass_list,
    std::vector<CompositorFrameTransitionDirective::TransitionConfig> configs =
        {}) {
  CHECK(configs.empty() || configs.size() == render_pass_list.size() - 1);

  std::vector<CompositorFrameTransitionDirective::SharedElement> elements(
      render_pass_list.size() - 1);
  for (size_t i = 0; i < elements.size(); i++) {
    elements[i].render_pass_id = render_pass_list[i]->id;

    if (!configs.empty())
      elements[i].config = configs[i];
  }

  return elements;
}

CompositorFrame CreateFrameWithSharedElement(
    gfx::RectF rect = gfx::RectF(100, 100),
    gfx::Transform transform = gfx::Transform(),
    float opacity = 1.f) {
  gfx::Rect output_rect = gfx::ToEnclosingRect(rect);
  auto frame = CompositorFrameBuilder()
                   .AddRenderPass(output_rect, output_rect)
                   .AddDefaultRenderPass()
                   .Build();
  auto* root_pass = frame.render_pass_list.back().get();
  auto* sqs = root_pass->CreateAndAppendSharedQuadState();
  auto* rpdq =
      root_pass->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
  rpdq->material = DrawQuad::Material::kCompositorRenderPass;
  rpdq->render_pass_id = frame.render_pass_list.front()->id;
  rpdq->shared_quad_state = sqs;

  frame.render_pass_list.front()->transform_to_root_target = transform;
  sqs->opacity = opacity;
  return frame;
}

void DispatchCopyResult(CopyOutputRequest& request, gfx::Size size) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(size.width(), size.height(),
                                                SkColorSpace::MakeSRGB()));
  auto result = std::make_unique<CopyOutputSkBitmapResult>(gfx::Rect(size),
                                                           std::move(bitmap));
  request.SendResult(std::move(result));
  base::RunLoop().RunUntilIdle();
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

TEST_F(SurfaceAnimationManagerTest, ConfigWithAllZeroDurations) {
  EXPECT_FALSE(manager().NeedsBeginFrame());

  auto frame = CreateFrameWithSharedElement();

  CompositorFrameTransitionDirective::TransitionConfig zero_config;
  zero_config.duration = zero_config.delay = base::TimeDelta();
  CompositorFrameTransitionDirective save(
      1, CompositorFrameTransitionDirective::Type::kSave, false,
      CompositorFrameTransitionDirective::Effect::kCoverDown,
      /*root_config=*/zero_config,
      /*shared_elements=*/
      CreateSharedElements(frame.render_pass_list, {zero_config}));

  CompositorFrameTransitionDirective animate(
      2, CompositorFrameTransitionDirective::Type::kAnimate, false,
      CompositorFrameTransitionDirective::Effect::kNone,
      /*root_config=*/zero_config,
      /*shared_elements=*/
      CreateSharedElements(frame.render_pass_list, {zero_config}));

  support_->SubmitCompositorFrame(surface_id_.local_surface_id(),
                                  std::move(frame));

  ASSERT_FALSE(manager().ProcessTransitionDirectives({save}, surface()));

  storage()->CompleteForTesting();

  ASSERT_TRUE(manager().ProcessTransitionDirectives({animate}, surface()));

  // We jump directly to the last frame but we should need a BeginFrame to tick
  // the last frame.
  EXPECT_TRUE(manager().NeedsBeginFrame());

  // Tick curves to set start time.
  manager().UpdateFrameTime(AdvanceTime(base::TimeDelta()));
  manager().NotifyFrameAdvanced();

  EXPECT_FALSE(manager().NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, CustomRootConfig) {
  EXPECT_FALSE(manager().NeedsBeginFrame());

  CompositorFrameTransitionDirective::TransitionConfig root_config;
  root_config.duration = base::Seconds(1);
  root_config.delay = base::Seconds(1);

  CompositorFrameTransitionDirective save(
      1, CompositorFrameTransitionDirective::Type::kSave, false,
      CompositorFrameTransitionDirective::Effect::kExplode,
      /*root_config=*/root_config,
      /*shared_elements=*/{});

  CompositorFrameTransitionDirective animate(
      2, CompositorFrameTransitionDirective::Type::kAnimate, false,
      CompositorFrameTransitionDirective::Effect::kNone,
      /*root_config=*/root_config,
      /*shared_elements=*/{});

  ASSERT_FALSE(manager().ProcessTransitionDirectives({save}, surface()));
  storage()->CompleteForTesting();
  ASSERT_TRUE(manager().ProcessTransitionDirectives({animate}, surface()));

  // Need the first frame which starts the animation.
  EXPECT_TRUE(manager().NeedsBeginFrame());
  manager().UpdateFrameTime(AdvanceTime(base::TimeDelta()));
  manager().NotifyFrameAdvanced();

  // Verify the initial state. Since the effect is explode the src will scale up
  // and fade out revealing destination. The fade out is delayed with respect to
  // the scaling.
  // The whole animation wil be delayed by the config.
  EXPECT_EQ(manager().root_animation_.src_opacity(), 1.f);
  EXPECT_EQ(manager().root_animation_.dst_opacity(), 1.f);
  EXPECT_TRUE(manager().root_animation_.src_transform().Apply().IsIdentity());
  EXPECT_TRUE(manager().root_animation_.dst_transform().Apply().IsIdentity());

  // Tick up to delay and we should still be at the initial state.
  EXPECT_TRUE(manager().NeedsBeginFrame());
  manager().UpdateFrameTime(AdvanceTime(root_config.delay));
  manager().NotifyFrameAdvanced();
  EXPECT_EQ(manager().root_animation_.src_opacity(), 1.f);
  EXPECT_EQ(manager().root_animation_.dst_opacity(), 1.f);
  EXPECT_TRUE(manager().root_animation_.src_transform().Apply().IsIdentity());
  EXPECT_TRUE(manager().root_animation_.dst_transform().Apply().IsIdentity());

  // Tick a small value to start the transform animation but not opacity.
  auto only_transform_delay = base::Milliseconds(100);
  EXPECT_TRUE(manager().NeedsBeginFrame());
  manager().UpdateFrameTime(AdvanceTime(only_transform_delay));
  manager().NotifyFrameAdvanced();
  EXPECT_EQ(manager().root_animation_.src_opacity(), 1.f);
  EXPECT_EQ(manager().root_animation_.dst_opacity(), 1.f);
  EXPECT_FALSE(manager().root_animation_.src_transform().Apply().IsIdentity());
  EXPECT_TRUE(
      manager().root_animation_.src_transform().Apply().IsScaleOrTranslation());
  EXPECT_TRUE(manager().root_animation_.dst_transform().Apply().IsIdentity());

  // Now tick midway through the transition, we should have an opacity animation
  // as well.
  EXPECT_TRUE(manager().NeedsBeginFrame());
  auto midway_delay = (root_config.duration / 2) - only_transform_delay;
  manager().UpdateFrameTime(AdvanceTime(midway_delay));
  manager().NotifyFrameAdvanced();
  EXPECT_BETWEEN(0.f, manager().root_animation_.src_opacity(), 1.f);
  EXPECT_EQ(manager().root_animation_.dst_opacity(), 1.f);
  EXPECT_FALSE(manager().root_animation_.src_transform().Apply().IsIdentity());
  EXPECT_TRUE(
      manager().root_animation_.src_transform().Apply().IsScaleOrTranslation());
  EXPECT_TRUE(manager().root_animation_.dst_transform().Apply().IsIdentity());

  // Jump to the end of the transition.
  EXPECT_TRUE(manager().NeedsBeginFrame());
  manager().UpdateFrameTime(AdvanceTime(root_config.duration / 2));
  manager().NotifyFrameAdvanced();
  EXPECT_EQ(manager().root_animation_.src_opacity(), 0.f);
  EXPECT_EQ(manager().root_animation_.dst_opacity(), 1.f);
  EXPECT_FALSE(manager().root_animation_.src_transform().Apply().IsIdentity());
  EXPECT_TRUE(
      manager().root_animation_.src_transform().Apply().IsScaleOrTranslation());
  EXPECT_TRUE(manager().root_animation_.dst_transform().Apply().IsIdentity());

  // The above should've been the last frame. One more tick and the transition
  // ends.
  EXPECT_TRUE(manager().NeedsBeginFrame());
  manager().UpdateFrameTime(AdvanceTime(base::TimeDelta()));
  manager().NotifyFrameAdvanced();
  EXPECT_FALSE(manager().NeedsBeginFrame());
}

TEST_F(SurfaceAnimationManagerTest, CustomSharedConfig) {
  EXPECT_FALSE(manager().NeedsBeginFrame());

  const gfx::SizeF old_size(100, 100);
  auto old_frame = CreateFrameWithSharedElement(gfx::RectF(old_size));
  auto root_size = old_frame.render_pass_list.back()->output_rect.size();
  auto element_id = old_frame.render_pass_list.front()->id;

  CompositorFrameTransitionDirective::TransitionConfig zero_config;
  zero_config.duration = zero_config.delay = base::TimeDelta();

  CompositorFrameTransitionDirective::TransitionConfig shared_config;
  shared_config.duration = base::Seconds(1);
  shared_config.delay = base::Seconds(1);

  CompositorFrameTransitionDirective save(
      1, CompositorFrameTransitionDirective::Type::kSave, false,
      CompositorFrameTransitionDirective::Effect::kNone,
      /*root_config=*/zero_config,
      /*shared_elements=*/
      CreateSharedElements(old_frame.render_pass_list, {shared_config}));

  support_->SubmitCompositorFrame(surface_id_.local_surface_id(),
                                  std::move(old_frame));
  ASSERT_FALSE(manager().ProcessTransitionDirectives({save}, surface()));

  // Dispatch copy request results. We're not using the test function here to
  // ensure valid result for shared elements.
  Surface::CopyRequestsMap copy_requests;
  surface()->TakeCopyOutputRequests(&copy_requests);
  ASSERT_EQ(copy_requests.size(), 2u);
  for (auto& it : copy_requests) {
    if (it.first == element_id) {
      DispatchCopyResult(*it.second, gfx::ToRoundedSize(old_size));
    } else {
      DispatchCopyResult(*it.second, root_size);
    }
  }

  gfx::SizeF new_size(200, 200);
  gfx::Transform new_transform;
  new_transform.Translate(10, 10);
  float new_opacity = 0.6f;
  auto new_frame = CreateFrameWithSharedElement(gfx::RectF(new_size),
                                                new_transform, new_opacity);

  CompositorFrameTransitionDirective animate(
      2, CompositorFrameTransitionDirective::Type::kAnimate, false,
      CompositorFrameTransitionDirective::Effect::kNone,
      /*root_config=*/zero_config,
      /*shared_elements=*/
      CreateSharedElements(new_frame.render_pass_list, {shared_config}));
  support_->SubmitCompositorFrame(surface_id_.local_surface_id(),
                                  std::move(new_frame));
  ASSERT_TRUE(manager().ProcessTransitionDirectives({animate}, surface()));

  // Need the first frame which starts the animation.
  EXPECT_TRUE(manager().NeedsBeginFrame());
  manager().UpdateFrameTime(AdvanceTime(base::TimeDelta()));
  manager().NotifyFrameAdvanced();
  manager().InterpolateFrame(surface());

  ASSERT_EQ(manager().shared_animations_.size(), 1u);
  const auto& shared_animation = manager().shared_animations_[0];

  // Verify the initial state.
  EXPECT_EQ(shared_animation.content_opacity(), 0.f);
  EXPECT_EQ(shared_animation.content_size(), old_size);
  EXPECT_EQ(shared_animation.combined_opacity(), 1.f);
  EXPECT_TRUE(shared_animation.combined_transform().Apply().IsIdentity());

  // Tick up to delay and we should still be at the initial state.
  EXPECT_TRUE(manager().NeedsBeginFrame());
  manager().UpdateFrameTime(AdvanceTime(shared_config.delay));
  manager().NotifyFrameAdvanced();
  EXPECT_EQ(shared_animation.content_opacity(), 0.f);
  EXPECT_EQ(shared_animation.content_size(), old_size);
  EXPECT_EQ(shared_animation.combined_opacity(), 1.f);
  EXPECT_TRUE(shared_animation.combined_transform().Apply().IsIdentity());

  float frame_points[] = {0.2, 0.4, 0.9, 1.0};

  // Tick a small value to start the transform and size animations but not
  // opacity.
  EXPECT_TRUE(manager().NeedsBeginFrame());
  manager().UpdateFrameTime(
      AdvanceTime(shared_config.duration * frame_points[0]));
  manager().NotifyFrameAdvanced();
  EXPECT_EQ(shared_animation.content_opacity(), 0.f);
  EXPECT_BETWEEN(old_size.width(), shared_animation.content_size().width(),
                 new_size.width());
  EXPECT_BETWEEN(old_size.height(), shared_animation.content_size().height(),
                 new_size.height());
  EXPECT_EQ(shared_animation.combined_opacity(), 1.f);
  EXPECT_FALSE(shared_animation.combined_transform().Apply().IsIdentity());
  EXPECT_NE(shared_animation.combined_transform().Apply(), new_transform);

  // Now tick to a point where we have transform, size and opacity animations.
  EXPECT_TRUE(manager().NeedsBeginFrame());
  manager().UpdateFrameTime(AdvanceTime(shared_config.duration *
                                        (frame_points[1] - frame_points[0])));
  manager().NotifyFrameAdvanced();
  EXPECT_BETWEEN(0.f, shared_animation.content_opacity(), 1.f);
  EXPECT_BETWEEN(old_size.width(), shared_animation.content_size().width(),
                 new_size.width());
  EXPECT_BETWEEN(old_size.height(), shared_animation.content_size().height(),
                 new_size.height());
  EXPECT_BETWEEN(new_opacity, shared_animation.combined_opacity(), 1.f);
  EXPECT_FALSE(shared_animation.combined_transform().Apply().IsIdentity());
  EXPECT_NE(shared_animation.combined_transform().Apply(), new_transform);

  // Opacity animations finish sooner than transform and size animations.
  EXPECT_TRUE(manager().NeedsBeginFrame());
  manager().UpdateFrameTime(AdvanceTime(shared_config.duration *
                                        (frame_points[2] - frame_points[1])));
  manager().NotifyFrameAdvanced();
  EXPECT_EQ(shared_animation.content_opacity(), 1.f);
  EXPECT_BETWEEN(old_size.width(), shared_animation.content_size().width(),
                 new_size.width());
  EXPECT_BETWEEN(old_size.height(), shared_animation.content_size().height(),
                 new_size.height());
  EXPECT_EQ(shared_animation.combined_opacity(), new_opacity);
  EXPECT_FALSE(shared_animation.combined_transform().Apply().IsIdentity());
  EXPECT_NE(shared_animation.combined_transform().Apply(), new_transform);

  // Jump to the end of the transition.
  EXPECT_TRUE(manager().NeedsBeginFrame());
  manager().UpdateFrameTime(AdvanceTime(shared_config.duration *
                                        (frame_points[3] - frame_points[2])));
  manager().NotifyFrameAdvanced();
  EXPECT_EQ(shared_animation.content_opacity(), 1.f);
  EXPECT_EQ(shared_animation.content_size(), new_size);
  EXPECT_EQ(shared_animation.combined_opacity(), new_opacity);
  EXPECT_EQ(shared_animation.combined_transform().Apply(), new_transform);

  // Trigger the last frame to end the animation.
  EXPECT_TRUE(manager().NeedsBeginFrame());
  manager().UpdateFrameTime(AdvanceTime(base::Milliseconds(16)));
  manager().NotifyFrameAdvanced();
  manager().UpdateFrameTime(AdvanceTime(base::TimeDelta()));
  manager().NotifyFrameAdvanced();
  EXPECT_FALSE(manager().NeedsBeginFrame());
}

}  // namespace viz
