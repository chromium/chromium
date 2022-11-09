// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_KEYBOARD_DELEGATE_H_
#define CHROME_BROWSER_VR_KEYBOARD_DELEGATE_H_

#include "chrome/browser/vr/vr_base_export.h"

namespace gfx {
class Point3F;
class PointF;
class Transform;
}  // namespace gfx

namespace vr {

class KeyboardUiInterface;
struct CameraModel;
struct TextInputInfo;

class VR_BASE_EXPORT KeyboardDelegate {
 public:
  virtual ~KeyboardDelegate() {}

  virtual void SetUiInterface(KeyboardUiInterface* ui) {}
  virtual void ShowKeyboard() = 0;
  virtual void HideKeyboard() = 0;
  virtual void SetTransform(const gfx::Transform&) = 0;
  virtual bool HitTest(const gfx::Point3F& ray_origin,
                       const gfx::Point3F& ray_target,
                       gfx::Point3F* hit_position) = 0;
  virtual void OnBeginFrame() {}
  virtual void Draw(const CameraModel&) = 0;
  virtual bool SupportsSelection() = 0;

  virtual void OnTouchStateUpdated(bool is_touching,
                                   const gfx::PointF& touch_position) {}
  virtual void OnHoverEnter(const gfx::PointF& position) {}
  virtual void OnHoverLeave() {}
  virtual void OnHoverMove(const gfx::PointF& position) {}
  virtual void OnButtonDown(const gfx::PointF& position) {}
  virtual void OnButtonUp(const gfx::PointF& position) {}

  // Called to update GVR keyboard with the given text input info.
  virtual void UpdateInput(const TextInputInfo& info) {}
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_KEYBOARD_DELEGATE_H_
