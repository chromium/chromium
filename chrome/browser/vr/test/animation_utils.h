// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_ANIMATION_UTILS_H_
#define CHROME_BROWSER_VR_TEST_ANIMATION_UTILS_H_

#include <vector>

#include "cc/animation/keyframe_model.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "chrome/browser/vr/target_property.h"

namespace vr {

class UiElement;

std::unique_ptr<cc::KeyframeModel> CreateTransformAnimation(
    int id,
    int group,
    const gfx::TransformOperations& from,
    const gfx::TransformOperations& to,
    base::TimeDelta duration);

std::unique_ptr<cc::KeyframeModel> CreateBoundsAnimation(
    int id,
    int group,
    const gfx::SizeF& from,
    const gfx::SizeF& to,
    base::TimeDelta duration);

std::unique_ptr<cc::KeyframeModel> CreateOpacityAnimation(
    int id,
    int group,
    float from,
    float to,
    base::TimeDelta duration);

std::unique_ptr<cc::KeyframeModel> CreateBackgroundColorAnimation(
    int id,
    int group,
    SkColor from,
    SkColor to,
    base::TimeDelta duration);

base::TimeTicks MicrosecondsToTicks(uint64_t us);
base::TimeDelta MicrosecondsToDelta(uint64_t us);

base::TimeTicks MsToTicks(uint64_t us);
base::TimeDelta MsToDelta(uint64_t us);

// Returns true if the given properties are being animated by the element.
bool IsAnimating(UiElement* element,
                 const std::vector<TargetProperty>& properties);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_ANIMATION_UTILS_H_
