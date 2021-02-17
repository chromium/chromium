// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ANIMATION_H_
#define CHROME_BROWSER_VR_ANIMATION_H_

#include <set>
#include <vector>

#include "base/macros.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/keyframe_model.h"
#include "chrome/browser/vr/transition.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class SizeF;
class TransformOperations;
}  // namespace gfx

namespace vr {

// This is a simplified version of the cc::Animation. Its sole purpose is the
// management of its collection of KeyframeModels. Ticking them, updating their
// state, and deleting them as required.
//
// TODO(vollick): if cc::KeyframeModel and friends move into gfx/, then this
// class should follow suit. As such, it should not absorb any vr-specific
// functionality.
class VR_UI_EXPORT Animation final {
 public:
  static int GetNextKeyframeModelId();
  static int GetNextGroupId();

  Animation();
  ~Animation();

  void AddKeyframeModel(std::unique_ptr<cc::KeyframeModel> keyframe_model);
  void RemoveKeyframeModel(int keyframe_model_id);
  void RemoveKeyframeModels(int target_property);

  void Tick(base::TimeTicks monotonic_time);

  // This ticks all keyframe models until they are complete.
  void FinishAll();

  using KeyframeModels = std::vector<std::unique_ptr<cc::KeyframeModel>>;
  const KeyframeModels& keyframe_models() { return keyframe_models_; }

  // The transition is analogous to CSS transitions. When configured, the
  // transition object will cause subsequent calls the corresponding
  // TransitionXXXTo functions to induce transition animations.
  const Transition& transition() const { return transition_; }
  void set_transition(const Transition& transition) {
    transition_ = transition;
  }

  void SetTransitionedProperties(const std::set<int>& properties);
  void SetTransitionDuration(base::TimeDelta delta);

  // TODO(754820): Remove duplicate code from the transition functions
  // by using templates.
  void TransitionFloatTo(cc::FloatAnimationCurve::Target* target,
                         base::TimeTicks monotonic_time,
                         int target_property,
                         float from,
                         float to);
  void TransitionTransformOperationsTo(
      cc::TransformAnimationCurve::Target* target,
      base::TimeTicks monotonic_time,
      int target_property,
      const gfx::TransformOperations& from,
      const gfx::TransformOperations& to);
  void TransitionSizeTo(cc::SizeAnimationCurve::Target* target,
                        base::TimeTicks monotonic_time,
                        int target_property,
                        const gfx::SizeF& from,
                        const gfx::SizeF& to);
  void TransitionColorTo(cc::ColorAnimationCurve::Target* target,
                         base::TimeTicks monotonic_time,
                         int target_property,
                         SkColor from,
                         SkColor to);

  bool IsAnimatingProperty(int property) const;

  float GetTargetFloatValue(int target_property, float default_value) const;
  gfx::TransformOperations GetTargetTransformOperationsValue(
      int target_property,
      const gfx::TransformOperations& default_value) const;
  gfx::SizeF GetTargetSizeValue(int target_property,
                                const gfx::SizeF& default_value) const;
  SkColor GetTargetColorValue(int target_property, SkColor default_value) const;
  cc::KeyframeModel* GetRunningKeyframeModelForProperty(
      int target_property) const;

 private:
  void TickInternal(base::TimeTicks monotonic_time,
                    bool include_infinite_animations);
  void StartKeyframeModels(base::TimeTicks monotonic_time,
                           bool include_infinite_animations);
  cc::KeyframeModel* GetKeyframeModelForProperty(int target_property) const;
  template <typename ValueType>
  ValueType GetTargetValue(int target_property,
                           const ValueType& default_value) const;

  KeyframeModels keyframe_models_;
  Transition transition_;

  DISALLOW_COPY_AND_ASSIGN(Animation);
};

}  // namespace vr

#endif  //  CHROME_BROWSER_VR_ANIMATION_H_
