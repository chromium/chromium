// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_BUTTON_H_
#define CHROME_BROWSER_VR_ELEMENTS_BUTTON_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "chrome/browser/vr/vr_ui_export.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace vr {

class Rect;

// Button has a rounded rectangle as the background and a hit plane as the
// foreground.  When hovered, background and foreground both move forward on Z
// axis.  This matches the Daydream disk-style button. Subclasses may add
// arbitrary non-hit-testable elements as children of the background, if
// desired.
class VR_UI_EXPORT Button : public UiElement {
 public:
  explicit Button(base::RepeatingCallback<void()> click_handler,
                  AudioDelegate* audio_delegate);
  ~Button() override;

  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const final;

  Rect* background() const { return background_; }
  UiElement* hit_plane() const { return hit_plane_; }
  void SetButtonColors(const ButtonColors& colors);
  void SetEnabled(bool enabled);

  void set_click_handler(base::RepeatingCallback<void()> click_handler) {
    click_handler_ = click_handler;
  }

  // TODO(vollick): once all elements are scaled by a ScaledDepthAdjuster, we
  // will never have to change the button hover offset from the default and this
  // method and the associated field can be removed.
  void set_hover_offset(float hover_offset) { hover_offset_ = hover_offset; }

  void set_disabled_sounds(const Sounds& sounds) { disabled_sounds_ = sounds; }

  bool hovered() const { return hovered_; }
  bool down() const { return down_; }
  bool pressed() const { return pressed_; }
  bool enabled() const { return enabled_; }

 protected:
  const ButtonColors& colors() const { return colors_; }
  float hover_offset() const { return hover_offset_; }

  void OnSetDrawPhase() override;
  void OnSetName() override;
  void OnSetSize(const gfx::SizeF& size) override;
  void OnSetCornerRadii(const CornerRadii& radii) override;
  void NotifyClientSizeAnimated(const gfx::SizeF& size,
                                int target_property_id,
                                cc::KeyframeModel* keyframe_model) override;
  virtual void OnStateUpdated();

 private:
  void HandleHoverEnter();
  void HandleHoverMove(const gfx::PointF& position);
  void HandleHoverLeave();
  void HandleButtonDown();
  void HandleButtonUp();

  virtual void OnSetColors(const ButtonColors& colors);

  const Sounds& GetSounds() const override;

  bool down_ = false;
  bool hovered_ = false;
  bool pressed_ = false;
  bool enabled_ = true;
  base::RepeatingCallback<void()> click_handler_;
  ButtonColors colors_;
  float hover_offset_;
  Rect* background_;
  UiElement* hit_plane_;
  Sounds disabled_sounds_;

  DISALLOW_COPY_AND_ASSIGN(Button);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_BUTTON_H_
