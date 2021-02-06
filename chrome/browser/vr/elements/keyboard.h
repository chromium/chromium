// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_KEYBOARD_H_
#define CHROME_BROWSER_VR_ELEMENTS_KEYBOARD_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/renderers/base_renderer.h"
#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

// Represents the virtual keyboard. This element is a proxy to the
// platform-specific keyboard implementation.
class VR_UI_EXPORT Keyboard : public UiElement {
 public:
  Keyboard();
  ~Keyboard() override;

  // The gvr keyboard requires that we advance its frame after initilization,
  // for example, regardless of visibility.
  void AdvanceKeyboardFrameIfNeeded();
  void SetKeyboardDelegate(KeyboardDelegate* keyboard_delegate);
  void OnTouchStateUpdated(bool is_touching, const gfx::PointF& touch_position);
  void HitTest(const HitTestRequest& request,
               HitTestResult* result) const final;
  void NotifyClientFloatAnimated(float value,
                                 int target_property_id,
                                 cc::KeyframeModel* keyframe_model) override;

  void OnHoverEnter(const gfx::PointF& position,
                    base::TimeTicks timestamp) override;
  void OnHoverLeave(base::TimeTicks timestamp) override;
  void OnHoverMove(const gfx::PointF& position,
                   base::TimeTicks timestamp) override;
  void OnButtonDown(const gfx::PointF& position,
                    base::TimeTicks timestamp) override;
  void OnButtonUp(const gfx::PointF& position,
                  base::TimeTicks timestamp) override;

  class Renderer : public BaseRenderer {
   public:
    Renderer();
    ~Renderer() override;
    void Draw(const CameraModel& camera_model, KeyboardDelegate* delegate);

   private:
    DISALLOW_COPY_AND_ASSIGN(Renderer);
  };

 private:
  bool OnBeginFrame(const gfx::Transform& head_pose) override;
  void OnUpdatedWorldSpaceTransform() override;
  void Render(UiElementRenderer* renderer,
              const CameraModel& camera_model) const final;
  void OnSetFocusable() override;

  void UpdateDelegateVisibility();

  KeyboardDelegate* delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Keyboard);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_KEYBOARD_H_
