// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_RESIZER_H_
#define CHROME_BROWSER_VR_ELEMENTS_RESIZER_H_

#include <sstream>

#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

// When enabled, a resizer scales its descendant elements in response to
// trackpad use.
class VR_UI_EXPORT Resizer : public UiElement {
 public:
  Resizer();

  Resizer(const Resizer&) = delete;
  Resizer& operator=(const Resizer&) = delete;

  ~Resizer() override;

  void set_touch_position(const gfx::PointF& position) {
    touchpad_position_ = position;
  }

  void SetTouchingTouchpad(bool touching);

  void SetEnabled(bool enabled);
  void Reset();

#ifndef NDEBUG
  void DumpGeometry(std::ostringstream* os) const override;
#endif

  bool ShouldUpdateWorldSpaceTransform(
      bool parent_transform_changed) const override;

 private:
  gfx::Transform LocalTransform() const override;
  gfx::Transform GetTargetLocalTransform() const override;
  void UpdateTransform(const gfx::Transform& head_pose);
  bool OnBeginFrame(const gfx::Transform& head_pose) override;

  bool enabled_ = false;

  // Initialized via constants.
  float t_;
  float initial_t_;

  gfx::Transform transform_;

  gfx::PointF touchpad_position_;
  gfx::PointF initial_touchpad_position_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_RESIZER_H_
