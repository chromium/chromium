// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/scoped_observation.h"
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
#include "ui/views/view_observer.h"

// static
const char ToolbarIconContainerView::kToolbarIconContainerViewClassName[] =
    "ToolbarIconContainerView";

// Watches for widget restore (or first show) and resets the animation so icons
// don't spuriously "animate in" when a window is shown or restored. See
// crbug.com/1106506 for more details.
//
// There is currently no signal that is consistent across platforms and
// accessible from the View hierarchy that can tell us if, e.g., a window has
// been restored from a minimized state. While we could theoretically plumb
// state changes through from NativeWidget, we can observe the specific set of
// cases we want by observing the size of the window.
//
// We *cannot* observe the size of the widget itself, as at least on Windows,
// minimizing a window does not set the widget to 0x0, but rather a small,
// Windows 3.1-esque tile (~160x28) and moves it to [-32000, -32000], so far off
// the screen it can't appear on any monitor.
//
// What we can observe is the widget's root view, which is (a) always present
// after the toolbar has been added to its widget and through its entire
// lifetime, and (b) is actually set to zero size when the window is zero size
// or minimized on Windows.
class ToolbarIconContainerView::WidgetRestoreObserver
    : public views::ViewObserver {
 public:
  explicit WidgetRestoreObserver(
      ToolbarIconContainerView* toolbar_icon_container_view)
      : toolbar_icon_container_view_(toolbar_icon_container_view) {
    scoped_observation_.Observe(
        toolbar_icon_container_view->GetWidget()->GetRootView());
  }

  void OnViewBoundsChanged(views::View* observed_view) override {
    const bool is_collapsed = observed_view->bounds().IsEmpty();
    if (is_collapsed != was_collapsed_) {
      was_collapsed_ = is_collapsed;
      if (!is_collapsed)
        toolbar_icon_container_view_->animating_layout_manager()->ResetLayout();
    }
  }

 private:
  bool was_collapsed_ = true;
  ToolbarIconContainerView* const toolbar_icon_container_view_;
  base::ScopedObservation<views::View, views::ViewObserver> scoped_observation_{
      this};
};

ToolbarIconContainerView::ToolbarIconContainerView(bool uses_highlight)
    : uses_highlight_(uses_highlight) {
  views::AnimatingLayoutManager* animating_layout =
      SetLayoutManager(std::make_unique<views::AnimatingLayoutManager>());
  animating_layout->SetBoundsAnimationMode(
      views::AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  animating_layout->SetDefaultFadeMode(
      views::AnimatingLayoutManager::FadeInOutMode::kSlideFromTrailingEdge);
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
  main_button_ = main_button;
  ObserveButton(main_button_);
  AddChildView(main_button_);
}

void ToolbarIconContainerView::ObserveButton(views::Button* button) {
  // We don't care about the main button being highlighted.
  if (button != main_button_) {
    subscriptions_.push_back(
        button->AddHighlightedChangedCallback(base::BindRepeating(
            &ToolbarIconContainerView::OnButtonHighlightedChanged,
            base::Unretained(this), base::Unretained(button))));
  }
  subscriptions_.push_back(button->AddStateChangedCallback(base::BindRepeating(
      &ToolbarIconContainerView::UpdateHighlight, base::Unretained(this))));
  button->AddObserver(this);
}

void ToolbarIconContainerView::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void ToolbarIconContainerView::RemoveObserver(const Observer* obs) {
  observers_.RemoveObserver(obs);
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

void ToolbarIconContainerView::OnViewFocused(views::View* observed_view) {
  UpdateHighlight();
}

void ToolbarIconContainerView::OnViewBlurred(views::View* observed_view) {
  UpdateHighlight();
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

void ToolbarIconContainerView::AddedToWidget() {
  // Add an observer to reset the animation if the browser window is restored,
  // preventing spurious animation. (See crbug.com/1106506)
  restore_observer_ = std::make_unique<WidgetRestoreObserver>(this);
}

void ToolbarIconContainerView::AnimationProgressed(
    const gfx::Animation* animation) {
  SetHighlightBorder();
}

void ToolbarIconContainerView::AnimationEnded(const gfx::Animation* animation) {
  SetHighlightBorder();
}

bool ToolbarIconContainerView::ShouldDisplayHighlight() {
  if (!uses_highlight_)
    return false;

  if (IsMouseHovered() && (!main_button_ || !main_button_->IsMouseHovered()))
    return true;

  // Focused, pressed or hovered children should trigger the highlight.
  for (views::View* child : children()) {
    if (child == main_button_)
      continue;
    if (child->HasFocus())
      return true;
    views::Button* button = views::Button::AsButton(child);
    if (!button)
      continue;
    if (button->GetState() == views::Button::ButtonState::STATE_PRESSED ||
        button->GetState() == views::Button::ButtonState::STATE_HOVERED) {
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

void ToolbarIconContainerView::OnButtonHighlightedChanged(
    views::Button* button) {
  if (button->GetHighlighted())
    highlighted_buttons_.insert(button);
  else
    highlighted_buttons_.erase(button);

  UpdateHighlight();
}
