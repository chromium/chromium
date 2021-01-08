// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/animation.h"

#include <algorithm>

#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_target.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "chrome/browser/vr/elements/ui_element.h"

namespace vr {

namespace {

static constexpr float kTolerance = 1e-5f;

static int s_next_keyframe_model_id = 1;
static int s_next_group_id = 1;

void ReverseKeyframeModel(base::TimeTicks monotonic_time,
                          cc::KeyframeModel* keyframe_model) {
  keyframe_model->set_direction(keyframe_model->direction() ==
                                        cc::KeyframeModel::Direction::NORMAL
                                    ? cc::KeyframeModel::Direction::REVERSE
                                    : cc::KeyframeModel::Direction::NORMAL);
  // Our goal here is to reverse the given keyframe_model. That is, if
  // we're 20% of the way through the keyframe_model in the forward direction,
  // we'd like to be 80% of the way of the reversed keyframe model (so it will
  // end quickly).
  //
  // We can modify our "progress" through an animation by modifying the "time
  // offset", a value added to the current time by the animation system before
  // applying any other adjustments.
  //
  // Let our start time be s, our current time be t, and our final time (or
  // duration) be d. After reversing the keyframe_model, we would like to start
  // sampling from d - t as depicted below.
  //
  //  Forward:
  //  s    t                         d
  //  |----|-------------------------|
  //
  //  Reversed:
  //  s                         t    d
  //  |----|--------------------|----|
  //       -----time-offset----->
  //
  // Now, if we let o represent our desired offset, we need to ensure that
  //   t = d - (o + t)
  //
  // That is, sampling at the current time in either the forward or reverse
  // curves must result in the same value, otherwise we'll get jank.
  //
  // This implies that,
  //   0 = d - o - 2t
  //   o = d - 2t
  //
  // Now if there was a previous offset, we must adjust d by that offset before
  // performing this computation, so it becomes d - o_old - 2t:
  keyframe_model->set_time_offset(
      keyframe_model->curve()->Duration() - keyframe_model->time_offset() -
      (2 * (monotonic_time - keyframe_model->start_time())));
}

std::unique_ptr<cc::CubicBezierTimingFunction>
CreateTransitionTimingFunction() {
  return cc::CubicBezierTimingFunction::CreatePreset(
      cc::CubicBezierTimingFunction::EaseType::EASE);
}

base::TimeDelta GetStartTime(cc::KeyframeModel* keyframe_model) {
  if (keyframe_model->direction() == cc::KeyframeModel::Direction::NORMAL) {
    return base::TimeDelta();
  }
  return keyframe_model->curve()->Duration();
}

base::TimeDelta GetEndTime(cc::KeyframeModel* keyframe_model) {
  if (keyframe_model->direction() == cc::KeyframeModel::Direction::REVERSE) {
    return base::TimeDelta();
  }
  return keyframe_model->curve()->Duration();
}

bool SufficientlyEqual(float lhs, float rhs) {
  return base::IsApproximatelyEqual(lhs, rhs, kTolerance);
}

bool SufficientlyEqual(const cc::TransformOperations& lhs,
                       const cc::TransformOperations& rhs) {
  return lhs.ApproximatelyEqual(rhs, kTolerance);
}

bool SufficientlyEqual(const gfx::SizeF& lhs, const gfx::SizeF& rhs) {
  return base::IsApproximatelyEqual(lhs.width(), rhs.width(), kTolerance) &&
         base::IsApproximatelyEqual(lhs.height(), rhs.height(), kTolerance);
}

bool SufficientlyEqual(SkColor lhs, SkColor rhs) {
  return lhs == rhs;
}

template <typename T>
struct AnimationTraits {};

#define DEFINE_ANIMATION_TRAITS(value_type, name, notify_name)                \
  template <>                                                                 \
  struct AnimationTraits<value_type> {                                        \
    typedef value_type ValueType;                                             \
    typedef cc::name##AnimationCurve CurveType;                               \
    typedef cc::Keyframed##name##AnimationCurve KeyframedCurveType;           \
    typedef cc::name##Keyframe KeyframeType;                                  \
    static const CurveType* ToDerivedCurve(const cc::AnimationCurve& curve) { \
      return curve.To##name##AnimationCurve();                                \
    }                                                                         \
    static void NotifyClientValueAnimated(                                    \
        cc::AnimationTarget* animation_target,                                \
        const ValueType& target_value,                                        \
        int target_property) {                                                \
      animation_target->NotifyClient##notify_name##Animated(                  \
          target_value, target_property, nullptr);                            \
    }                                                                         \
  }

DEFINE_ANIMATION_TRAITS(float, Float, Float);
DEFINE_ANIMATION_TRAITS(cc::TransformOperations,
                        Transform,
                        TransformOperations);
DEFINE_ANIMATION_TRAITS(gfx::SizeF, Size, Size);
DEFINE_ANIMATION_TRAITS(SkColor, Color, Color);

#undef DEFINE_ANIMATION_TRAITS

}  // namespace

int Animation::GetNextKeyframeModelId() {
  return s_next_keyframe_model_id++;
}

int Animation::GetNextGroupId() {
  return s_next_group_id++;
}

Animation::Animation() {}
Animation::~Animation() {}

void Animation::AddKeyframeModel(
    std::unique_ptr<cc::KeyframeModel> keyframe_model) {
  keyframe_models_.push_back(std::move(keyframe_model));
}

void Animation::RemoveKeyframeModel(int keyframe_model_id) {
  base::EraseIf(keyframe_models_,
                [keyframe_model_id](
                    const std::unique_ptr<cc::KeyframeModel>& keyframe_model) {
                  return keyframe_model->id() == keyframe_model_id;
                });
}

void Animation::RemoveKeyframeModels(int target_property) {
  base::EraseIf(keyframe_models_,
                [target_property](
                    const std::unique_ptr<cc::KeyframeModel>& keyframe_model) {
                  return keyframe_model->target_property_type() ==
                         target_property;
                });
}

void Animation::Tick(base::TimeTicks monotonic_time) {
  TickInternal(monotonic_time, true);
}

void Animation::TickInternal(base::TimeTicks monotonic_time,
                             bool include_infinite_animations) {
  DCHECK(target_);

  StartKeyframeModels(monotonic_time, include_infinite_animations);

  for (auto& keyframe_model : keyframe_models_) {
    if (!include_infinite_animations &&
        keyframe_model->iterations() == std::numeric_limits<double>::infinity())
      continue;
    cc::KeyframeEffect::TickKeyframeModel(monotonic_time, keyframe_model.get(),
                                          target_);
  }

  // Remove finished keyframe_models.
  base::EraseIf(keyframe_models_,
                [monotonic_time](
                    const std::unique_ptr<cc::KeyframeModel>& keyframe_model) {
                  return !keyframe_model->is_finished() &&
                         keyframe_model->IsFinishedAt(monotonic_time);
                });

  StartKeyframeModels(monotonic_time, include_infinite_animations);
}

void Animation::FinishAll() {
  base::TimeTicks now = base::TimeTicks::Now();
  const bool include_infinite_animations = false;
  TickInternal(now, include_infinite_animations);
  TickInternal(base::TimeTicks::Max(), include_infinite_animations);
#ifndef NDEBUG
  for (auto& keyframe_model : keyframe_models_) {
    DCHECK_EQ(std::numeric_limits<double>::infinity(),
              keyframe_model->iterations());
  }
#endif
}

void Animation::SetTransitionedProperties(const std::set<int>& properties) {
  transition_.target_properties = properties;
}

void Animation::SetTransitionDuration(base::TimeDelta delta) {
  transition_.duration = delta;
}

void Animation::TransitionFloatTo(base::TimeTicks monotonic_time,
                                  int target_property,
                                  float current,
                                  float target) {
  TransitionValueTo<float>(monotonic_time, target_property, current, target);
}

void Animation::TransitionTransformOperationsTo(
    base::TimeTicks monotonic_time,
    int target_property,
    const cc::TransformOperations& current,
    const cc::TransformOperations& target) {
  TransitionValueTo<cc::TransformOperations>(monotonic_time, target_property,
                                             current, target);
}

void Animation::TransitionSizeTo(base::TimeTicks monotonic_time,
                                 int target_property,
                                 const gfx::SizeF& current,
                                 const gfx::SizeF& target) {
  TransitionValueTo<gfx::SizeF>(monotonic_time, target_property, current,
                                target);
}

void Animation::TransitionColorTo(base::TimeTicks monotonic_time,
                                  int target_property,
                                  SkColor current,
                                  SkColor target) {
  TransitionValueTo<SkColor>(monotonic_time, target_property, current, target);
}

bool Animation::IsAnimatingProperty(int property) const {
  for (auto& keyframe_model : keyframe_models_) {
    if (keyframe_model->target_property_type() == property)
      return true;
  }
  return false;
}

float Animation::GetTargetFloatValue(int target_property,
                                     float default_value) const {
  return GetTargetValue<float>(target_property, default_value);
}

cc::TransformOperations Animation::GetTargetTransformOperationsValue(
    int target_property,
    const cc::TransformOperations& default_value) const {
  return GetTargetValue<cc::TransformOperations>(target_property,
                                                 default_value);
}

gfx::SizeF Animation::GetTargetSizeValue(
    int target_property,
    const gfx::SizeF& default_value) const {
  return GetTargetValue<gfx::SizeF>(target_property, default_value);
}

SkColor Animation::GetTargetColorValue(int target_property,
                                       SkColor default_value) const {
  return GetTargetValue<SkColor>(target_property, default_value);
}

void Animation::StartKeyframeModels(base::TimeTicks monotonic_time,
                                    bool include_infinite_animations) {
  cc::TargetProperties animated_properties;
  for (auto& keyframe_model : keyframe_models_) {
    if (!include_infinite_animations &&
        keyframe_model->iterations() == std::numeric_limits<double>::infinity())
      continue;
    if (keyframe_model->run_state() == cc::KeyframeModel::RUNNING ||
        keyframe_model->run_state() == cc::KeyframeModel::PAUSED) {
      animated_properties[keyframe_model->target_property_type()] = true;
    }
  }
  for (auto& keyframe_model : keyframe_models_) {
    if (!include_infinite_animations &&
        keyframe_model->iterations() == std::numeric_limits<double>::infinity())
      continue;
    if (!animated_properties[keyframe_model->target_property_type()] &&
        keyframe_model->run_state() ==
            cc::KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY) {
      animated_properties[keyframe_model->target_property_type()] = true;
      keyframe_model->SetRunState(cc::KeyframeModel::RUNNING, monotonic_time);
      keyframe_model->set_start_time(monotonic_time);
    }
  }
}

template <typename ValueType>
void Animation::TransitionValueTo(base::TimeTicks monotonic_time,
                                  int target_property,
                                  const ValueType& current,
                                  const ValueType& target) {
  DCHECK(target_);

  if (transition_.target_properties.find(target_property) ==
      transition_.target_properties.end()) {
    AnimationTraits<ValueType>::NotifyClientValueAnimated(target_, target,
                                                          target_property);
    return;
  }

  cc::KeyframeModel* running_keyframe_model =
      GetRunningKeyframeModelForProperty(target_property);

  ValueType effective_current = current;

  if (running_keyframe_model) {
    const auto* curve = AnimationTraits<ValueType>::ToDerivedCurve(
        *running_keyframe_model->curve());

    if (running_keyframe_model->IsFinishedAt(monotonic_time)) {
      effective_current = curve->GetValue(GetEndTime(running_keyframe_model));
    } else {
      if (SufficientlyEqual(
              target, curve->GetValue(GetEndTime(running_keyframe_model)))) {
        return;
      }
      if (SufficientlyEqual(
              target, curve->GetValue(GetStartTime(running_keyframe_model)))) {
        ReverseKeyframeModel(monotonic_time, running_keyframe_model);
        return;
      }
    }
  } else if (SufficientlyEqual(target, current)) {
    return;
  }

  RemoveKeyframeModels(target_property);

  std::unique_ptr<typename AnimationTraits<ValueType>::KeyframedCurveType>
      curve(AnimationTraits<ValueType>::KeyframedCurveType::Create());

  curve->AddKeyframe(AnimationTraits<ValueType>::KeyframeType::Create(
      base::TimeDelta(), effective_current, CreateTransitionTimingFunction()));

  curve->AddKeyframe(AnimationTraits<ValueType>::KeyframeType::Create(
      transition_.duration, target, CreateTransitionTimingFunction()));

  AddKeyframeModel(cc::KeyframeModel::Create(
      std::move(curve), GetNextKeyframeModelId(), GetNextGroupId(),
      cc::KeyframeModel::TargetPropertyId(target_property)));
}

cc::KeyframeModel* Animation::GetRunningKeyframeModelForProperty(
    int target_property) const {
  for (auto& keyframe_model : keyframe_models_) {
    if ((keyframe_model->run_state() == cc::KeyframeModel::RUNNING ||
         keyframe_model->run_state() == cc::KeyframeModel::PAUSED) &&
        keyframe_model->target_property_type() == target_property) {
      return keyframe_model.get();
    }
  }
  return nullptr;
}

cc::KeyframeModel* Animation::GetKeyframeModelForProperty(
    int target_property) const {
  for (auto& keyframe_model : keyframe_models_) {
    if (keyframe_model->target_property_type() == target_property) {
      return keyframe_model.get();
    }
  }
  return nullptr;
}

template <typename ValueType>
ValueType Animation::GetTargetValue(int target_property,
                                    const ValueType& default_value) const {
  cc::KeyframeModel* running_keyframe_model =
      GetKeyframeModelForProperty(target_property);
  if (!running_keyframe_model) {
    return default_value;
  }
  const auto* curve = AnimationTraits<ValueType>::ToDerivedCurve(
      *running_keyframe_model->curve());
  return curve->GetValue(GetEndTime(running_keyframe_model));
}

}  // namespace vr
