// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/throbber.h"

#include "chrome/browser/vr/target_property.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"
#include "ui/gfx/animation/keyframe/timing_function.h"
#include "ui/gfx/geometry/transform_operations.h"

namespace vr {

namespace {
constexpr float kStartScale = 1.0f;
constexpr float kEndScale = 2.0f;
constexpr int kCircleGrowAnimationTimeMs = 1000;
}  // namespace

Throbber::Throbber() = default;
Throbber::~Throbber() = default;

void Throbber::OnFloatAnimated(const float& value,
                               int target_property_id,
                               gfx::KeyframeModel* animation) {
  if (target_property_id == CIRCLE_GROW) {
    DCHECK(!IsAnimatingProperty(TRANSFORM));
    DCHECK(!IsAnimatingProperty(OPACITY));

    SetScale(scale_before_animation_.scale.x * value,
             scale_before_animation_.scale.y * value,
             scale_before_animation_.scale.z);
    float animation_progress =
        (value - kStartScale) / (kEndScale - kStartScale);
    SetOpacity(opacity_before_animation_ * (1.0 - animation_progress));
    return;
  }
  Rect::OnFloatAnimated(value, target_property_id, animation);
}

void Throbber::SetCircleGrowAnimationEnabled(bool enabled) {
  if (!enabled) {
    if (animator().IsAnimatingProperty(CIRCLE_GROW)) {
      SetOpacity(opacity_before_animation_);
      SetScale(scale_before_animation_.scale.x, scale_before_animation_.scale.y,
               scale_before_animation_.scale.z);
    }
    animator().RemoveKeyframeModels(CIRCLE_GROW);
    return;
  }

  if (animator().IsAnimatingProperty(CIRCLE_GROW))
    return;

  scale_before_animation_ = GetTargetTransform().at(kScaleIndex);
  opacity_before_animation_ = GetTargetOpacity();
  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> curve(
      gfx::KeyframedFloatAnimationCurve::Create());

  curve->AddKeyframe(
      gfx::FloatKeyframe::Create(base::TimeDelta(), kStartScale, nullptr));
  curve->AddKeyframe(gfx::FloatKeyframe::Create(
      base::Milliseconds(kCircleGrowAnimationTimeMs), kEndScale, nullptr));
  curve->set_target(this);

  std::unique_ptr<gfx::KeyframeModel> keyframe_model(gfx::KeyframeModel::Create(
      std::move(curve), gfx::KeyframeEffect::GetNextKeyframeModelId(),
      CIRCLE_GROW));
  keyframe_model->set_iterations(std::numeric_limits<double>::infinity());
  AddKeyframeModel(std::move(keyframe_model));
}

}  // namespace vr
