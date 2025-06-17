// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/keyframe_model.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/target_property.h"
#include "components/viz/service/layers/layer_context_impl.h"
#include "components/viz/service/layers/layer_context_impl_base_unittest.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

namespace viz {
namespace {

class LayerContextImplAnimationTest : public LayerContextImplTest {
 protected:
  mojom::TimingFunctionPtr CreateDefaultMojomTimingFunction() {
    auto cubic_bezier =
        mojom::CubicBezierTimingFunction::New(0.25, 0.1, 0.25, 1.0);
    return mojom::TimingFunction::NewCubicBezier(std::move(cubic_bezier));
  }

  mojom::AnimationKeyframePtr CreateDefaultMojomScalarKeyframe(
      base::TimeDelta start_time,
      float value) {
    auto keyframe = mojom::AnimationKeyframe::New();
    keyframe->start_time = start_time;
    keyframe->value = mojom::AnimationKeyframeValue::NewScalar(value);
    keyframe->timing_function = CreateDefaultMojomTimingFunction();
    return keyframe;
  }

  mojom::AnimationKeyframeModelPtr CreateDefaultOpacityMojomKeyframeModel(
      int model_id,
      int group_id) {
    auto model = mojom::AnimationKeyframeModel::New();
    model->id = model_id;
    model->group_id = group_id;
    model->target_property_type =
        static_cast<int32_t>(cc::TargetProperty::OPACITY);
    model->timing_function = CreateDefaultMojomTimingFunction();
    model->scaled_duration = 1.0;
    model->keyframes.push_back(
        CreateDefaultMojomScalarKeyframe(base::TimeDelta(), 0.0f));
    model->keyframes.push_back(
        CreateDefaultMojomScalarKeyframe(base::Seconds(1), 1.0f));
    model->direction = mojom::AnimationDirection::kNormal;
    model->fill_mode = mojom::AnimationFillMode::kForwards;
    model->playback_rate = 1.0;
    model->iterations = 1.0;
    model->iteration_start = 0.0;
    model->time_offset = base::TimeDelta();
    model->element_id = cc::ElementId(123);  // Arbitrary element ID
    return model;
  }

  mojom::AnimationPtr CreateDefaultMojomAnimation(int animation_id,
                                                  int model_id,
                                                  int group_id) {
    auto animation = mojom::Animation::New();
    animation->id = animation_id;
    animation->element_id = cc::ElementId(123);  // Arbitrary element ID
    animation->keyframe_models.push_back(
        CreateDefaultOpacityMojomKeyframeModel(model_id, group_id));
    return animation;
  }

  mojom::AnimationTimelinePtr CreateDefaultMojomTimeline(int timeline_id) {
    auto timeline = mojom::AnimationTimeline::New();
    timeline->id = timeline_id;
    return timeline;
  }

  cc::AnimationHost* GetAnimationHost() {
    return static_cast<cc::AnimationHost*>(
        layer_context_impl_->host_impl()->active_tree()->mutator_host());
  }
};

TEST_F(LayerContextImplAnimationTest, AddNewAnimationTimelineAndAnimation) {
  constexpr int kTimelineId = 1;
  constexpr int kAnimationId = 10;
  constexpr int kKeyframeModelId = 100;
  constexpr int kGroupId = 1;
  const cc::ElementId kElementId(123);

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  timeline_mojom->new_animations.push_back(
      CreateDefaultMojomAnimation(kAnimationId, kKeyframeModelId, kGroupId));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::AnimationHost* host = GetAnimationHost();
  ASSERT_NE(nullptr, host);

  cc::AnimationTimeline* timeline_impl = host->GetTimelineById(kTimelineId);
  ASSERT_NE(nullptr, timeline_impl);

  cc::Animation* animation_impl = timeline_impl->GetAnimationById(kAnimationId);
  ASSERT_NE(nullptr, animation_impl);
  EXPECT_EQ(animation_impl->id(), kAnimationId);
  EXPECT_EQ(animation_impl->element_id(), kElementId);

  cc::KeyframeEffect* effect_impl = animation_impl->keyframe_effect();
  ASSERT_NE(nullptr, effect_impl);
  ASSERT_EQ(effect_impl->keyframe_models().size(), 1u);

  gfx::KeyframeModel* gfx_keyframe_model =
      effect_impl->keyframe_models()[0].get();
  ASSERT_NE(nullptr, gfx_keyframe_model);
  cc::KeyframeModel* cc_keyframe_model =
      cc::KeyframeModel::ToCcKeyframeModel(gfx_keyframe_model);
  ASSERT_NE(nullptr, cc_keyframe_model);

  EXPECT_EQ(gfx_keyframe_model->id(), kKeyframeModelId);
  EXPECT_EQ(cc_keyframe_model->group(), kGroupId);
  EXPECT_EQ(cc_keyframe_model->TargetProperty(),
            static_cast<int32_t>(cc::TargetProperty::OPACITY));
  EXPECT_EQ(cc_keyframe_model->element_id(), kElementId);
}

TEST_F(LayerContextImplAnimationTest, AddAnimationToExistingTimeline) {
  constexpr int kTimelineId = 2;
  constexpr int kAnimationId1 = 20;
  constexpr int kKeyframeModelId1 = 200;
  constexpr int kGroupId1 = 2;
  constexpr int kAnimationId2 = 21;
  constexpr int kKeyframeModelId2 = 201;
  constexpr int kGroupId2 = 2;

  // First, create the timeline with one animation.
  auto update1 = CreateDefaultUpdate();
  update1->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();
  auto timeline_mojom1 = CreateDefaultMojomTimeline(kTimelineId);
  timeline_mojom1->new_animations.push_back(
      CreateDefaultMojomAnimation(kAnimationId1, kKeyframeModelId1, kGroupId1));
  update1->animation_timelines->push_back(std::move(timeline_mojom1));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  cc::AnimationHost* host = GetAnimationHost();
  cc::AnimationTimeline* timeline_impl = host->GetTimelineById(kTimelineId);
  ASSERT_NE(nullptr, timeline_impl);
  EXPECT_NE(nullptr, timeline_impl->GetAnimationById(kAnimationId1));
  EXPECT_EQ(nullptr, timeline_impl->GetAnimationById(kAnimationId2));

  // Second, add another animation to the same timeline.
  auto update2 = CreateDefaultUpdate();
  update2->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();
  auto timeline_mojom2 =
      CreateDefaultMojomTimeline(kTimelineId);  // Existing ID
  timeline_mojom2->new_animations.push_back(
      CreateDefaultMojomAnimation(kAnimationId2, kKeyframeModelId2, kGroupId2));
  update2->animation_timelines->push_back(std::move(timeline_mojom2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());

  ASSERT_NE(nullptr, timeline_impl->GetAnimationById(kAnimationId1));
  cc::Animation* animation2_impl =
      timeline_impl->GetAnimationById(kAnimationId2);
  ASSERT_NE(nullptr, animation2_impl);
  EXPECT_EQ(animation2_impl->id(), kAnimationId2);
}

TEST_F(LayerContextImplAnimationTest, UpdateExistingAnimationFails) {
  constexpr int kTimelineId = 3;
  constexpr int kAnimationId = 30;
  constexpr int kKeyframeModelId = 300;
  constexpr int kGroupId = 3;

  // First, create the timeline with an animation.
  auto update1 = CreateDefaultUpdate();
  update1->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();
  auto timeline_mojom1 = CreateDefaultMojomTimeline(kTimelineId);
  timeline_mojom1->new_animations.push_back(
      CreateDefaultMojomAnimation(kAnimationId, kKeyframeModelId, kGroupId));
  update1->animation_timelines->push_back(std::move(timeline_mojom1));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  // Second, attempt to add an animation with the same ID to the same timeline.
  auto update2 = CreateDefaultUpdate();
  update2->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();
  auto timeline_mojom2 =
      CreateDefaultMojomTimeline(kTimelineId);  // Existing ID
  // Create a new animation object but with the same ID.
  timeline_mojom2->new_animations.push_back(CreateDefaultMojomAnimation(
      kAnimationId, kKeyframeModelId + 1, kGroupId));
  update2->animation_timelines->push_back(std::move(timeline_mojom2));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update2));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Unexpected duplicate animation ID");

  // Verify the original animation is still there.
  cc::AnimationHost* host = GetAnimationHost();
  cc::AnimationTimeline* timeline_impl = host->GetTimelineById(kTimelineId);
  ASSERT_NE(nullptr, timeline_impl);
  cc::Animation* animation_impl = timeline_impl->GetAnimationById(kAnimationId);
  ASSERT_NE(nullptr, animation_impl);
  // Check if it's the original model by its ID.
  // id() is on gfx::KeyframeModel, so no cast is strictly needed here.
  ASSERT_EQ(animation_impl->keyframe_effect()->keyframe_models().size(), 1u);
  EXPECT_EQ(animation_impl->keyframe_effect()->keyframe_models()[0]->id(),
            kKeyframeModelId);
}

TEST_F(LayerContextImplAnimationTest, RemoveAnimationFromTimeline) {
  constexpr int kTimelineId = 4;
  constexpr int kAnimationId = 40;
  constexpr int kKeyframeModelId = 400;
  constexpr int kGroupId = 4;
  constexpr int kNonExistentAnimationId = 41;

  // First, create the timeline with an animation.
  auto update1 = CreateDefaultUpdate();
  update1->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();
  auto timeline_mojom1 = CreateDefaultMojomTimeline(kTimelineId);
  timeline_mojom1->new_animations.push_back(
      CreateDefaultMojomAnimation(kAnimationId, kKeyframeModelId, kGroupId));
  update1->animation_timelines->push_back(std::move(timeline_mojom1));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  cc::AnimationHost* host = GetAnimationHost();
  cc::AnimationTimeline* timeline_impl = host->GetTimelineById(kTimelineId);
  ASSERT_NE(nullptr, timeline_impl);
  EXPECT_NE(nullptr, timeline_impl->GetAnimationById(kAnimationId));

  // Second, remove the animation. Also try removing a non-existent one.
  auto update2 = CreateDefaultUpdate();
  update2->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();
  auto timeline_mojom2 =
      CreateDefaultMojomTimeline(kTimelineId);  // Existing ID
  timeline_mojom2->removed_animations.push_back(kAnimationId);
  timeline_mojom2->removed_animations.push_back(kNonExistentAnimationId);
  update2->animation_timelines->push_back(std::move(timeline_mojom2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());

  EXPECT_EQ(nullptr, timeline_impl->GetAnimationById(kAnimationId));
}

TEST_F(LayerContextImplAnimationTest, RemoveAnimationTimeline) {
  constexpr int kTimelineId = 5;
  constexpr int kNonExistentTimelineId = 6;

  // First, create the timeline.
  auto update1 = CreateDefaultUpdate();
  update1->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();
  update1->animation_timelines->push_back(
      CreateDefaultMojomTimeline(kTimelineId));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  cc::AnimationHost* host = GetAnimationHost();
  ASSERT_NE(nullptr, host);
  EXPECT_NE(nullptr, host->GetTimelineById(kTimelineId));

  // Second, remove the timeline. Also try removing a non-existent one.
  auto update2 = CreateDefaultUpdate();
  update2->removed_animation_timelines = std::vector<int32_t>();
  update2->removed_animation_timelines->push_back(kTimelineId);
  update2->removed_animation_timelines->push_back(kNonExistentTimelineId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());

  EXPECT_EQ(nullptr, host->GetTimelineById(kTimelineId));
}

TEST_F(LayerContextImplAnimationTest, AnimationWithNoKeyframesFails) {
  constexpr int kTimelineId = 7;
  constexpr int kAnimationId = 70;
  const cc::ElementId kElementId(777);
  constexpr int kKeyframeModelId = 700;
  constexpr int kGroupId = 7;
  constexpr double kScaledDuration = 1.0;

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom = mojom::Animation::New();
  animation_mojom->id = kAnimationId;
  animation_mojom->element_id = kElementId;
  // Add a keyframe model but leave its keyframes vector empty.
  auto keyframe_model_mojom = mojom::AnimationKeyframeModel::New();
  keyframe_model_mojom->id = kKeyframeModelId;
  keyframe_model_mojom->group_id = kGroupId;
  keyframe_model_mojom->target_property_type =
      static_cast<int32_t>(cc::TargetProperty::OPACITY);
  keyframe_model_mojom->timing_function = CreateDefaultMojomTimingFunction();
  keyframe_model_mojom->scaled_duration = kScaledDuration;
  // keyframes is intentionally empty.
  animation_mojom->keyframe_models.push_back(std::move(keyframe_model_mojom));

  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Unexpected animation with no keyframes");
}

}  // namespace
}  // namespace viz
