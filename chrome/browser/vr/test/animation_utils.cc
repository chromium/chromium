// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/animation_utils.h"

#include "chrome/browser/vr/animation.h"
#include "chrome/browser/vr/elements/ui_element.h"

namespace vr {

std::unique_ptr<cc::KeyframeModel> CreateTransformAnimation(
    int id,
    int group,
    const cc::TransformOperations& from,
    const cc::TransformOperations& to,
    base::TimeDelta duration) {
  std::unique_ptr<cc::KeyframedTransformAnimationCurve> curve(
      cc::KeyframedTransformAnimationCurve::Create());
  curve->AddKeyframe(
      cc::TransformKeyframe::Create(base::TimeDelta(), from, nullptr));
  curve->AddKeyframe(cc::TransformKeyframe::Create(duration, to, nullptr));
  std::unique_ptr<cc::KeyframeModel> keyframe_model(cc::KeyframeModel::Create(
      std::move(curve), id, group,
      cc::KeyframeModel::TargetPropertyId(TargetProperty::TRANSFORM)));
  return keyframe_model;
}

std::unique_ptr<cc::KeyframeModel> CreateBoundsAnimation(
    int id,
    int group,
    const gfx::SizeF& from,
    const gfx::SizeF& to,
    base::TimeDelta duration) {
  std::unique_ptr<cc::KeyframedSizeAnimationCurve> curve(
      cc::KeyframedSizeAnimationCurve::Create());
  curve->AddKeyframe(
      cc::SizeKeyframe::Create(base::TimeDelta(), from, nullptr));
  curve->AddKeyframe(cc::SizeKeyframe::Create(duration, to, nullptr));
  std::unique_ptr<cc::KeyframeModel> keyframe_model(cc::KeyframeModel::Create(
      std::move(curve), id, group,
      cc::KeyframeModel::TargetPropertyId(TargetProperty::BOUNDS)));
  return keyframe_model;
}

std::unique_ptr<cc::KeyframeModel> CreateOpacityAnimation(
    int id,
    int group,
    float from,
    float to,
    base::TimeDelta duration) {
  std::unique_ptr<cc::KeyframedFloatAnimationCurve> curve(
      cc::KeyframedFloatAnimationCurve::Create());
  curve->AddKeyframe(
      cc::FloatKeyframe::Create(base::TimeDelta(), from, nullptr));
  curve->AddKeyframe(cc::FloatKeyframe::Create(duration, to, nullptr));
  std::unique_ptr<cc::KeyframeModel> keyframe_model(cc::KeyframeModel::Create(
      std::move(curve), id, group,
      cc::KeyframeModel::TargetPropertyId(TargetProperty::OPACITY)));
  return keyframe_model;
}

std::unique_ptr<cc::KeyframeModel> CreateBackgroundColorAnimation(
    int id,
    int group,
    SkColor from,
    SkColor to,
    base::TimeDelta duration) {
  std::unique_ptr<cc::KeyframedColorAnimationCurve> curve(
      cc::KeyframedColorAnimationCurve::Create());
  curve->AddKeyframe(
      cc::ColorKeyframe::Create(base::TimeDelta(), from, nullptr));
  curve->AddKeyframe(cc::ColorKeyframe::Create(duration, to, nullptr));
  std::unique_ptr<cc::KeyframeModel> keyframe_model(cc::KeyframeModel::Create(
      std::move(curve), id, group,
      cc::KeyframeModel::TargetPropertyId(TargetProperty::BACKGROUND_COLOR)));
  return keyframe_model;
}

base::TimeTicks MicrosecondsToTicks(uint64_t us) {
  base::TimeTicks to_return;
  return base::TimeDelta::FromMicroseconds(us) + to_return;
}

base::TimeDelta MicrosecondsToDelta(uint64_t us) {
  return base::TimeDelta::FromMicroseconds(us);
}

base::TimeTicks MsToTicks(uint64_t ms) {
  return MicrosecondsToTicks(1000 * ms);
}

base::TimeDelta MsToDelta(uint64_t ms) {
  return MicrosecondsToDelta(1000 * ms);
}

bool IsAnimating(UiElement* element,
                 const std::vector<TargetProperty>& properties) {
  for (auto property : properties) {
    if (!element->IsAnimatingProperty(property))
      return false;
  }
  return true;
}

}  // namespace vr
