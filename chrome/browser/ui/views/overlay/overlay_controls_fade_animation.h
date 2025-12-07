// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_CONTROLS_FADE_ANIMATION_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_CONTROLS_FADE_ANIMATION_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/animation/linear_animation.h"

namespace views {
class View;
}  // namespace views

// Animates a VideoOverlayWindowViews's controls to fade in or out.
class OverlayControlsFadeAnimation : public gfx::LinearAnimation {
 public:
  enum class Type {
    // Will animate the controls to a shown state.
    kToShown,

    // Will animate the controls to a hidden state.
    kToHidden,
  };

  // Every element of `controls` MUST outlive `this`.
  OverlayControlsFadeAnimation(
      const std::vector<raw_ptr<views::View>>& controls,
      Type type);
  OverlayControlsFadeAnimation(const OverlayControlsFadeAnimation&) = delete;
  OverlayControlsFadeAnimation& operator=(const OverlayControlsFadeAnimation&) =
      delete;
  ~OverlayControlsFadeAnimation() override;

  // gfx::LinearAnimation:
  void AnimateToState(double state) override;

  Type type() const { return type_; }

 private:
  const std::vector<raw_ptr<views::View>> controls_;
  const Type type_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_CONTROLS_FADE_ANIMATION_H_
