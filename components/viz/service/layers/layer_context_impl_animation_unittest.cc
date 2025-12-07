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
  // Default TimingFunction values
  static constexpr double kDefaultCubicBezierX1 = 0.25;
  static constexpr double kDefaultCubicBezierY1 = 0.1;
  static constexpr double kDefaultCubicBezierX2 = 0.25;
  static constexpr double kDefaultCubicBezierY2 = 1.0;
  static constexpr uint32_t kDefaultSteps = 5;
  static constexpr mojom::TimingStepPosition kDefaultStepPosition =
      mojom::TimingStepPosition::kStart;

  // Default Keyframe values
  static constexpr base::TimeDelta kDefaultKeyframeStartTime =
      base::TimeDelta();
  static constexpr float kDefaultKeyframeStartOpacity = 0.0f;
  static constexpr base::TimeDelta kDefaultKeyframeEndOpacityTime =
      base::Seconds(1);
  static constexpr float kDefaultKeyframeEndOpacity = 1.0f;

  // Default KeyframeModel values
  static constexpr double kDefaultScaledDuration = 1.0;
  static constexpr double kDefaultPlaybackRate = 1.0;
  static constexpr double kDefaultIterations = 1.0;
  static constexpr double kDefaultIterationStart = 0.0;
  static const cc::ElementId kDefaultElementId;

  mojom::TimingFunctionPtr CreateDefaultMojomTimingFunction() {
    auto cubic_bezier = mojom::CubicBezierTimingFunction::New(
        kDefaultCubicBezierX1, kDefaultCubicBezierY1, kDefaultCubicBezierX2,
        kDefaultCubicBezierY2);
    return mojom::TimingFunction::NewCubicBezier(std::move(cubic_bezier));
  }

  mojom::TimingFunctionPtr CreateMojomLinearTimingFunction() {
    return mojom::TimingFunction::NewLinear(
        std::vector<mojom::LinearEasingPointPtr>());
  }

  mojom::TimingFunctionPtr CreateMojomStepsTimingFunction(
      uint32_t steps = kDefaultSteps,
      mojom::TimingStepPosition position = kDefaultStepPosition) {
    auto steps_fn = mojom::StepsTimingFunction::New();
    steps_fn->num_steps = steps;
    steps_fn->step_position = position;
    return mojom::TimingFunction::NewSteps(std::move(steps_fn));
  }

  mojom::AnimationKeyframePtr CreateDefaultMojomScalarKeyframe(
      base::TimeDelta start_time,
      float value,
      mojom::TimingFunctionPtr timing_function = nullptr) {
    auto keyframe = mojom::AnimationKeyframe::New();
    keyframe->start_time = start_time;
    keyframe->value = mojom::AnimationKeyframeValue::NewScalar(value);
    keyframe->timing_function = timing_function
                                    ? std::move(timing_function)
                                    : CreateDefaultMojomTimingFunction();
    return keyframe;
  }

  // Helper function to create a AnimationKeyframeModel with common defaults.
  mojom::AnimationKeyframeModelPtr CreateDefaultMojomKeyframeModel(
      int model_id,
      int group_id,
      cc::TargetProperty::Type target_property_type) {
    auto model = mojom::AnimationKeyframeModel::New();
    model->id = model_id;
    model->group_id = group_id;
    model->target_property_type = static_cast<int32_t>(target_property_type);
    model->timing_function = CreateDefaultMojomTimingFunction();
    model->scaled_duration = kDefaultScaledDuration;
    model->direction = mojom::AnimationDirection::kNormal;
    model->fill_mode = mojom::AnimationFillMode::kForwards;
    model->playback_rate = kDefaultPlaybackRate;
    model->iterations = kDefaultIterations;
    model->iteration_start = kDefaultIterationStart;
    model->time_offset = base::TimeDelta();
    model->element_id = kDefaultElementId;
    // Add default keyframes for opacity as a common case.
    if (target_property_type == cc::TargetProperty::OPACITY) {
      model->keyframes.push_back(CreateDefaultMojomScalarKeyframe(
          kDefaultKeyframeStartTime, kDefaultKeyframeStartOpacity));
      model->keyframes.push_back(CreateDefaultMojomScalarKeyframe(
          kDefaultKeyframeEndOpacityTime, kDefaultKeyframeEndOpacity));
    }
    return model;
  }

  mojom::AnimationKeyframePtr CreateMojomColorKeyframe(
      base::TimeDelta start_time,
      SkColor value,
      mojom::TimingFunctionPtr timing_function = nullptr) {
    auto keyframe = mojom::AnimationKeyframe::New();
    keyframe->start_time = start_time;
    keyframe->value = mojom::AnimationKeyframeValue::NewColor(value);
    keyframe->timing_function = timing_function
                                    ? std::move(timing_function)
                                    : CreateDefaultMojomTimingFunction();
    return keyframe;
  }

  mojom::AnimationKeyframePtr CreateMojomSizeKeyframe(
      base::TimeDelta start_time,
      const gfx::SizeF& value,
      mojom::TimingFunctionPtr timing_function = nullptr) {
    auto keyframe = mojom::AnimationKeyframe::New();
    keyframe->start_time = start_time;
    keyframe->value = mojom::AnimationKeyframeValue::NewSize(value);
    keyframe->timing_function = timing_function
                                    ? std::move(timing_function)
                                    : CreateDefaultMojomTimingFunction();
    return keyframe;
  }

  mojom::AnimationKeyframePtr CreateMojomRectKeyframe(
      base::TimeDelta start_time,
      const gfx::Rect& value,
      mojom::TimingFunctionPtr timing_function = nullptr) {
    auto keyframe = mojom::AnimationKeyframe::New();
    keyframe->start_time = start_time;
    keyframe->value = mojom::AnimationKeyframeValue::NewRect(value);
    keyframe->timing_function = timing_function
                                    ? std::move(timing_function)
                                    : CreateDefaultMojomTimingFunction();
    return keyframe;
  }

  mojom::AnimationKeyframePtr CreateMojomTransformKeyframe(
      base::TimeDelta start_time,
      const gfx::TransformOperations& value,
      mojom::TimingFunctionPtr timing_function = nullptr) {
    auto keyframe = mojom::AnimationKeyframe::New();
    keyframe->start_time = start_time;
    std::vector<mojom::TransformOperationPtr> ops_mojom;
    for (size_t i = 0; i < value.size(); ++i) {
      const auto& op = value.at(i);
      switch (op.type) {
        case gfx::TransformOperation::TRANSFORM_OPERATION_TRANSLATE:
          ops_mojom.push_back(mojom::TransformOperation::NewTranslate(
              gfx::Vector3dF(op.translate.x, op.translate.y, op.translate.z)));
          break;
        case gfx::TransformOperation::TRANSFORM_OPERATION_ROTATE:
          ops_mojom.push_back(
              mojom::TransformOperation::NewRotate(mojom::AxisAngle::New(
                  gfx::Vector3dF(op.rotate.axis.x, op.rotate.axis.y,
                                 op.rotate.axis.z),
                  op.rotate.angle)));
          break;
        case gfx::TransformOperation::TRANSFORM_OPERATION_SCALE:
          ops_mojom.push_back(mojom::TransformOperation::NewScale(
              gfx::Vector3dF(op.scale.x, op.scale.y, op.scale.z)));
          break;
        case gfx::TransformOperation::TRANSFORM_OPERATION_IDENTITY:
          // Actual bool value true/false is meaningless. It is only important
          // that the operation has union type 'identity'.
          ops_mojom.push_back(mojom::TransformOperation::NewIdentity(true));
          break;
        case gfx::TransformOperation::TRANSFORM_OPERATION_PERSPECTIVE:
          ops_mojom.push_back(mojom::TransformOperation::NewPerspectiveDepth(
              op.perspective_m43 ? -1.0f / op.perspective_m43 : 0.0f));
          break;
        case gfx::TransformOperation::TRANSFORM_OPERATION_SKEW:
          ops_mojom.push_back(mojom::TransformOperation::NewSkew(
              gfx::Vector2dF(op.skew.x, op.skew.y)));
          break;
        case gfx::TransformOperation::TRANSFORM_OPERATION_MATRIX:
          ops_mojom.push_back(mojom::TransformOperation::NewMatrix(op.matrix));
          break;
        default:
          NOTREACHED();
      }
    }
    keyframe->value =
        mojom::AnimationKeyframeValue::NewTransform(std::move(ops_mojom));
    keyframe->timing_function = timing_function
                                    ? std::move(timing_function)
                                    : CreateDefaultMojomTimingFunction();
    return keyframe;
  }

  mojom::AnimationKeyframePtr CreateMojomTransformKeyframeDefault(
      base::TimeDelta start_time) {
    gfx::TransformOperations transform;
    transform.AppendTranslate(1.0, 1.0, 1.0);  // Default non-identity transform
    return CreateMojomTransformKeyframe(start_time, transform,
                                        CreateDefaultMojomTimingFunction());
  }

  mojom::AnimationKeyframePtr CreateMojomColorKeyframeDefault(
      base::TimeDelta start_time) {
    return CreateMojomColorKeyframe(start_time, SK_ColorGREEN,
                                    CreateDefaultMojomTimingFunction());
  }

  mojom::AnimationPtr CreateAnimationWithDefaultModel(
      int animation_id,
      int model_id,
      int group_id,
      cc::TargetProperty::Type target_property_type) {
    auto animation = mojom::Animation::New();
    animation->id = animation_id;
    animation->element_id = kDefaultElementId;
    animation->keyframe_models.push_back(CreateDefaultMojomKeyframeModel(
        model_id, group_id, target_property_type));
    return animation;
  }

  mojom::AnimationPtr CreateDefaultMojomAnimation(int animation_id,
                                                  int model_id,
                                                  int group_id) {
    return CreateAnimationWithDefaultModel(animation_id, model_id, group_id,
                                           cc::TargetProperty::OPACITY);
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

const cc::ElementId LayerContextImplAnimationTest::kDefaultElementId(123);

TEST_F(LayerContextImplAnimationTest, AddNewAnimationTimelineAndAnimation) {
  constexpr int kTimelineId = 1;
  constexpr int kAnimationId = 10;
  constexpr int kKeyframeModelId = 100;
  constexpr int kGroupId = 1;

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
  EXPECT_EQ(animation_impl->element_id(), kDefaultElementId);

  cc::KeyframeEffect* effect_impl = animation_impl->keyframe_effect();
  ASSERT_NE(nullptr, effect_impl);
  ASSERT_EQ(effect_impl->keyframe_models().size(), 1u);

  gfx::KeyframeModel* gfx_keyframe_model =
      effect_impl->keyframe_models()[0].get();
  ASSERT_NE(nullptr, gfx_keyframe_model);
  cc::KeyframeModel* cc_keyframe_model =
      cc::KeyframeModel::ToCcKeyframeModel(gfx_keyframe_model);
  ASSERT_NE(nullptr, cc_keyframe_model);

  // Verify gfx::KeyframeModel properties
  EXPECT_EQ(gfx_keyframe_model->id(), kKeyframeModelId);
  EXPECT_EQ(gfx_keyframe_model->iterations(), kDefaultIterations);
  EXPECT_EQ(gfx_keyframe_model->iteration_start(), kDefaultIterationStart);
  EXPECT_EQ(gfx_keyframe_model->direction(),
            gfx::KeyframeModel::Direction::NORMAL);
  EXPECT_EQ(gfx_keyframe_model->fill_mode(),
            gfx::KeyframeModel::FillMode::FORWARDS);
  EXPECT_EQ(gfx_keyframe_model->playback_rate(), kDefaultPlaybackRate);
  EXPECT_EQ(gfx_keyframe_model->time_offset(), base::TimeDelta());

  // Verify cc::KeyframeModel specific properties
  EXPECT_EQ(cc_keyframe_model->group(), kGroupId);
  EXPECT_EQ(cc_keyframe_model->TargetProperty(),
            static_cast<int32_t>(cc::TargetProperty::OPACITY));
  EXPECT_EQ(cc_keyframe_model->element_id(), kDefaultElementId);

  // Verify AnimationCurve properties (via gfx::KeyframeModel)
  const gfx::AnimationCurve* curve = gfx_keyframe_model->curve();
  ASSERT_NE(nullptr, curve);
  EXPECT_EQ(curve->Type(), gfx::AnimationCurve::FLOAT);
  const auto* float_curve =
      static_cast<const gfx::KeyframedFloatAnimationCurve*>(curve);
  ASSERT_NE(nullptr, float_curve);
  EXPECT_EQ(float_curve->Duration(),
            kDefaultKeyframeEndOpacityTime - kDefaultKeyframeStartTime);
  EXPECT_DOUBLE_EQ(float_curve->scaled_duration(), kDefaultScaledDuration);
  ASSERT_NE(nullptr, float_curve->timing_function());  // Default cubic bezier
  const auto& keyframes = float_curve->keyframes();
  ASSERT_EQ(keyframes.size(), 2u);
  EXPECT_EQ(keyframes[0]->Time(), kDefaultKeyframeStartTime);
  EXPECT_EQ(keyframes[0]->Value(), kDefaultKeyframeStartOpacity);
  EXPECT_EQ(keyframes[1]->Time(), kDefaultKeyframeEndOpacityTime);
  EXPECT_EQ(keyframes[1]->Value(), kDefaultKeyframeEndOpacity);
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
  // Use an ElementId distinct from kDefaultElementId.
  const cc::ElementId kDistinctElementId(777);
  constexpr int kKeyframeModelId = 700;
  constexpr int kGroupId = 7;

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom = mojom::Animation::New();
  animation_mojom->id = kAnimationId;
  animation_mojom->element_id = kDistinctElementId;
  auto keyframe_model_mojom = CreateDefaultMojomKeyframeModel(
      kKeyframeModelId, kGroupId, cc::TargetProperty::OPACITY);
  // keyframes is intentionally empty.
  keyframe_model_mojom->keyframes.clear();
  animation_mojom->keyframe_models.push_back(std::move(keyframe_model_mojom));

  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Unexpected animation with no keyframes");
}

TEST_F(LayerContextImplAnimationTest, DeserializeColorAnimationCurve) {
  constexpr int kTimelineId = 8;
  constexpr int kAnimationId = 80;
  constexpr int kKeyframeModelId = 800;
  constexpr int kGroupId = 8;
  const SkColor kStartColor = SK_ColorRED;
  const SkColor kEndColor = SK_ColorBLUE;

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom =
      CreateAnimationWithDefaultModel(kAnimationId, kKeyframeModelId, kGroupId,
                                      cc::TargetProperty::BACKGROUND_COLOR);
  animation_mojom->keyframe_models[0]->keyframes.push_back(
      CreateMojomColorKeyframe(kDefaultKeyframeStartTime, kStartColor));
  animation_mojom->keyframe_models[0]->keyframes.push_back(
      CreateMojomColorKeyframe(kDefaultKeyframeEndOpacityTime, kEndColor));
  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::AnimationHost* host = GetAnimationHost();
  cc::AnimationTimeline* timeline_impl = host->GetTimelineById(kTimelineId);
  ASSERT_NE(nullptr, timeline_impl);
  cc::Animation* animation_impl = timeline_impl->GetAnimationById(kAnimationId);
  ASSERT_NE(nullptr, animation_impl);
  cc::KeyframeEffect* effect_impl = animation_impl->keyframe_effect();
  ASSERT_NE(nullptr, effect_impl);
  ASSERT_EQ(effect_impl->keyframe_models().size(), 1u);
  gfx::KeyframeModel* gfx_model_impl = effect_impl->keyframe_models()[0].get();
  ASSERT_NE(nullptr, gfx_model_impl);
  EXPECT_EQ(gfx_model_impl->curve()->Type(), gfx::AnimationCurve::COLOR);
  const auto* color_curve =
      static_cast<const gfx::KeyframedColorAnimationCurve*>(
          gfx_model_impl->curve());
  ASSERT_NE(nullptr, color_curve);
  ASSERT_EQ(color_curve->keyframes().size(), 2u);
  EXPECT_EQ(color_curve->keyframes()[0]->Value(), kStartColor);
  EXPECT_EQ(color_curve->keyframes()[1]->Value(), kEndColor);
}

TEST_F(LayerContextImplAnimationTest, DeserializeSizeAnimationCurve) {
  constexpr int kTimelineId = 9;
  constexpr int kAnimationId = 90;
  constexpr int kKeyframeModelId = 900;
  constexpr int kGroupId = 9;
  const gfx::SizeF kStartSize(10.f, 20.f);
  const gfx::SizeF kEndSize(30.f, 40.f);

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom = CreateAnimationWithDefaultModel(
      kAnimationId, kKeyframeModelId, kGroupId, cc::TargetProperty::BOUNDS);
  animation_mojom->keyframe_models[0]->keyframes.push_back(
      CreateMojomSizeKeyframe(kDefaultKeyframeStartTime, kStartSize));
  animation_mojom->keyframe_models[0]->keyframes.push_back(
      CreateMojomSizeKeyframe(kDefaultKeyframeEndOpacityTime, kEndSize));

  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::AnimationHost* host = GetAnimationHost();
  cc::AnimationTimeline* timeline_impl = host->GetTimelineById(kTimelineId);
  ASSERT_NE(nullptr, timeline_impl);
  cc::Animation* animation_impl = timeline_impl->GetAnimationById(kAnimationId);
  ASSERT_NE(nullptr, animation_impl);
  cc::KeyframeEffect* effect_impl = animation_impl->keyframe_effect();
  ASSERT_NE(nullptr, effect_impl);
  ASSERT_EQ(effect_impl->keyframe_models().size(), 1u);
  gfx::KeyframeModel* gfx_model_impl = effect_impl->keyframe_models()[0].get();
  ASSERT_NE(nullptr, gfx_model_impl);
  EXPECT_EQ(gfx_model_impl->curve()->Type(), gfx::AnimationCurve::SIZE);
  const auto* size_curve = static_cast<const gfx::KeyframedSizeAnimationCurve*>(
      gfx_model_impl->curve());
  ASSERT_NE(nullptr, size_curve);
  ASSERT_EQ(size_curve->keyframes().size(), 2u);
  EXPECT_EQ(size_curve->keyframes()[0]->Value(), kStartSize);
  EXPECT_EQ(size_curve->keyframes()[1]->Value(), kEndSize);
}

TEST_F(LayerContextImplAnimationTest, DeserializeRectAnimationCurve) {
  constexpr int kTimelineId = 10;
  constexpr int kAnimationId = 100;
  constexpr int kKeyframeModelId = 1000;
  constexpr int kGroupId = 10;
  const gfx::Rect kStartRect(10, 20, 30, 40);
  const gfx::Rect kEndRect(50, 60, 70, 80);

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom = CreateAnimationWithDefaultModel(
      kAnimationId, kKeyframeModelId, kGroupId, cc::TargetProperty::BOUNDS);
  animation_mojom->keyframe_models[0]->keyframes.push_back(
      CreateMojomRectKeyframe(kDefaultKeyframeStartTime, kStartRect));
  animation_mojom->keyframe_models[0]->keyframes.push_back(
      CreateMojomRectKeyframe(kDefaultKeyframeEndOpacityTime, kEndRect));

  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::AnimationHost* host = GetAnimationHost();
  cc::AnimationTimeline* timeline_impl = host->GetTimelineById(kTimelineId);
  ASSERT_NE(nullptr, timeline_impl);
  cc::Animation* animation_impl = timeline_impl->GetAnimationById(kAnimationId);
  ASSERT_NE(nullptr, animation_impl);
  cc::KeyframeEffect* effect_impl = animation_impl->keyframe_effect();
  ASSERT_NE(nullptr, effect_impl);
  ASSERT_EQ(effect_impl->keyframe_models().size(), 1u);
  gfx::KeyframeModel* gfx_model_impl = effect_impl->keyframe_models()[0].get();
  ASSERT_NE(nullptr, gfx_model_impl);
  EXPECT_EQ(gfx_model_impl->curve()->Type(), gfx::AnimationCurve::RECT);
  const auto* rect_curve = static_cast<const gfx::KeyframedRectAnimationCurve*>(
      gfx_model_impl->curve());
  ASSERT_NE(nullptr, rect_curve);
  ASSERT_EQ(rect_curve->keyframes().size(), 2u);
  EXPECT_EQ(rect_curve->keyframes()[0]->Value(), kStartRect);
  EXPECT_EQ(rect_curve->keyframes()[1]->Value(), kEndRect);
}

TEST_F(LayerContextImplAnimationTest, DeserializeTransformAnimationCurve) {
  constexpr int kTimelineId = 11;
  constexpr int kAnimationId = 110;
  constexpr int kKeyframeModelId = 1100;
  constexpr int kGroupId = 11;
  gfx::TransformOperations kStartTransform;
  kStartTransform.AppendIdentity();
  kStartTransform.AppendPerspective(1000.0);
  kStartTransform.AppendSkew(10.0, 20.0);
  kStartTransform.AppendTranslate(10.f, 20.f, 30.f);  // Already covered

  gfx::TransformOperations kEndTransform;
  kEndTransform.AppendRotate(1.0, 0.5, 0.25, 45.0);
  kEndTransform.AppendScale(2.f, 2.f, 1.f);  // Already covered
  kEndTransform.AppendMatrix(gfx::Transform::MakeScale(3.0));

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom = CreateAnimationWithDefaultModel(
      kAnimationId, kKeyframeModelId, kGroupId, cc::TargetProperty::TRANSFORM);
  animation_mojom->keyframe_models[0]->keyframes.push_back(
      CreateMojomTransformKeyframe(kDefaultKeyframeStartTime, kStartTransform));
  animation_mojom->keyframe_models[0]->keyframes.push_back(
      CreateMojomTransformKeyframe(kDefaultKeyframeEndOpacityTime,
                                   kEndTransform));

  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::AnimationHost* host = GetAnimationHost();
  cc::AnimationTimeline* timeline_impl = host->GetTimelineById(kTimelineId);
  ASSERT_NE(nullptr, timeline_impl);
  cc::Animation* animation_impl = timeline_impl->GetAnimationById(kAnimationId);
  ASSERT_NE(nullptr, animation_impl);
  cc::KeyframeEffect* effect_impl = animation_impl->keyframe_effect();
  ASSERT_NE(nullptr, effect_impl);
  ASSERT_EQ(effect_impl->keyframe_models().size(), 1u);
  gfx::KeyframeModel* gfx_model_impl = effect_impl->keyframe_models()[0].get();
  ASSERT_NE(nullptr, gfx_model_impl);
  EXPECT_EQ(gfx_model_impl->curve()->Type(), gfx::AnimationCurve::TRANSFORM);
  const auto* transform_curve =
      static_cast<const gfx::KeyframedTransformAnimationCurve*>(
          gfx_model_impl->curve());
  ASSERT_NE(nullptr, transform_curve);
  ASSERT_EQ(transform_curve->keyframes().size(), 2u);
  // Check that the deserialized transform operations match the input.
  EXPECT_TRUE(gfx::SufficientlyEqual(transform_curve->keyframes()[0]->Value(),
                                     kStartTransform));
  EXPECT_TRUE(gfx::SufficientlyEqual(transform_curve->keyframes()[1]->Value(),
                                     kEndTransform));
}

TEST_F(LayerContextImplAnimationTest, DeserializeWithLinearTimingFunction) {
  constexpr int kTimelineId = 12;
  constexpr int kAnimationId = 120;
  constexpr int kKeyframeModelId = 1200;
  constexpr int kGroupId = 12;

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom =
      CreateDefaultMojomAnimation(kAnimationId, kKeyframeModelId, kGroupId);
  // Override the keyframe model's timing function.
  animation_mojom->keyframe_models[0]->timing_function =
      CreateMojomLinearTimingFunction();
  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::AnimationHost* host = GetAnimationHost();
  cc::AnimationTimeline* timeline_impl = host->GetTimelineById(kTimelineId);
  ASSERT_NE(nullptr, timeline_impl);
  cc::Animation* animation_impl = timeline_impl->GetAnimationById(kAnimationId);
  ASSERT_NE(nullptr, animation_impl);
  cc::KeyframeEffect* effect_impl = animation_impl->keyframe_effect();
  ASSERT_NE(nullptr, effect_impl);
  ASSERT_EQ(effect_impl->keyframe_models().size(), 1u);
  gfx::KeyframeModel* gfx_model_impl = effect_impl->keyframe_models()[0].get();
  ASSERT_NE(nullptr, gfx_model_impl);
  const gfx::AnimationCurve* curve = gfx_model_impl->curve();
  ASSERT_NE(nullptr, curve);
  // Default animation is opacity (float).
  const auto* float_curve =
      static_cast<const gfx::KeyframedFloatAnimationCurve*>(curve);
  ASSERT_NE(nullptr, float_curve->timing_function());
  EXPECT_EQ(float_curve->timing_function()->GetType(),
            gfx::TimingFunction::Type::LINEAR);
}

namespace {

mojom::TimingFunctionPtr CreateMojomLinearTimingFunctionWithPoints(
    double p1_in,
    double p1_out,
    double p2_in,
    double p2_out,
    double p3_in,
    double p3_out) {
  std::vector<mojom::LinearEasingPointPtr> points_vector;
  points_vector.push_back(mojom::LinearEasingPoint::New(p1_in, p1_out));
  points_vector.push_back(mojom::LinearEasingPoint::New(p2_in, p2_out));
  points_vector.push_back(mojom::LinearEasingPoint::New(p3_in, p3_out));
  return mojom::TimingFunction::NewLinear(std::move(points_vector));
}

}  // namespace

TEST_F(LayerContextImplAnimationTest,
       DeserializeWithModelLinearTimingFunctionWithPoints) {
  constexpr int kTimelineId = 22;
  constexpr int kAnimationId = 220;
  constexpr int kKeyframeModelId = 2200;
  constexpr int kGroupId = 22;
  constexpr double kPoint1Input = 0.0;
  constexpr double kPoint1Output = 0.0;
  constexpr double kPoint2Input = 0.5;
  constexpr double kPoint2Output = 0.25;
  constexpr double kPoint3Input = 1.0;
  constexpr double kPoint3Output = 1.0;

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom =
      CreateDefaultMojomAnimation(kAnimationId, kKeyframeModelId, kGroupId);

  // Set the model's timing function to be linear with points.
  animation_mojom->keyframe_models[0]->timing_function =
      CreateMojomLinearTimingFunctionWithPoints(kPoint1Input, kPoint1Output,
                                                kPoint2Input, kPoint2Output,
                                                kPoint3Input, kPoint3Output);

  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::AnimationHost* host = GetAnimationHost();
  cc::AnimationTimeline* timeline_impl = host->GetTimelineById(kTimelineId);
  ASSERT_NE(nullptr, timeline_impl);
  cc::Animation* animation_impl = timeline_impl->GetAnimationById(kAnimationId);
  ASSERT_NE(nullptr, animation_impl);
  cc::KeyframeEffect* effect_impl = animation_impl->keyframe_effect();
  ASSERT_NE(nullptr, effect_impl);
  ASSERT_EQ(effect_impl->keyframe_models().size(), 1u);
  gfx::KeyframeModel* gfx_model_impl = effect_impl->keyframe_models()[0].get();
  ASSERT_NE(nullptr, gfx_model_impl);

  // Verify the model's curve timing function.
  const auto* float_curve =
      static_cast<const gfx::KeyframedFloatAnimationCurve*>(
          gfx_model_impl->curve());
  ASSERT_NE(nullptr, float_curve->timing_function());
  EXPECT_EQ(float_curve->timing_function()->GetType(),
            gfx::TimingFunction::Type::LINEAR);
  const auto* linear_timing_fn = static_cast<const gfx::LinearTimingFunction*>(
      float_curve->timing_function());
  ASSERT_EQ(linear_timing_fn->Points().size(), 3u);
  EXPECT_EQ(linear_timing_fn->Points()[0].input, kPoint1Input);
  EXPECT_EQ(linear_timing_fn->Points()[0].output, kPoint1Output);
  EXPECT_EQ(linear_timing_fn->Points()[1].input, kPoint2Input);
  EXPECT_EQ(linear_timing_fn->Points()[1].output, kPoint2Output);
  EXPECT_EQ(linear_timing_fn->Points()[2].input, kPoint3Input);
  EXPECT_EQ(linear_timing_fn->Points()[2].output, kPoint3Output);

  // Verify the first keyframe's timing function (should be default cubic
  // bezier).
  ASSERT_FALSE(float_curve->keyframes().empty());
  const auto* keyframe_timing_fn =
      float_curve->keyframes()[0]->timing_function();
  ASSERT_NE(nullptr, keyframe_timing_fn);
  EXPECT_EQ(keyframe_timing_fn->GetType(),
            gfx::TimingFunction::Type::CUBIC_BEZIER);
}

TEST_F(LayerContextImplAnimationTest,
       DeserializeWithKeyframeLinearTimingFunctionWithPoints) {
  constexpr int kTimelineId = 23;
  constexpr int kAnimationId = 230;
  constexpr int kKeyframeModelId = 2300;
  constexpr int kGroupId = 23;
  constexpr double kPoint1Input = 0.1;
  constexpr double kPoint1Output = 0.2;
  constexpr double kPoint2Input = 0.6;
  constexpr double kPoint2Output = 0.7;
  constexpr double kPoint3Input = 0.9;
  constexpr double kPoint3Output = 0.8;

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom =
      CreateDefaultMojomAnimation(kAnimationId, kKeyframeModelId, kGroupId);

  // The model itself uses the default timing function (cubic bezier).
  // Set the first keyframe's timing function to be linear with points.
  ASSERT_FALSE(animation_mojom->keyframe_models[0]->keyframes.empty());
  animation_mojom->keyframe_models[0]->keyframes[0]->timing_function =
      CreateMojomLinearTimingFunctionWithPoints(kPoint1Input, kPoint1Output,
                                                kPoint2Input, kPoint2Output,
                                                kPoint3Input, kPoint3Output);

  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::AnimationHost* host = GetAnimationHost();
  cc::AnimationTimeline* timeline_impl = host->GetTimelineById(kTimelineId);
  ASSERT_NE(nullptr, timeline_impl);
  cc::Animation* animation_impl = timeline_impl->GetAnimationById(kAnimationId);
  ASSERT_NE(nullptr, animation_impl);
  cc::KeyframeEffect* effect_impl = animation_impl->keyframe_effect();
  ASSERT_NE(nullptr, effect_impl);
  ASSERT_EQ(effect_impl->keyframe_models().size(), 1u);
  gfx::KeyframeModel* gfx_model_impl = effect_impl->keyframe_models()[0].get();
  ASSERT_NE(nullptr, gfx_model_impl);

  // Verify the model's curve timing function (should be default cubic bezier).
  const auto* float_curve =
      static_cast<const gfx::KeyframedFloatAnimationCurve*>(
          gfx_model_impl->curve());
  ASSERT_NE(nullptr, float_curve->timing_function());
  EXPECT_EQ(float_curve->timing_function()->GetType(),
            gfx::TimingFunction::Type::CUBIC_BEZIER);

  // Verify the first keyframe's timing function.
  ASSERT_FALSE(float_curve->keyframes().empty());
  const auto* keyframe_linear_timing_fn =
      static_cast<const gfx::LinearTimingFunction*>(
          float_curve->keyframes()[0]->timing_function());
  ASSERT_NE(nullptr, keyframe_linear_timing_fn);
  EXPECT_EQ(keyframe_linear_timing_fn->GetType(),
            gfx::TimingFunction::Type::LINEAR);
  ASSERT_EQ(keyframe_linear_timing_fn->Points().size(), 3u);
  EXPECT_EQ(keyframe_linear_timing_fn->Points()[0].input, kPoint1Input);
  EXPECT_EQ(keyframe_linear_timing_fn->Points()[0].output, kPoint1Output);
  EXPECT_EQ(keyframe_linear_timing_fn->Points()[1].input, kPoint2Input);
  EXPECT_EQ(keyframe_linear_timing_fn->Points()[1].output, kPoint2Output);
  EXPECT_EQ(keyframe_linear_timing_fn->Points()[2].input, kPoint3Input);
  EXPECT_EQ(keyframe_linear_timing_fn->Points()[2].output, kPoint3Output);

  // Verify the second keyframe's timing function (should be default cubic
  // bezier).
  ASSERT_EQ(float_curve->keyframes().size(), 2u);
  const auto* second_keyframe_timing_fn =
      float_curve->keyframes()[1]->timing_function();
  ASSERT_NE(nullptr, second_keyframe_timing_fn);
  EXPECT_EQ(second_keyframe_timing_fn->GetType(),
            gfx::TimingFunction::Type::CUBIC_BEZIER);
}

TEST_F(LayerContextImplAnimationTest, DeserializeWithStepsTimingFunction) {
  constexpr int kTimelineId = 13;
  constexpr int kAnimationId = 130;
  constexpr int kKeyframeModelId = 1300;
  constexpr int kGroupId = 13;
  constexpr uint32_t kNumSteps = 4;
  constexpr mojom::TimingStepPosition kStepPosition =
      mojom::TimingStepPosition::kEnd;

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom =
      CreateDefaultMojomAnimation(kAnimationId, kKeyframeModelId, kGroupId);
  // Override the keyframe model's timing function.
  animation_mojom->keyframe_models[0]->timing_function =
      CreateMojomStepsTimingFunction(kNumSteps, kStepPosition);
  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  cc::AnimationHost* host = GetAnimationHost();
  cc::AnimationTimeline* timeline_impl = host->GetTimelineById(kTimelineId);
  ASSERT_NE(nullptr, timeline_impl);
  cc::Animation* animation_impl = timeline_impl->GetAnimationById(kAnimationId);
  ASSERT_NE(nullptr, animation_impl);
  cc::KeyframeEffect* effect_impl = animation_impl->keyframe_effect();
  ASSERT_NE(nullptr, effect_impl);
  ASSERT_EQ(effect_impl->keyframe_models().size(), 1u);
  gfx::KeyframeModel* gfx_model_impl = effect_impl->keyframe_models()[0].get();
  ASSERT_NE(nullptr, gfx_model_impl);
  const gfx::AnimationCurve* curve = gfx_model_impl->curve();
  ASSERT_NE(nullptr, curve);
  // Default animation is opacity (float).
  const auto* float_curve =
      static_cast<const gfx::KeyframedFloatAnimationCurve*>(curve);
  ASSERT_NE(nullptr, float_curve->timing_function());
  EXPECT_EQ(float_curve->timing_function()->GetType(),
            gfx::TimingFunction::Type::STEPS);
  const auto* steps_timing_fn = static_cast<const gfx::StepsTimingFunction*>(
      float_curve->timing_function());
  EXPECT_EQ(steps_timing_fn->steps(), static_cast<int>(kNumSteps));
  EXPECT_EQ(steps_timing_fn->step_position(),
            gfx::StepsTimingFunction::StepPosition::END);
}

struct StepsTimingFunctionTestData {
  int timeline_id;
  int animation_id;
  int keyframe_model_id;
  int group_id;
  mojom::TimingStepPosition mojom_step_position;
  gfx::StepsTimingFunction::StepPosition expected_gfx_step_position;
};

class LayerContextImplStepsTimingFunctionTest
    : public LayerContextImplAnimationTest,
      public testing::WithParamInterface<StepsTimingFunctionTestData> {};

TEST_P(LayerContextImplStepsTimingFunctionTest, Deserialize) {
  const auto& param = GetParam();

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(param.timeline_id);
  auto animation_mojom = CreateDefaultMojomAnimation(
      param.animation_id, param.keyframe_model_id, param.group_id);
  animation_mojom->keyframe_models[0]->timing_function =
      CreateMojomStepsTimingFunction(kDefaultSteps, param.mojom_step_position);
  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  const cc::AnimationHost* host = GetAnimationHost();
  const cc::AnimationTimeline* timeline_impl =
      host->GetTimelineById(param.timeline_id);
  ASSERT_NE(nullptr, timeline_impl);
  const cc::Animation* animation_impl =
      timeline_impl->GetAnimationById(param.animation_id);
  ASSERT_NE(nullptr, animation_impl);
  const cc::KeyframeEffect* effect_impl = animation_impl->keyframe_effect();
  ASSERT_NE(nullptr, effect_impl);
  ASSERT_EQ(effect_impl->keyframe_models().size(), 1u);
  const gfx::KeyframeModel* gfx_model_impl =
      effect_impl->keyframe_models()[0].get();
  ASSERT_NE(nullptr, gfx_model_impl);
  const auto* float_curve =
      static_cast<const gfx::KeyframedFloatAnimationCurve*>(
          gfx_model_impl->curve());
  ASSERT_NE(nullptr, float_curve->timing_function());
  const auto* steps_timing_fn = static_cast<const gfx::StepsTimingFunction*>(
      float_curve->timing_function());
  EXPECT_EQ(steps_timing_fn->step_position(), param.expected_gfx_step_position);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    LayerContextImplStepsTimingFunctionTest,
    testing::Values(
        StepsTimingFunctionTestData{
            18, 180, 1800, 18, mojom::TimingStepPosition::kJumpBoth,
            gfx::StepsTimingFunction::StepPosition::JUMP_BOTH},
        StepsTimingFunctionTestData{
            19, 190, 1900, 19, mojom::TimingStepPosition::kJumpEnd,
            gfx::StepsTimingFunction::StepPosition::JUMP_END},
        StepsTimingFunctionTestData{
            20, 200, 2000, 20, mojom::TimingStepPosition::kJumpNone,
            gfx::StepsTimingFunction::StepPosition::JUMP_NONE},
        StepsTimingFunctionTestData{
            21, 210, 2100, 21, mojom::TimingStepPosition::kJumpStart,
            gfx::StepsTimingFunction::StepPosition::JUMP_START}));

TEST_F(LayerContextImplAnimationTest,
       DeserializeAnimationWithZeroPlaybackRateFails) {
  constexpr int kTimelineId = 14;
  constexpr int kAnimationId = 140;
  constexpr int kKeyframeModelId = 1400;
  constexpr int kGroupId = 14;

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom = mojom::Animation::New();
  animation_mojom->id = kAnimationId;
  animation_mojom->element_id = kDefaultElementId;

  auto model_mojom = CreateDefaultMojomKeyframeModel(
      kKeyframeModelId, kGroupId, cc::TargetProperty::OPACITY);
  model_mojom->playback_rate = 0.0;  // Invalid playback rate

  animation_mojom->keyframe_models.push_back(std::move(model_mojom));
  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid playback_rate: cannot be 0");

  // Verify no animation was added.
  cc::AnimationHost* host = GetAnimationHost();
  ASSERT_NE(nullptr, host);
  cc::AnimationTimeline* timeline_impl = host->GetTimelineById(kTimelineId);
  EXPECT_EQ(nullptr, timeline_impl);
}

TEST_F(LayerContextImplAnimationTest,
       DeserializeAnimationWithMismatchedKeyframeTypeScalarToTransform) {
  constexpr int kTimelineId = 15;
  constexpr int kAnimationId = 150;
  constexpr int kKeyframeModelId = 1500;
  constexpr int kGroupId = 15;

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom = CreateAnimationWithDefaultModel(
      kAnimationId, kKeyframeModelId, kGroupId, cc::TargetProperty::TRANSFORM);
  // Keyframe 1: TRANSFORM (sets the curve type)
  animation_mojom->keyframe_models[0]->keyframes.push_back(
      CreateMojomTransformKeyframeDefault(kDefaultKeyframeStartTime));
  // Keyframe 2: SCALAR (mismatches the curve type)
  animation_mojom->keyframe_models[0]->keyframes.push_back(
      CreateDefaultMojomScalarKeyframe(kDefaultKeyframeEndOpacityTime,
                                       kDefaultKeyframeStartOpacity));

  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid keyframe type");
}

TEST_F(LayerContextImplAnimationTest,
       DeserializeAnimationWithMismatchedKeyframeTypeColorToOpacity) {
  constexpr int kTimelineId = 16;
  constexpr int kAnimationId = 160;
  constexpr int kKeyframeModelId = 1600;
  constexpr int kGroupId = 16;

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom = CreateAnimationWithDefaultModel(
      kAnimationId, kKeyframeModelId, kGroupId, cc::TargetProperty::OPACITY);
  // Keyframe 1: SCALAR (sets the curve type for OPACITY)
  animation_mojom->keyframe_models[0]->keyframes.push_back(
      CreateDefaultMojomScalarKeyframe(kDefaultKeyframeStartTime,
                                       kDefaultKeyframeStartOpacity));
  // Keyframe 2: COLOR (mismatches the curve type)
  animation_mojom->keyframe_models[0]->keyframes.push_back(
      CreateMojomColorKeyframe(kDefaultKeyframeEndOpacityTime, SK_ColorRED));

  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid keyframe type");
}

TEST_F(LayerContextImplAnimationTest,
       DeserializeAnimationWithMismatchedKeyframeTypeRectToBackgroundColor) {
  constexpr int kTimelineId = 17;
  constexpr int kAnimationId = 170;
  constexpr int kKeyframeModelId = 1700;
  constexpr int kGroupId = 17;

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();

  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom =
      CreateAnimationWithDefaultModel(kAnimationId, kKeyframeModelId, kGroupId,
                                      cc::TargetProperty::BACKGROUND_COLOR);
  // Keyframe 1: COLOR (sets the curve type for BACKGROUND_COLOR)
  animation_mojom->keyframe_models[0]->keyframes.push_back(
      CreateMojomColorKeyframeDefault(kDefaultKeyframeStartTime));
  // Keyframe 2: RECT (mismatches the curve type)
  animation_mojom->keyframe_models[0]->keyframes.push_back(
      CreateMojomRectKeyframe(kDefaultKeyframeEndOpacityTime,
                              gfx::Rect(1, 2, 3, 4)));

  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid keyframe type");
}

struct AnimationDirectionTestData {
  mojom::AnimationDirection mojom_direction;
  cc::KeyframeModel::Direction expected_cc_direction;
};

class LayerContextImplAnimationDirectionTest
    : public LayerContextImplAnimationTest,
      public testing::WithParamInterface<AnimationDirectionTestData> {};

TEST_P(LayerContextImplAnimationDirectionTest, Deserialize) {
  const auto& param = GetParam();
  constexpr int kTimelineId = 24;  // Ensure unique IDs
  constexpr int kAnimationId = 240;
  constexpr int kKeyframeModelId = 2400;
  constexpr int kGroupId = 24;

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();
  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom =
      CreateDefaultMojomAnimation(kAnimationId, kKeyframeModelId, kGroupId);
  animation_mojom->keyframe_models[0]->direction = param.mojom_direction;
  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  const cc::AnimationHost* host = GetAnimationHost();
  const cc::AnimationTimeline* timeline_impl =
      host->GetTimelineById(kTimelineId);
  ASSERT_NE(nullptr, timeline_impl);
  const cc::Animation* animation_impl =
      timeline_impl->GetAnimationById(kAnimationId);
  ASSERT_NE(nullptr, animation_impl);
  const cc::KeyframeEffect* effect_impl = animation_impl->keyframe_effect();
  ASSERT_NE(nullptr, effect_impl);
  ASSERT_EQ(effect_impl->keyframe_models().size(), 1u);
  const gfx::KeyframeModel* gfx_model_impl =
      effect_impl->keyframe_models()[0].get();
  ASSERT_NE(nullptr, gfx_model_impl);
  EXPECT_EQ(gfx_model_impl->direction(), param.expected_cc_direction);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    LayerContextImplAnimationDirectionTest,
    testing::Values(
        AnimationDirectionTestData{mojom::AnimationDirection::kNormal,
                                   cc::KeyframeModel::Direction::NORMAL},
        AnimationDirectionTestData{mojom::AnimationDirection::kReverse,
                                   cc::KeyframeModel::Direction::REVERSE},
        AnimationDirectionTestData{
            mojom::AnimationDirection::kAlternateNormal,
            cc::KeyframeModel::Direction::ALTERNATE_NORMAL},
        AnimationDirectionTestData{
            mojom::AnimationDirection::kAlternateReverse,
            cc::KeyframeModel::Direction::ALTERNATE_REVERSE}));

struct AnimationFillModeTestData {
  mojom::AnimationFillMode mojom_fill_mode;
  cc::KeyframeModel::FillMode expected_cc_fill_mode;
};

class LayerContextImplAnimationFillModeTest
    : public LayerContextImplAnimationTest,
      public testing::WithParamInterface<AnimationFillModeTestData> {};

TEST_P(LayerContextImplAnimationFillModeTest, Deserialize) {
  const auto& param = GetParam();
  constexpr int kTimelineId = 25;  // Ensure unique IDs
  constexpr int kAnimationId = 250;
  constexpr int kKeyframeModelId = 2500;
  constexpr int kGroupId = 25;

  auto update = CreateDefaultUpdate();
  update->animation_timelines = std::vector<mojom::AnimationTimelinePtr>();
  auto timeline_mojom = CreateDefaultMojomTimeline(kTimelineId);
  auto animation_mojom =
      CreateDefaultMojomAnimation(kAnimationId, kKeyframeModelId, kGroupId);
  animation_mojom->keyframe_models[0]->fill_mode = param.mojom_fill_mode;
  timeline_mojom->new_animations.push_back(std::move(animation_mojom));
  update->animation_timelines->push_back(std::move(timeline_mojom));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  const cc::AnimationHost* host = GetAnimationHost();
  const cc::AnimationTimeline* timeline_impl =
      host->GetTimelineById(kTimelineId);
  ASSERT_NE(nullptr, timeline_impl);
  const cc::Animation* animation_impl =
      timeline_impl->GetAnimationById(kAnimationId);
  ASSERT_NE(nullptr, animation_impl);
  const cc::KeyframeEffect* effect_impl = animation_impl->keyframe_effect();
  ASSERT_NE(nullptr, effect_impl);
  ASSERT_EQ(effect_impl->keyframe_models().size(), 1u);
  const gfx::KeyframeModel* gfx_model_impl =
      effect_impl->keyframe_models()[0].get();
  ASSERT_NE(nullptr, gfx_model_impl);
  EXPECT_EQ(gfx_model_impl->fill_mode(), param.expected_cc_fill_mode);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    LayerContextImplAnimationFillModeTest,
    testing::Values(
        AnimationFillModeTestData{mojom::AnimationFillMode::kNone,
                                  cc::KeyframeModel::FillMode::NONE},
        AnimationFillModeTestData{mojom::AnimationFillMode::kForwards,
                                  cc::KeyframeModel::FillMode::FORWARDS},
        AnimationFillModeTestData{mojom::AnimationFillMode::kBackwards,
                                  cc::KeyframeModel::FillMode::BACKWARDS},
        AnimationFillModeTestData{mojom::AnimationFillMode::kBoth,
                                  cc::KeyframeModel::FillMode::BOTH},
        AnimationFillModeTestData{mojom::AnimationFillMode::kAuto,
                                  cc::KeyframeModel::FillMode::AUTO}));

}  // namespace
}  // namespace viz
