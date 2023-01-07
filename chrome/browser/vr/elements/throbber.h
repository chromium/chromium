// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_THROBBER_H_
#define CHROME_BROWSER_VR_ELEMENTS_THROBBER_H_

#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/geometry/transform_operation.h"

namespace vr {

// A throbber renders a fading and pulsing rect through animating element's
// scale and opacity.
class VR_UI_EXPORT Throbber : public Rect {
 public:
  Throbber();

  Throbber(const Throbber&) = delete;
  Throbber& operator=(const Throbber&) = delete;

  ~Throbber() override;

  void OnFloatAnimated(const float& value,
                       int target_property_id,
                       gfx::KeyframeModel* keyframe_model) override;

  void SetCircleGrowAnimationEnabled(bool enabled);

 private:
  gfx::TransformOperation scale_before_animation_;
  float opacity_before_animation_ = 0.f;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_THROBBER_H_
