// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/animation.h"

#include "cc/animation/animation_curve.h"
#include "cc/test/geometry_test_utils.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/transition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/test/gfx_util.h"

namespace vr {

static constexpr float kNoise = 1e-6f;

class TestAnimationTarget : public cc::SizeAnimationCurve::Target,
                            public cc::TransformAnimationCurve::Target,
                            public cc::FloatAnimationCurve::Target,
                            public cc::ColorAnimationCurve::Target {
 public:
  TestAnimationTarget() {
    layout_offset_.AppendTranslate(0, 0, 0);
    operations_.AppendTranslate(0, 0, 0);
    operations_.AppendRotate(1, 0, 0, 0);
    operations_.AppendScale(1, 1, 1);
  }

  const gfx::SizeF& size() const { return size_; }
  const gfx::TransformOperations& operations() const { return operations_; }
  const gfx::TransformOperations& layout_offset() const {
    return layout_offset_;
  }
  float opacity() const { return opacity_; }
  SkColor background_color() const { return background_color_; }

  void OnSizeAnimated(const gfx::SizeF& size,
                      int target_property_id,
                      cc::KeyframeModel* keyframe_model) override {
    size_ = size;
  }

  void OnTransformAnimated(const gfx::TransformOperations& operations,
                           int target_property_id,
                           cc::KeyframeModel* keyframe_model) override {
    if (target_property_id == LAYOUT_OFFSET) {
      layout_offset_ = operations;
    } else {
      operations_ = operations;
    }
  }

  void OnFloatAnimated(const float& opacity,
                       int target_property_id,
                       cc::KeyframeModel* keyframe_model) override {
    opacity_ = opacity;
  }

  void OnColorAnimated(const SkColor& color,
                       int target_property_id,
                       cc::KeyframeModel* keyframe_model) override {
    background_color_ = color;
  }

 private:
  gfx::TransformOperations layout_offset_;
  gfx::TransformOperations operations_;
  gfx::SizeF size_ = {10.0f, 10.0f};
  float opacity_ = 1.0f;
  SkColor background_color_ = SK_ColorRED;
};

TEST(AnimationTest, AddRemoveKeyframeModels) {
  Animation animation;
  EXPECT_TRUE(animation.keyframe_models().empty());
  TestAnimationTarget target;

  animation.AddKeyframeModel(
      CreateBoundsAnimation(&target, 1, 1, gfx::SizeF(10, 100),
                            gfx::SizeF(20, 200), MicrosecondsToDelta(10000)));
  EXPECT_EQ(1ul, animation.keyframe_models().size());
  EXPECT_EQ(BOUNDS, animation.keyframe_models()[0]->target_property_type());

  gfx::TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  gfx::TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  animation.AddKeyframeModel(
      CreateTransformAnimation(&target, 2, 2, from_operations, to_operations,
                               MicrosecondsToDelta(10000)));

  EXPECT_EQ(2ul, animation.keyframe_models().size());
  EXPECT_EQ(TRANSFORM, animation.keyframe_models()[1]->target_property_type());

  animation.AddKeyframeModel(
      CreateTransformAnimation(&target, 3, 3, from_operations, to_operations,
                               MicrosecondsToDelta(10000)));
  EXPECT_EQ(3ul, animation.keyframe_models().size());
  EXPECT_EQ(TRANSFORM, animation.keyframe_models()[2]->target_property_type());

  animation.RemoveKeyframeModels(TRANSFORM);
  EXPECT_EQ(1ul, animation.keyframe_models().size());
  EXPECT_EQ(BOUNDS, animation.keyframe_models()[0]->target_property_type());

  animation.RemoveKeyframeModel(animation.keyframe_models()[0]->id());
  EXPECT_TRUE(animation.keyframe_models().empty());
}

TEST(AnimationTest, AnimationLifecycle) {
  TestAnimationTarget target;
  Animation animation;

  animation.AddKeyframeModel(
      CreateBoundsAnimation(&target, 1, 1, gfx::SizeF(10, 100),
                            gfx::SizeF(20, 200), MicrosecondsToDelta(10000)));
  EXPECT_EQ(1ul, animation.keyframe_models().size());
  EXPECT_EQ(BOUNDS, animation.keyframe_models()[0]->target_property_type());
  EXPECT_EQ(cc::KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation.keyframe_models()[0]->run_state());

  base::TimeTicks start_time = MicrosecondsToTicks(1);
  animation.Tick(start_time);
  EXPECT_EQ(cc::KeyframeModel::RUNNING,
            animation.keyframe_models()[0]->run_state());

  EXPECT_SIZEF_EQ(gfx::SizeF(10, 100), target.size());

  // Tick beyond the animation
  animation.Tick(start_time + MicrosecondsToDelta(20000));

  EXPECT_TRUE(animation.keyframe_models().empty());

  // Should have assumed the final value.
  EXPECT_SIZEF_EQ(gfx::SizeF(20, 200), target.size());
}

TEST(AnimationTest, AnimationQueue) {
  TestAnimationTarget target;
  Animation animation;

  animation.AddKeyframeModel(
      CreateBoundsAnimation(&target, 1, 1, gfx::SizeF(10, 100),
                            gfx::SizeF(20, 200), MicrosecondsToDelta(10000)));
  EXPECT_EQ(1ul, animation.keyframe_models().size());
  EXPECT_EQ(BOUNDS, animation.keyframe_models()[0]->target_property_type());
  EXPECT_EQ(cc::KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation.keyframe_models()[0]->run_state());

  base::TimeTicks start_time = MicrosecondsToTicks(1);
  animation.Tick(start_time);
  EXPECT_EQ(cc::KeyframeModel::RUNNING,
            animation.keyframe_models()[0]->run_state());
  EXPECT_SIZEF_EQ(gfx::SizeF(10, 100), target.size());

  animation.AddKeyframeModel(
      CreateBoundsAnimation(&target, 2, 2, gfx::SizeF(10, 100),
                            gfx::SizeF(20, 200), MicrosecondsToDelta(10000)));

  gfx::TransformOperations from_operations;
  from_operations.AppendTranslate(10, 100, 1000);
  gfx::TransformOperations to_operations;
  to_operations.AppendTranslate(20, 200, 2000);
  animation.AddKeyframeModel(
      CreateTransformAnimation(&target, 3, 2, from_operations, to_operations,
                               MicrosecondsToDelta(10000)));

  EXPECT_EQ(3ul, animation.keyframe_models().size());
  EXPECT_EQ(BOUNDS, animation.keyframe_models()[1]->target_property_type());
  EXPECT_EQ(TRANSFORM, animation.keyframe_models()[2]->target_property_type());
  int id1 = animation.keyframe_models()[1]->id();

  animation.Tick(start_time + MicrosecondsToDelta(1));

  // Only the transform animation should have started (since there's no
  // conflicting animation).
  EXPECT_EQ(cc::KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY,
            animation.keyframe_models()[1]->run_state());
  EXPECT_EQ(cc::KeyframeModel::RUNNING,
            animation.keyframe_models()[2]->run_state());

  // Tick beyond the first animation. This should cause it (and the transform
  // animation) to get removed and for the second bounds animation to start.
  animation.Tick(start_time + MicrosecondsToDelta(15000));

  EXPECT_EQ(1ul, animation.keyframe_models().size());
  EXPECT_EQ(cc::KeyframeModel::RUNNING,
            animation.keyframe_models()[0]->run_state());
  EXPECT_EQ(id1, animation.keyframe_models()[0]->id());

  // Tick beyond all animations. There should be none remaining.
  animation.Tick(start_time + MicrosecondsToDelta(30000));
  EXPECT_TRUE(animation.keyframe_models().empty());
}

TEST(AnimationTest, FinishedTransition) {
  TestAnimationTarget target;
  Animation animation;
  Transition transition;
  transition.target_properties = {OPACITY};
  transition.duration = MsToDelta(10);
  animation.set_transition(transition);

  base::TimeTicks start_time = MsToTicks(1000);
  animation.Tick(start_time);

  float from = 1.0f;
  float to = 0.0f;
  animation.TransitionFloatTo(&target, start_time, OPACITY, from, to);

  animation.Tick(start_time);
  EXPECT_EQ(from, target.opacity());

  // We now simulate a long pause where the element hasn't been ticked (eg, it
  // may have been hidden). If this happens, the unticked transition must still
  // be treated as having finished.
  animation.TransitionFloatTo(&target, start_time + MsToDelta(1000), OPACITY,
                              target.opacity(), 1.0f);

  animation.Tick(start_time + MsToDelta(1000));
  EXPECT_EQ(to, target.opacity());
}

TEST(AnimationTest, OpacityTransitions) {
  TestAnimationTarget target;
  Animation animation;
  Transition transition;
  transition.target_properties = {OPACITY};
  transition.duration = MicrosecondsToDelta(10000);
  animation.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animation.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  animation.TransitionFloatTo(&target, start_time, OPACITY, from, to);

  EXPECT_EQ(from, target.opacity());
  animation.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int keyframe_model_id = animation.keyframe_models().front()->id();
  float nearby = to + kNoise;
  animation.TransitionFloatTo(&target, start_time, OPACITY, from, nearby);
  EXPECT_EQ(keyframe_model_id, animation.keyframe_models().front()->id());

  animation.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_GT(from, target.opacity());
  EXPECT_LT(to, target.opacity());

  animation.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_EQ(to, target.opacity());
}

TEST(AnimationTest, ReversedOpacityTransitions) {
  TestAnimationTarget target;
  Animation animation;
  Transition transition;
  transition.target_properties = {OPACITY};
  transition.duration = MicrosecondsToDelta(10000);
  animation.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animation.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  animation.TransitionFloatTo(&target, start_time, OPACITY, from, to);

  EXPECT_EQ(from, target.opacity());
  animation.Tick(start_time);

  animation.Tick(start_time + MicrosecondsToDelta(1000));
  float value_before_reversing = target.opacity();
  EXPECT_GT(from, value_before_reversing);
  EXPECT_LT(to, value_before_reversing);

  animation.TransitionFloatTo(&target, start_time + MicrosecondsToDelta(1000),
                              OPACITY, target.opacity(), from);
  animation.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_FLOAT_EQ(value_before_reversing, target.opacity());

  animation.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_EQ(from, target.opacity());
}

TEST(AnimationTest, LayoutOffsetTransitions) {
  // In this test, we do expect exact equality.
  float tolerance = 0.0f;
  TestAnimationTarget target;
  Animation animation;
  Transition transition;
  transition.target_properties = {LAYOUT_OFFSET};
  transition.duration = MicrosecondsToDelta(10000);
  animation.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animation.Tick(start_time);

  gfx::TransformOperations from = target.layout_offset();

  gfx::TransformOperations to;
  to.AppendTranslate(8, 0, 0);

  animation.TransitionTransformOperationsTo(&target, start_time, LAYOUT_OFFSET,
                                            from, to);

  EXPECT_TRUE(from.ApproximatelyEqual(target.layout_offset(), tolerance));
  animation.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int keyframe_model_id = animation.keyframe_models().front()->id();
  gfx::TransformOperations nearby = to;
  nearby.at(0).translate.x += kNoise;
  animation.TransitionTransformOperationsTo(&target, start_time, LAYOUT_OFFSET,
                                            from, nearby);
  EXPECT_EQ(keyframe_model_id, animation.keyframe_models().front()->id());

  animation.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_LT(from.at(0).translate.x, target.layout_offset().at(0).translate.x);
  EXPECT_GT(to.at(0).translate.x, target.layout_offset().at(0).translate.x);

  animation.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_TRUE(to.ApproximatelyEqual(target.layout_offset(), tolerance));
}

TEST(AnimationTest, TransformTransitions) {
  // In this test, we do expect exact equality.
  float tolerance = 0.0f;
  TestAnimationTarget target;
  Animation animation;
  Transition transition;
  transition.target_properties = {TRANSFORM};
  transition.duration = MicrosecondsToDelta(10000);
  animation.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animation.Tick(start_time);

  gfx::TransformOperations from = target.operations();

  gfx::TransformOperations to;
  to.AppendTranslate(8, 0, 0);
  to.AppendRotate(1, 0, 0, 0);
  to.AppendScale(1, 1, 1);

  animation.TransitionTransformOperationsTo(&target, start_time, TRANSFORM,
                                            from, to);

  EXPECT_TRUE(from.ApproximatelyEqual(target.operations(), tolerance));
  animation.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int keyframe_model_id = animation.keyframe_models().front()->id();
  gfx::TransformOperations nearby = to;
  nearby.at(0).translate.x += kNoise;
  animation.TransitionTransformOperationsTo(&target, start_time, TRANSFORM,
                                            from, nearby);
  EXPECT_EQ(keyframe_model_id, animation.keyframe_models().front()->id());

  animation.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_LT(from.at(0).translate.x, target.operations().at(0).translate.x);
  EXPECT_GT(to.at(0).translate.x, target.operations().at(0).translate.x);

  animation.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_TRUE(to.ApproximatelyEqual(target.operations(), tolerance));
}

TEST(AnimationTest, ReversedTransformTransitions) {
  // In this test, we do expect exact equality.
  float tolerance = 0.0f;
  TestAnimationTarget target;
  Animation animation;
  Transition transition;
  transition.target_properties = {TRANSFORM};
  transition.duration = MicrosecondsToDelta(10000);
  animation.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animation.Tick(start_time);

  gfx::TransformOperations from = target.operations();

  gfx::TransformOperations to;
  to.AppendTranslate(8, 0, 0);
  to.AppendRotate(1, 0, 0, 0);
  to.AppendScale(1, 1, 1);

  animation.TransitionTransformOperationsTo(&target, start_time, TRANSFORM,
                                            from, to);

  EXPECT_TRUE(from.ApproximatelyEqual(target.operations(), tolerance));
  animation.Tick(start_time);

  animation.Tick(start_time + MicrosecondsToDelta(1000));
  gfx::TransformOperations value_before_reversing = target.operations();
  EXPECT_LT(from.at(0).translate.x, target.operations().at(0).translate.x);
  EXPECT_GT(to.at(0).translate.x, target.operations().at(0).translate.x);

  animation.TransitionTransformOperationsTo(
      &target, start_time + MicrosecondsToDelta(1000), TRANSFORM,
      target.operations(), from);
  animation.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_TRUE(value_before_reversing.ApproximatelyEqual(target.operations(),
                                                        tolerance));

  animation.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_TRUE(from.ApproximatelyEqual(target.operations(), tolerance));
}

TEST(AnimationTest, BoundsTransitions) {
  TestAnimationTarget target;
  Animation animation;
  Transition transition;
  transition.target_properties = {BOUNDS};
  transition.duration = MicrosecondsToDelta(10000);
  animation.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animation.Tick(start_time);

  gfx::SizeF from = target.size();
  gfx::SizeF to(20.0f, 20.0f);

  animation.TransitionSizeTo(&target, start_time, BOUNDS, from, to);

  EXPECT_FLOAT_SIZE_EQ(from, target.size());
  animation.Tick(start_time);

  // Scheduling a redundant, approximately equal transition should be ignored.
  int keyframe_model_id = animation.keyframe_models().front()->id();
  gfx::SizeF nearby = to;
  nearby.set_width(to.width() + kNoise);
  animation.TransitionSizeTo(&target, start_time, BOUNDS, from, nearby);
  EXPECT_EQ(keyframe_model_id, animation.keyframe_models().front()->id());

  animation.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_LT(from.width(), target.size().width());
  EXPECT_GT(to.width(), target.size().width());
  EXPECT_LT(from.height(), target.size().height());
  EXPECT_GT(to.height(), target.size().height());

  animation.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_FLOAT_SIZE_EQ(to, target.size());
}

TEST(AnimationTest, ReversedBoundsTransitions) {
  TestAnimationTarget target;
  Animation animation;
  Transition transition;
  transition.target_properties = {BOUNDS};
  transition.duration = MicrosecondsToDelta(10000);
  animation.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animation.Tick(start_time);

  gfx::SizeF from = target.size();
  gfx::SizeF to(20.0f, 20.0f);

  animation.TransitionSizeTo(&target, start_time, BOUNDS, from, to);

  EXPECT_FLOAT_SIZE_EQ(from, target.size());
  animation.Tick(start_time);

  animation.Tick(start_time + MicrosecondsToDelta(1000));
  gfx::SizeF value_before_reversing = target.size();
  EXPECT_LT(from.width(), target.size().width());
  EXPECT_GT(to.width(), target.size().width());
  EXPECT_LT(from.height(), target.size().height());
  EXPECT_GT(to.height(), target.size().height());

  animation.TransitionSizeTo(&target, start_time + MicrosecondsToDelta(1000),
                             BOUNDS, target.size(), from);
  animation.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_FLOAT_SIZE_EQ(value_before_reversing, target.size());

  animation.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_FLOAT_SIZE_EQ(from, target.size());
}

TEST(AnimationTest, BackgroundColorTransitions) {
  TestAnimationTarget target;
  Animation animation;
  Transition transition;
  transition.target_properties = {BACKGROUND_COLOR};
  transition.duration = MicrosecondsToDelta(10000);
  animation.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animation.Tick(start_time);

  SkColor from = SK_ColorRED;
  SkColor to = SK_ColorGREEN;

  animation.TransitionColorTo(&target, start_time, BACKGROUND_COLOR, from, to);

  EXPECT_EQ(from, target.background_color());
  animation.Tick(start_time);

  animation.Tick(start_time + MicrosecondsToDelta(5000));
  EXPECT_GT(SkColorGetR(from), SkColorGetR(target.background_color()));
  EXPECT_LT(SkColorGetR(to), SkColorGetR(target.background_color()));
  EXPECT_LT(SkColorGetG(from), SkColorGetG(target.background_color()));
  EXPECT_GT(SkColorGetG(to), SkColorGetG(target.background_color()));
  EXPECT_EQ(0u, SkColorGetB(target.background_color()));
  EXPECT_EQ(255u, SkColorGetA(target.background_color()));

  animation.Tick(start_time + MicrosecondsToDelta(10000));
  EXPECT_EQ(to, target.background_color());
}

TEST(AnimationTest, ReversedBackgroundColorTransitions) {
  TestAnimationTarget target;
  Animation animation;
  Transition transition;
  transition.target_properties = {BACKGROUND_COLOR};
  transition.duration = MicrosecondsToDelta(10000);
  animation.set_transition(transition);
  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animation.Tick(start_time);

  SkColor from = SK_ColorRED;
  SkColor to = SK_ColorGREEN;

  animation.TransitionColorTo(&target, start_time, BACKGROUND_COLOR, from, to);

  EXPECT_EQ(from, target.background_color());
  animation.Tick(start_time);

  animation.Tick(start_time + MicrosecondsToDelta(1000));
  SkColor value_before_reversing = target.background_color();
  EXPECT_GT(SkColorGetR(from), SkColorGetR(target.background_color()));
  EXPECT_LT(SkColorGetR(to), SkColorGetR(target.background_color()));
  EXPECT_LT(SkColorGetG(from), SkColorGetG(target.background_color()));
  EXPECT_GT(SkColorGetG(to), SkColorGetG(target.background_color()));
  EXPECT_EQ(0u, SkColorGetB(target.background_color()));
  EXPECT_EQ(255u, SkColorGetA(target.background_color()));

  animation.TransitionColorTo(&target, start_time + MicrosecondsToDelta(1000),
                              BACKGROUND_COLOR, target.background_color(),
                              from);
  animation.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_EQ(value_before_reversing, target.background_color());

  animation.Tick(start_time + MicrosecondsToDelta(2000));
  EXPECT_EQ(from, target.background_color());
}

TEST(AnimationTest, DoubleReversedTransitions) {
  TestAnimationTarget target;
  Animation animation;
  Transition transition;
  transition.target_properties = {OPACITY};
  transition.duration = MicrosecondsToDelta(10000);
  animation.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animation.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  animation.TransitionFloatTo(&target, start_time, OPACITY, from, to);

  EXPECT_EQ(from, target.opacity());
  animation.Tick(start_time);

  animation.Tick(start_time + MicrosecondsToDelta(1000));
  float value_before_reversing = target.opacity();
  EXPECT_GT(from, value_before_reversing);
  EXPECT_LT(to, value_before_reversing);

  animation.TransitionFloatTo(&target, start_time + MicrosecondsToDelta(1000),
                              OPACITY, target.opacity(), from);
  animation.Tick(start_time + MicrosecondsToDelta(1000));
  EXPECT_FLOAT_EQ(value_before_reversing, target.opacity());

  animation.Tick(start_time + MicrosecondsToDelta(1500));
  value_before_reversing = target.opacity();
  // If the code for reversing transitions does not account for an existing time
  // offset, then reversing a second time will give incorrect values.
  animation.TransitionFloatTo(&target, start_time + MicrosecondsToDelta(1500),
                              OPACITY, target.opacity(), to);
  animation.Tick(start_time + MicrosecondsToDelta(1500));
  EXPECT_FLOAT_EQ(value_before_reversing, target.opacity());
}

TEST(AnimationTest, RedundantTransition) {
  TestAnimationTarget target;
  Animation animation;
  Transition transition;
  transition.target_properties = {OPACITY};
  transition.duration = MicrosecondsToDelta(10000);
  animation.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animation.Tick(start_time);

  float from = 1.0f;
  float to = 0.5f;
  animation.TransitionFloatTo(&target, start_time, OPACITY, from, to);

  EXPECT_EQ(from, target.opacity());
  animation.Tick(start_time);

  animation.Tick(start_time + MicrosecondsToDelta(1000));
  float value_before_redundant_transition = target.opacity();

  // While an existing transition is in progress to the same value, we should
  // not start a new transition.
  animation.TransitionFloatTo(&target, start_time, OPACITY, target.opacity(),
                              to);

  EXPECT_EQ(1lu, animation.keyframe_models().size());
  EXPECT_EQ(value_before_redundant_transition, target.opacity());
}

TEST(AnimationTest, TransitionToSameValue) {
  TestAnimationTarget target;
  Animation animation;
  Transition transition;
  transition.target_properties = {OPACITY};
  transition.duration = MicrosecondsToDelta(10000);
  animation.set_transition(transition);

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animation.Tick(start_time);

  // Transitioning to the same value should be a no-op.
  float from = 1.0f;
  float to = 1.0f;
  animation.TransitionFloatTo(&target, start_time, OPACITY, from, to);
  EXPECT_EQ(from, target.opacity());
  EXPECT_TRUE(animation.keyframe_models().empty());
}

TEST(AnimationTest, CorrectTargetValue) {
  TestAnimationTarget target;
  Animation animation;
  base::TimeDelta duration = MicrosecondsToDelta(10000);

  float from_opacity = 1.0f;
  float to_opacity = 0.5f;
  gfx::SizeF from_bounds = gfx::SizeF(10, 200);
  gfx::SizeF to_bounds = gfx::SizeF(20, 200);
  SkColor from_color = SK_ColorRED;
  SkColor to_color = SK_ColorGREEN;
  gfx::TransformOperations from_transform;
  from_transform.AppendTranslate(10, 100, 1000);
  gfx::TransformOperations to_transform;
  to_transform.AppendTranslate(20, 200, 2000);

  // Verify the default value is returned if there's no running animations.
  EXPECT_EQ(from_opacity, animation.GetTargetFloatValue(OPACITY, from_opacity));
  EXPECT_SIZEF_EQ(from_bounds,
                  animation.GetTargetSizeValue(BOUNDS, from_bounds));
  EXPECT_EQ(from_color,
            animation.GetTargetColorValue(BACKGROUND_COLOR, from_color));
  EXPECT_TRUE(from_transform.ApproximatelyEqual(
      animation.GetTargetTransformOperationsValue(TRANSFORM, from_transform),
      kEpsilon));

  // Add keyframe_models.
  animation.AddKeyframeModel(CreateOpacityAnimation(&target, 2, 1, from_opacity,
                                                    to_opacity, duration));
  animation.AddKeyframeModel(
      CreateBoundsAnimation(&target, 1, 1, from_bounds, to_bounds, duration));
  animation.AddKeyframeModel(CreateBackgroundColorAnimation(
      &target, 3, 1, from_color, to_color, duration));
  animation.AddKeyframeModel(CreateTransformAnimation(
      &target, 4, 1, from_transform, to_transform, duration));

  base::TimeTicks start_time = MicrosecondsToTicks(1000000);
  animation.Tick(start_time);

  // Verify target value.
  EXPECT_EQ(to_opacity, animation.GetTargetFloatValue(OPACITY, from_opacity));
  EXPECT_SIZEF_EQ(to_bounds, animation.GetTargetSizeValue(BOUNDS, from_bounds));
  EXPECT_EQ(to_color,
            animation.GetTargetColorValue(BACKGROUND_COLOR, from_color));
  EXPECT_TRUE(to_transform.ApproximatelyEqual(
      animation.GetTargetTransformOperationsValue(TRANSFORM, from_transform),
      kEpsilon));
}

}  // namespace vr
