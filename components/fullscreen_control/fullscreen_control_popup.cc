// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fullscreen_control/fullscreen_control_popup.h"

#include <memory>

#include "base/functional/bind.h"
#include "components/fullscreen_control/fullscreen_control_view.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"

namespace {

// Offsets with respect to the top y coordinate of the parent widget.
constexpr int kFinalOffset = 45;

constexpr float kInitialOpacity = 0.1f;
constexpr float kFinalOpacity = 1.f;

// Creates a Widget containing an FullscreenControlView.
std::unique_ptr<views::Widget> CreatePopupWidget(
    gfx::NativeView parent_view,
    std::unique_ptr<FullscreenControlView> view) {
  // Initialize the popup.
  std::unique_ptr<views::Widget> popup = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.z_order = ui::ZOrderLevel::kSecuritySurface;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.parent = parent_view;
  popup->Init(std::move(params));
  popup->SetContentsView(std::move(view));

  return popup;
}

}  // namespace

FullscreenControlPopup::FullscreenControlPopup(
    gfx::NativeView parent_view,
    const base::RepeatingClosure& on_button_pressed,
    const base::RepeatingClosure& on_visibility_changed)
    : FullscreenControlPopup(
          CreatePopupWidget(
              parent_view,
              std::make_unique<FullscreenControlView>(on_button_pressed)),
          on_visibility_changed) {}

FullscreenControlPopup::~FullscreenControlPopup() {}

// static
int FullscreenControlPopup::GetButtonBottomOffset() {
  return kFinalOffset + FullscreenControlView::kCircleButtonDiameter;
}

void FullscreenControlPopup::Show(const gfx::Rect& parent_bounds_in_screen) {
  if (IsVisible())
    return;

  parent_bounds_in_screen_ = parent_bounds_in_screen;

  animation_->SetSlideDuration(base::Milliseconds(300));
  animation_->Show();

  // The default animation progress is 0. Call it once here then show the popup
  // to prevent potential flickering.
  AnimationProgressed(animation_.get());
  popup_->Show();
}

void FullscreenControlPopup::Hide(bool animated) {
  if (!IsVisible())
    return;

  if (animated) {
    animation_->SetSlideDuration(base::Milliseconds(150));
    animation_->Hide();
    return;
  }

  animation_->Reset(0);
  AnimationEnded(animation_.get());
}

views::Widget* FullscreenControlPopup::GetPopupWidget() {
  return popup_.get();
}

gfx::SlideAnimation* FullscreenControlPopup::GetAnimationForTesting() {
  return animation_.get();
}

bool FullscreenControlPopup::IsAnimating() const {
  return animation_->is_animating();
}

bool FullscreenControlPopup::IsVisible() const {
  return popup_->IsVisible();
}

FullscreenControlPopup::FullscreenControlPopup(
    std::unique_ptr<views::Widget> popup,
    const base::RepeatingClosure& on_visibility_changed)
    : AnimationDelegateViews(popup->GetRootView()),
      popup_(std::move(popup)),
      control_view_(
          static_cast<FullscreenControlView*>(popup_->GetContentsView())),
      animation_(std::make_unique<gfx::SlideAnimation>(this)),
      on_visibility_changed_(on_visibility_changed) {
  DCHECK(on_visibility_changed_);
  animation_->Reset(0);
}

void FullscreenControlPopup::AnimationProgressed(
    const gfx::Animation* animation) {
  float opacity = static_cast<float>(
      animation_->CurrentValueBetween(kInitialOpacity, kFinalOpacity));
  popup_->SetOpacity(opacity);

  int initial_offset = -control_view_->GetPreferredSize({}).height();
  popup_->SetBounds(CalculateBounds(
      animation_->CurrentValueBetween(initial_offset, kFinalOffset)));
}

void FullscreenControlPopup::AnimationEnded(const gfx::Animation* animation) {
  if (animation_->GetCurrentValue() == 0.0) {
    // It's the end of the reversed animation. Just hide the popup in this case.
    parent_bounds_in_screen_ = gfx::Rect();
    popup_->Hide();
  } else {
    AnimationProgressed(animation);
  }
  OnVisibilityChanged();
}

gfx::Rect FullscreenControlPopup::CalculateBounds(int y_offset) const {
  if (parent_bounds_in_screen_.IsEmpty())
    return gfx::Rect();

  gfx::Point origin(parent_bounds_in_screen_.CenterPoint().x() -
                        control_view_->GetPreferredSize({}).width() / 2,
                    parent_bounds_in_screen_.y() + y_offset);
  return {origin, control_view_->GetPreferredSize({})};
}

void FullscreenControlPopup::OnVisibilityChanged() {
  on_visibility_changed_.Run();
}
