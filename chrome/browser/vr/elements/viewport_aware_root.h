// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_VIEWPORT_AWARE_ROOT_H_
#define CHROME_BROWSER_VR_ELEMENTS_VIEWPORT_AWARE_ROOT_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/transform.h"

namespace vr {

// This class is the root of all viewport aware elements. It calcuates the
// yaw rotation which all elements need to apply to be visible in current
// viewport.
class VR_UI_EXPORT ViewportAwareRoot : public UiElement {
 public:
  static const float kViewportRotationTriggerDegrees;

  ViewportAwareRoot();
  ~ViewportAwareRoot() override;

  void Reset();
  bool HasVisibleChildren();
  void SetRecenterOnRotate(bool recenter_on_rotate) {
    recenter_on_rotate_ = true;
  }

 protected:
  // Returns true if the rotation was adjusted.
  // Virtual for tests.
  virtual bool AdjustRotationForHeadPose(const gfx::Vector3dF& look_at);
  virtual bool AdjustTranslation(float head_x, float head_z, bool did_rotate);

 private:
  bool OnBeginFrame(const gfx::Transform& head_pose) override;

  float viewport_aware_total_rotation_ = 0.f;
  float x_center = 0;
  float z_center = 0;
  bool children_visible_ = false;
  bool recenter_on_rotate_ = false;

  DISALLOW_COPY_AND_ASSIGN(ViewportAwareRoot);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_VIEWPORT_AWARE_ROOT_H_
