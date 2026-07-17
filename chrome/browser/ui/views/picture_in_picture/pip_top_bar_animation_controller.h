// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PIP_TOP_BAR_ANIMATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PIP_TOP_BAR_ANIMATION_CONTROLLER_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/animation/slide_animation.h"

namespace ui {
class ColorProvider;
}  // namespace ui

namespace views {
class View;
class Widget;
}  // namespace views

class ContentSettingImageView;

// Owns and drives the hover animations for the Document Picture-in-Picture top
// bar: the active/inactive foreground color fade, the
// window-control button show/hide opacity fades, and the camera content-setting
// icon slide. Extracted from PictureInPictureBrowserFrameView's top-bar
// animations.
//
// This controller must not outlive the frame view that owns it, nor the views
// it references (they are owned by the same frame view / its widget).
class PipTopBarAnimationController : public gfx::AnimationDelegate {
 public:
  // Implemented by the owning frame view for the parts of the animation that
  // touch state the controller does not own (notably the origin chip).
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Applies `color` to the top-bar foreground elements (window title,
    // content-setting icons, and origin chip). Invoked during the color fade
    // after current_foreground_color() has been updated, so the origin chip
    // label -- which reads the controller's color -- stays in sync.
    virtual void ApplyTopBarForegroundColor(SkColor color) = 0;

    // Returns the color provider used to resolve the active/inactive foreground
    // endpoints. Only queried while animating (i.e. after widget attach).
    virtual const ui::ColorProvider* GetTopBarColorProvider() const = 0;
  };

  // `back_to_tab_button` may be null (when the PiP disallows returning to the
  // opener). `content_setting_views` is copied into this controller (it just
  // holds pointers, so copying is cheap). Both window-control buttons are
  // painted to layers here so their opacity can be animated.
  PipTopBarAnimationController(
      Delegate* delegate,
      views::View* back_to_tab_button,
      views::View* close_button,
      std::vector<raw_ptr<ContentSettingImageView, VectorExperimental>>
          content_setting_views);
  PipTopBarAnimationController(const PipTopBarAnimationController&) = delete;
  PipTopBarAnimationController& operator=(const PipTopBarAnimationController&) =
      delete;
  ~PipTopBarAnimationController() override;

  // Attaches all animations to a single compositor-backed container so they
  // update together and in sync with the display. Must be called once the
  // frame view has been added to its Widget.
  void SetUpAnimationContainer(views::Widget* widget);

  // Transitions the top bar to the active (highlighted, `active` == true) or
  // inactive (dimmed) state, starting the appropriate color-fade, button-fade,
  // and camera-slide animations. No-op if already in the requested state.
  void SetTopBarActiveStatus(bool active);

  bool is_top_bar_active() const { return top_bar_active_; }

  // The interpolated foreground color while the color fade runs; nullopt when
  // settled (the frame view then falls back to the steady-state color). Read by
  // the origin chip so its label tracks the fade.
  std::optional<SkColor> current_foreground_color() const {
    return current_foreground_color_;
  }

  // Returns the animations that run when transitioning to (respectively, away
  // from) the active state, so tests can wait for them to finish. Must only be
  // called while in the matching state (i.e. after the corresponding
  // SetTopBarActiveStatus() call).
  std::vector<gfx::Animation*> GetActiveTransitionAnimationsForTesting();
  std::vector<gfx::Animation*> GetInactiveTransitionAnimationsForTesting();

 private:
  // Grants the frame view's unit test access to the individual animations so it
  // can drive them to completion and assert on their running state.
  friend class DocumentPipFrameViewTest;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // Returns true if any camera/microphone content-setting icon is visible.
  bool HasAnyVisibleContentSettingViews() const;

  const raw_ref<Delegate> delegate_;
  const raw_ptr<views::View> back_to_tab_button_;  // May be null.
  const raw_ref<views::View> close_image_button_;
  const std::vector<raw_ptr<ContentSettingImageView, VectorExperimental>>
      content_setting_views_;

  // 1.0 (in the animations below) is the active (mouse-in) state and 0.0 the
  // inactive (mouse-out) state. The top bar starts active because the window is
  // active when first shown.
  bool top_bar_active_ = true;

  // The interpolated foreground color while `top_bar_color_animation_` runs;
  // nullopt when not animating.
  std::optional<SkColor> current_foreground_color_;

  // Animations for the top bar title and buttons. When the mouse moves in or
  // out of the window, the title/icon colors fade between the active and
  // inactive foreground, the back-to-tab and close buttons fade in or out, and
  // the camera icon slides left or right to fill the space the buttons vacate.
  gfx::SlideAnimation top_bar_color_animation_;
  gfx::SlideAnimation move_camera_button_to_left_animation_;
  gfx::MultiAnimation move_camera_button_to_right_animation_;
  gfx::MultiAnimation show_back_to_tab_button_animation_;
  gfx::MultiAnimation hide_back_to_tab_button_animation_;
  gfx::MultiAnimation show_close_button_animation_;
  gfx::MultiAnimation hide_close_button_animation_;
  gfx::LinearAnimation show_all_buttons_animation_;
  gfx::LinearAnimation hide_all_buttons_animation_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PIP_TOP_BAR_ANIMATION_CONTROLLER_H_
