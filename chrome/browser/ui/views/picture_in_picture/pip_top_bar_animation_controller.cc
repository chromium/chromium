// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/pip_top_bar_animation_controller.h"

#include <array>
#include <memory>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/time/time.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/animation/compositor_animation_runner.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// The total duration of the top bar active/inactive animation. Mirrors
// PictureInPictureBrowserFrameView.
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(250);

// The animation durations for the top-right buttons, split into multiple parts
// because some transitions are delayed. Mirror
// PictureInPictureBrowserFrameView.
constexpr std::array<base::TimeDelta, 2>
    kMoveCameraButtonToRightAnimationDurations = {kAnimationDuration * 0.4,
                                                  kAnimationDuration * 0.6};
constexpr std::array<base::TimeDelta, 3>
    kShowBackToTabButtonAnimationDurations = {kAnimationDuration * 0.4,
                                              kAnimationDuration * 0.4,
                                              kAnimationDuration * 0.2};
constexpr std::array<base::TimeDelta, 2>
    kHideBackToTabButtonAnimationDurations = {kAnimationDuration * 0.4,
                                              kAnimationDuration * 0.6};
constexpr std::array<base::TimeDelta, 3> kCloseButtonAnimationDurations = {
    kAnimationDuration * 0.2, kAnimationDuration * 0.4,
    kAnimationDuration * 0.4};

constexpr base::TimeDelta kShowHideAllButtonsAnimationDuration =
    kAnimationDuration;

}  // namespace

PipTopBarAnimationController::PipTopBarAnimationController(
    Delegate* delegate,
    views::View* back_to_tab_button,
    views::View* close_button,
    std::vector<raw_ptr<ContentSettingImageView, VectorExperimental>>
        content_setting_views)
    : delegate_(CHECK_DEREF(delegate)),
      back_to_tab_button_(back_to_tab_button),
      close_image_button_(CHECK_DEREF(close_button)),
      content_setting_views_(std::move(content_setting_views)),
      top_bar_color_animation_(this),
      move_camera_button_to_left_animation_(this),
      move_camera_button_to_right_animation_(gfx::MultiAnimation::Parts{
          {kMoveCameraButtonToRightAnimationDurations[0],
           gfx::Tween::Type::ZERO, 1.0, 1.0},
          {kMoveCameraButtonToRightAnimationDurations[1],
           gfx::Tween::Type::EASE_OUT, 1.0, 0.0}}),
      show_back_to_tab_button_animation_(
          gfx::MultiAnimation::Parts{{kShowBackToTabButtonAnimationDurations[0],
                                      gfx::Tween::Type::ZERO, 0.0, 0.0},
                                     {kShowBackToTabButtonAnimationDurations[1],
                                      gfx::Tween::Type::LINEAR, 0.0, 1.0},
                                     {kShowBackToTabButtonAnimationDurations[2],
                                      gfx::Tween::Type::ZERO, 1.0, 1.0}}),
      hide_back_to_tab_button_animation_(
          gfx::MultiAnimation::Parts{{kHideBackToTabButtonAnimationDurations[0],
                                      gfx::Tween::Type::LINEAR, 1.0, 0.0},
                                     {kHideBackToTabButtonAnimationDurations[1],
                                      gfx::Tween::Type::ZERO, 0.0, 0.0}}),
      show_close_button_animation_(gfx::MultiAnimation::Parts{
          {kCloseButtonAnimationDurations[0], gfx::Tween::Type::ZERO, 0.0, 0.0},
          {kCloseButtonAnimationDurations[1], gfx::Tween::Type::LINEAR, 0.0,
           1.0},
          {kCloseButtonAnimationDurations[2], gfx::Tween::Type::ZERO, 1.0,
           1.0}}),
      hide_close_button_animation_(gfx::MultiAnimation::Parts{
          {kCloseButtonAnimationDurations[0], gfx::Tween::Type::ZERO, 1.0, 1.0},
          {kCloseButtonAnimationDurations[1], gfx::Tween::Type::LINEAR, 1.0,
           0.0},
          {kCloseButtonAnimationDurations[2], gfx::Tween::Type::ZERO, 0.0,
           0.0}}),
      show_all_buttons_animation_(kShowHideAllButtonsAnimationDuration,
                                  gfx::LinearAnimation::kDefaultFrameRate,
                                  this),
      hide_all_buttons_animation_(kShowHideAllButtonsAnimationDuration,
                                  gfx::LinearAnimation::kDefaultFrameRate,
                                  this) {
  // Paint the window-control buttons to layers so their opacity can be animated
  // as they fade in/out on active/inactive transitions. The fixed-size wrappers
  // (created by the frame view) keep reserving the buttons' layout space while
  // they fade.
  if (back_to_tab_button_) {
    back_to_tab_button_->SetPaintToLayer();
    back_to_tab_button_->layer()->SetFillsBoundsOpaquely(false);
  }
  close_image_button_->SetPaintToLayer();
  close_image_button_->layer()->SetFillsBoundsOpaquely(false);

  // Configure the top-bar animations, mirroring
  // PictureInPictureBrowserFrameView. Start in the active state (1.0) because
  // the window is active when first shown.
  top_bar_color_animation_.SetSlideDuration(kAnimationDuration);
  top_bar_color_animation_.SetTweenType(gfx::Tween::LINEAR);
  top_bar_color_animation_.Reset(1.0);

  move_camera_button_to_left_animation_.SetSlideDuration(kAnimationDuration);
  move_camera_button_to_right_animation_.set_continuous(false);
  move_camera_button_to_right_animation_.set_delegate(this);

  if (back_to_tab_button_) {
    show_back_to_tab_button_animation_.set_continuous(false);
    show_back_to_tab_button_animation_.set_delegate(this);
    hide_back_to_tab_button_animation_.set_continuous(false);
    hide_back_to_tab_button_animation_.set_delegate(this);
  }
  show_close_button_animation_.set_continuous(false);
  show_close_button_animation_.set_delegate(this);
  hide_close_button_animation_.set_continuous(false);
  hide_close_button_animation_.set_delegate(this);
}

PipTopBarAnimationController::~PipTopBarAnimationController() = default;

void PipTopBarAnimationController::SetUpAnimationContainer(
    views::Widget* widget) {
  // Drive all top-bar animations from a single container backed by a compositor
  // runner so they update together and in sync with the display, mirroring
  // PictureInPictureBrowserFrameView. Requires the Widget, so it is set up here
  // rather than in the constructor.
  auto* const animation_container = new gfx::AnimationContainer();
  animation_container->SetAnimationRunner(
      std::make_unique<views::CompositorAnimationRunner>(widget));
  top_bar_color_animation_.SetContainer(animation_container);
  move_camera_button_to_left_animation_.SetContainer(animation_container);
  move_camera_button_to_right_animation_.SetContainer(animation_container);
  show_all_buttons_animation_.SetContainer(animation_container);
  hide_all_buttons_animation_.SetContainer(animation_container);
  if (back_to_tab_button_) {
    show_back_to_tab_button_animation_.SetContainer(animation_container);
    hide_back_to_tab_button_animation_.SetContainer(animation_container);
    show_close_button_animation_.SetContainer(animation_container);
    hide_close_button_animation_.SetContainer(animation_container);
  }
}

void PipTopBarAnimationController::SetTopBarActiveStatus(bool active) {
  // Check if the update is needed to avoid redundant animations.
  if (top_bar_active_ == active) {
    return;
  }
  top_bar_active_ = active;

  const bool has_visible_content_setting_views =
      HasAnyVisibleContentSettingViews();

  // Stop the opposite-direction animations first so a rapid in/out toggle
  // doesn't leave earlier animations overriding the new ones. The window-
  // control buttons fade individually when a content-setting icon is visible
  // (so the camera can slide into the freed space) and together otherwise.
  // Mirrors PictureInPictureBrowserFrameView::UpdateTopBarView.
  if (top_bar_active_) {
    move_camera_button_to_right_animation_.Stop();
    if (has_visible_content_setting_views) {
      if (back_to_tab_button_) {
        hide_back_to_tab_button_animation_.Stop();
      }
      hide_close_button_animation_.Stop();
    } else {
      hide_all_buttons_animation_.Stop();
    }

    top_bar_color_animation_.Show();

    // SlideAnimation needs to be reset if only Show() is called.
    move_camera_button_to_left_animation_.Reset(0.0);
    move_camera_button_to_left_animation_.Show();

    if (has_visible_content_setting_views) {
      if (back_to_tab_button_) {
        show_back_to_tab_button_animation_.Start();
      }
      show_close_button_animation_.Start();
    } else {
      show_all_buttons_animation_.Start();
    }
  } else {
    move_camera_button_to_left_animation_.Stop();

    if (has_visible_content_setting_views) {
      if (back_to_tab_button_) {
        show_back_to_tab_button_animation_.Stop();
      }
      show_close_button_animation_.Stop();
    } else {
      show_all_buttons_animation_.Stop();
    }

    top_bar_color_animation_.Hide();
    move_camera_button_to_right_animation_.Start();
    if (has_visible_content_setting_views) {
      if (back_to_tab_button_) {
        hide_back_to_tab_button_animation_.Start();
      }
      hide_close_button_animation_.Start();
    } else {
      hide_all_buttons_animation_.Start();
    }
  }
}

bool PipTopBarAnimationController::HasAnyVisibleContentSettingViews() const {
  for (ContentSettingImageView* view : content_setting_views_) {
    if (view->GetVisible()) {
      return true;
    }
  }
  return false;
}

std::vector<gfx::Animation*>
PipTopBarAnimationController::GetActiveTransitionAnimationsForTesting() {
  CHECK(top_bar_active_);
  std::vector<gfx::Animation*> animations(
      {&top_bar_color_animation_, &move_camera_button_to_left_animation_,
       &show_close_button_animation_});
  if (back_to_tab_button_) {
    animations.push_back(&show_back_to_tab_button_animation_);
  }
  if (!HasAnyVisibleContentSettingViews()) {
    animations.push_back(&show_all_buttons_animation_);
  }
  return animations;
}

std::vector<gfx::Animation*>
PipTopBarAnimationController::GetInactiveTransitionAnimationsForTesting() {
  CHECK(!top_bar_active_);
  std::vector<gfx::Animation*> animations(
      {&top_bar_color_animation_, &move_camera_button_to_right_animation_,
       &hide_close_button_animation_});
  if (back_to_tab_button_) {
    animations.push_back(&hide_back_to_tab_button_animation_);
  }
  if (!HasAnyVisibleContentSettingViews()) {
    animations.push_back(&hide_all_buttons_animation_);
  }
  return animations;
}

void PipTopBarAnimationController::AnimationEnded(
    const gfx::Animation* animation) {
  if (animation != &top_bar_color_animation_) {
    return;
  }
  // Drop the interpolated color and settle on the steady-state foreground.
  current_foreground_color_ = std::nullopt;
  delegate_->ApplyTopBarForegroundColor(
      delegate_->GetTopBarColorProvider()->GetColor(
          top_bar_active_ ? kColorPipWindowForeground
                          : kColorPipWindowForegroundInactive));
}

void PipTopBarAnimationController::AnimationProgressed(
    const gfx::Animation* animation) {
  if (animation == &top_bar_color_animation_) {
    const ui::ColorProvider* const color_provider =
        delegate_->GetTopBarColorProvider();
    const SkColor color = gfx::Tween::ColorValueBetween(
        animation->GetCurrentValue(),
        color_provider->GetColor(kColorPipWindowForegroundInactive),
        color_provider->GetColor(kColorPipWindowForeground));
    current_foreground_color_ = color;
    delegate_->ApplyTopBarForegroundColor(color);
    return;
  }

  if (animation == &move_camera_button_to_left_animation_ ||
      animation == &move_camera_button_to_right_animation_) {
    // Slide the content-setting icon(s) between their resting position (0) and
    // the space occupied by the (now-hidden) window-control buttons.
    int close_and_back_to_tab_button_combined_widths =
        close_image_button_->width();
    if (back_to_tab_button_) {
      close_and_back_to_tab_button_combined_widths +=
          back_to_tab_button_->width();
    }
    for (ContentSettingImageView* view : content_setting_views_) {
      view->SetX(animation->CurrentValueBetween(
          close_and_back_to_tab_button_combined_widths, 0));
    }
    return;
  }

  if (animation == &show_all_buttons_animation_ ||
      animation == &hide_all_buttons_animation_) {
    double animation_current_value = animation->GetCurrentValue();
    // `hide_all_buttons_animation_` is a gfx::LinearAnimation, which runs from
    // 0.0 to 1.0, so invert it to fade the buttons out.
    if (animation == &hide_all_buttons_animation_) {
      animation_current_value = 1.0 - animation_current_value;
    }
    if (back_to_tab_button_) {
      back_to_tab_button_->layer()->SetOpacity(animation_current_value);
    }
    close_image_button_->layer()->SetOpacity(animation_current_value);
    return;
  }

  // The remaining (individual button) animations only run when there is a
  // visible content-setting view; the show/hide-all animations above cover the
  // no-content-setting case.
  if (!HasAnyVisibleContentSettingViews()) {
    return;
  }

  if (animation == &show_back_to_tab_button_animation_ ||
      animation == &hide_back_to_tab_button_animation_) {
    CHECK(back_to_tab_button_);
    back_to_tab_button_->layer()->SetOpacity(animation->GetCurrentValue());
    return;
  }

  CHECK(animation == &show_close_button_animation_ ||
        animation == &hide_close_button_animation_);
  close_image_button_->layer()->SetOpacity(animation->GetCurrentValue());
}
