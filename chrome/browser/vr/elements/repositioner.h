// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_REPOSITIONER_H_
#define CHROME_BROWSER_VR_ELEMENTS_REPOSITIONER_H_

#include <sstream>

#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

// A repositioner adjusts the position of its children by rotation. The
// reposition is driven by controller. It maintains a transform and updates it
// when enabled as either the head or the controller move. In a nutshell, it
// rotates the elements per the angular change in the controller orientation,
// adjusting the up vector of the content so that it aligns with the head's up
// vector. As the window is being repositioned, we rotate it so that it remains
// pointing upward.
class VR_UI_EXPORT Repositioner : public UiElement {
 public:
  Repositioner();

  Repositioner(const Repositioner&) = delete;
  Repositioner& operator=(const Repositioner&) = delete;

  ~Repositioner() override;

  void set_laser_direction(const gfx::Vector3dF& laser_direction) {
    laser_direction_ = laser_direction;
  }

  void SetEnabled(bool enabled);
  void Reset();

  // This method returns true if the user has repositioned far enough that we
  // should consider it an intentional drag (and the UI may want to respond
  // different if this has happened).
  bool HasMovedBeyondThreshold() const { return has_moved_beyond_threshold_; }

  bool ShouldUpdateWorldSpaceTransform(
      bool parent_transform_changed) const override;

 private:
  gfx::Transform LocalTransform() const override;
  gfx::Transform GetTargetLocalTransform() const override;
  void UpdateTransform(const gfx::Transform& head_pose);
  bool OnBeginFrame(const gfx::Transform& head_pose) override;
#ifndef NDEBUG
  void DumpGeometry(std::ostringstream* os) const override;
#endif

  bool enabled_ = false;
  bool has_moved_beyond_threshold_ = false;
  bool reset_yaw_ = false;
  gfx::Transform transform_;
  gfx::Vector3dF laser_direction_;

  gfx::Transform initial_transform_;
  gfx::Vector3dF initial_laser_direction_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_REPOSITIONER_H_
