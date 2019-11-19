// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"

#include <memory>

#include "base/stl_util.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

// static
const char ToolbarIconContainerView::kToolbarIconContainerViewClassName[] =
    "ToolbarIconContainerView";

ToolbarIconContainerView::ToolbarIconContainerView(bool uses_highlight)
    : uses_highlight_(uses_highlight) {
  views::AnimatingLayoutManager* animating_layout =
      SetLayoutManager(std::make_unique<views::AnimatingLayoutManager>());
  animating_layout->SetShouldAnimateBounds(true);
  auto* flex_layout = animating_layout->SetTargetLayoutManager(
      std::make_unique<views::FlexLayout>());
  flex_layout->SetCollapseMargins(true)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets(0, GetLayoutConstant(TOOLBAR_ELEMENT_PADDING)));
}

ToolbarIconContainerView::~ToolbarIconContainerView() {
  // As childred might be Observers of |this|, we need to destroy them before
  // destroying |observers_|.
  RemoveAllChildViews(true);
}

void ToolbarIconContainerView::AddMainButton(views::Button* main_button) {
  DCHECK(!main_button_);
  main_button->AddObserver(this);
  main_button->AddButtonObserver(this);
  main_button_ = main_button;
  AddChildView(main_button_);
}

void ToolbarIconContainerView::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void ToolbarIconContainerView::RemoveObserver(const Observer* obs) {
  observers_.RemoveObserver(obs);
}

void ToolbarIconContainerView::OnHighlightChanged(
    views::Button* observed_button,
    bool highlighted) {
  // We don't care about the main button being highlighted.
  if (observed_button == main_button_)
    return;

  if (highlighted) {
    DCHECK(observed_button);
    highlighted_buttons_.insert(observed_button);
  } else {
    highlighted_buttons_.erase(observed_button);
  }

  UpdateHighlight();
}

void ToolbarIconContainerView::OnStateChanged(
    views::Button* observed_button,
    views::Button::ButtonState old_state) {
  UpdateHighlight();
}

void ToolbarIconContainerView::OnViewFocused(views::View* observed_view) {
  UpdateHighlight();
}

void ToolbarIconContainerView::OnViewBlurred(views::View* observed_view) {
  UpdateHighlight();
}

const views::View::Views& ToolbarIconContainerView::GetChildren() const {
  return children();
}

void ToolbarIconContainerView::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateHighlight();
}

void ToolbarIconContainerView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateHighlight();
}

gfx::Insets ToolbarIconContainerView::GetInsets() const {
  // Use empty insets to have the border paint into the view instead of around
  // it. This prevents inadvertently increasing its size while the stroke is
  // drawn.
  return gfx::Insets();
}

const char* ToolbarIconContainerView::GetClassName() const {
  return kToolbarIconContainerViewClassName;
}

bool ToolbarIconContainerView::ShouldDisplayHighlight() {
  if (!uses_highlight_)
    return false;

  if (IsMouseHovered() && (!main_button_ || !main_button_->IsMouseHovered()))
    return true;

  // Focused, pressed or hovered children should trigger the highlight.
  for (views::View* child : GetChildren()) {
    if (child == main_button_)
      continue;
    if (child->HasFocus())
      return true;
    views::Button* button = views::Button::AsButton(child);
    if (!button)
      continue;
    if (button->state() == views::Button::ButtonState::STATE_PRESSED ||
        button->state() == views::Button::ButtonState::STATE_HOVERED) {
      return true;
    }
    // The container should also be highlighted if a dialog is anchored to.
    if (base::Contains(highlighted_buttons_, button))
      return true;
  }

  return false;
}

void ToolbarIconContainerView::UpdateHighlight() {
  bool showing_before = highlight_animation_.IsShowing();

  if (ShouldDisplayHighlight()) {
    highlight_animation_.Show();
  } else {
    highlight_animation_.Hide();
  }

  if (showing_before == highlight_animation_.IsShowing())
    return;
  for (Observer& observer : observers_)
    observer.OnHighlightChanged();
}

void ToolbarIconContainerView::OverrideIconColor(SkColor color) {
  icon_color_ = color;
  UpdateAllIcons();
}

SkColor ToolbarIconContainerView::GetIconColor() const {
  if (icon_color_)
    return icon_color_.value();
  return GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
}

bool ToolbarIconContainerView::IsHighlighted() {
  return ShouldDisplayHighlight();
}

void ToolbarIconContainerView::SetHighlightBorder() {
  const float highlight_value = highlight_animation_.GetCurrentValue();
  if (highlight_value > 0.0f) {
    SkColor border_color = ToolbarButton::GetDefaultBorderColor(this);
    SetBorder(views::CreateRoundedRectBorder(
        1,
        ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
            views::EMPHASIS_MAXIMUM, size()),
        SkColorSetA(border_color,
                    SkColorGetA(border_color) * highlight_value)));
  } else {
    SetBorder(nullptr);
  }
}

void ToolbarIconContainerView::AnimationProgressed(
    const gfx::Animation* animation) {
  SetHighlightBorder();
}

void ToolbarIconContainerView::AnimationEnded(const gfx::Animation* animation) {
  SetHighlightBorder();
}
