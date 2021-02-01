// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_POPUP_H_
#define CHROME_BROWSER_UI_VIEWS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_POPUP_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/animation/animation_delegate_views.h"

namespace views {
class Widget;
}  // namespace views

class FullscreenControlView;

// FullscreenControlPopup is a helper class that holds a FullscreenControlView
// and allows showing and hiding the view with a drop down animation.
class FullscreenControlPopup : public views::AnimationDelegateViews {
 public:
  FullscreenControlPopup(gfx::NativeView parent_view,
                         const base::RepeatingClosure& on_button_pressed,
                         const base::RepeatingClosure& on_visibility_changed);
  ~FullscreenControlPopup() override;

  // Returns the final bottom of the button as a y offset to its parent view.
  static int GetButtonBottomOffset();

  // Shows the indicator with an animation that drops it off the top of
  // |parent_view|.
  // |parent_bounds_in_screen| holds the bounds of |parent_view| in the screen
  // coordinate system.
  void Show(const gfx::Rect& parent_bounds_in_screen);

  // Hides the indicator. If |animated| is true, the indicator will be hidden by
  // the reversed animation of Show(), i.e. the indicator flies to the top of
  // |parent_widget|.
  void Hide(bool animated);

  views::Widget* GetPopupWidget();
  gfx::SlideAnimation* GetAnimationForTesting();

  bool IsAnimating() const;

  // Returns true if the popup is visible on the screen, i.e. Show() has been
  // called and Hide() has never been called since then.
  bool IsVisible() const;

  FullscreenControlView* control_view() { return control_view_; }

 private:
  FullscreenControlPopup(std::unique_ptr<views::Widget> popup,
                         const base::RepeatingClosure& on_visibility_changed);

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  gfx::Rect CalculateBounds(int y_offset) const;

  void OnVisibilityChanged();

  FullscreenControlView* const control_view_;
  const std::unique_ptr<views::Widget> popup_;
  const std::unique_ptr<gfx::SlideAnimation> animation_;

  // The bounds is empty when the popup is not showing.
  gfx::Rect parent_bounds_in_screen_;

  const base::RepeatingClosure on_visibility_changed_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenControlPopup);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FULLSCREEN_CONTROL_FULLSCREEN_CONTROL_POPUP_H_
