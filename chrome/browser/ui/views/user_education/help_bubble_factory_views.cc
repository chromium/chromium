// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/help_bubble_factory_views.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/user_education/help_bubble_params.h"
#include "chrome/browser/ui/views/chrome_view_class_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/help_bubble_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"

namespace {

// Returns whether losing focus would cause a widget to be destroyed.
// This prevents us from accidentally closing a widget a bubble is anchored to
// at the cost of not being able to directly access the help bubble.
bool BlurWouldCloseWidget(const views::Widget* widget) {
  // Right now, we can only ask the question if we know the bubble is
  // controlled by a BubbleDialogDelegateView, since runtime type information
  // isn't present for any of the other objects involved.
  auto* const contents = widget->widget_delegate()->GetContentsView();
  return contents &&
         views::IsViewClass<views::BubbleDialogDelegateView>(contents) &&
         static_cast<const views::BubbleDialogDelegateView*>(contents)
             ->close_on_deactivate();
}

}  // namespace

DEFINE_FRAMEWORK_SPECIFIC_METADATA(HelpBubbleViews)
DEFINE_FRAMEWORK_SPECIFIC_METADATA(HelpBubbleFactoryViews)

HelpBubbleViews::HelpBubbleViews(HelpBubbleView* help_bubble_view)
    : help_bubble_view_(help_bubble_view) {
  DCHECK(help_bubble_view);
  DCHECK(help_bubble_view->GetWidget());
  scoped_observation_.Observe(help_bubble_view->GetWidget());
}

HelpBubbleViews::~HelpBubbleViews() {
  // Needs to be called here while we still have access to HelpBubbleViews-
  // specific logic.
  Close();
}

bool HelpBubbleViews::ToggleFocusForAccessibility() {
  // // If the bubble isn't present or can't be meaningfully focused, stop.
  if (!help_bubble_view_)
    return false;

  // Focus can't be determined just by widget activity; we must check to see if
  // there's a focused view in the anchor widget or the top-level browser
  // widget (if different).
  auto* const anchor = help_bubble_view_->GetAnchorView();
  const bool is_focus_in_ancestor_widget =
      (anchor && anchor->GetFocusManager()->GetFocusedView()) ||
      help_bubble_view_->GetWidget()
          ->GetPrimaryWindowWidget()
          ->GetFocusManager()
          ->GetFocusedView();

  // If the focus isn't in the help bubble, focus the help bubble.
  // Note that if is_focus_in_ancestor_widget is true, then anchor both exists
  // and has a widget, so anchor->GetWidget() will always be valid.
  if (is_focus_in_ancestor_widget &&
      !BlurWouldCloseWidget(anchor->GetWidget())) {
    help_bubble_view_->GetWidget()->Activate();
    help_bubble_view_->RequestFocus();
    return true;
  }

  if (!anchor)
    return false;

  // An AccessiblePaneView can receive focus, but is not necessarily itself
  // accessibility focusable. Use the built-in functionality for focusing
  // elements of AccessiblePaneView instead.
  if (!anchor->IsAccessibilityFocusable()) {
    if (views::IsViewClass<views::AccessiblePaneView>(anchor)) {
      // You can't focus an accessible pane if it's already in accessibility
      // mode, so avoid doing that; the SetPaneFocus() call will go back into
      // accessibility navigation mode.
      anchor->GetFocusManager()->SetKeyboardAccessible(false);
      return static_cast<views::AccessiblePaneView*>(anchor)->SetPaneFocus(
          nullptr);
    } else {
      return false;
    }
  }

  // Focus the anchor. We can't request focus for an accessibility-only view
  // until we turn on keyboard accessibility for its focus manager.
  anchor->GetFocusManager()->SetKeyboardAccessible(true);
  anchor->RequestFocus();
  return true;
}

void HelpBubbleViews::OnAnchorBoundsChanged() {
  if (help_bubble_view_)
    help_bubble_view_->OnAnchorBoundsChanged();
}

gfx::Rect HelpBubbleViews::GetBoundsInScreen() const {
  return help_bubble_view_
             ? help_bubble_view_->GetWidget()->GetWindowBoundsInScreen()
             : gfx::Rect();
}

ui::ElementContext HelpBubbleViews::GetContext() const {
  return help_bubble_view_
             ? views::ElementTrackerViews::GetContextForView(help_bubble_view_)
             : ui::ElementContext();
}

void HelpBubbleViews::MaybeResetAnchorView() {
  if (!help_bubble_view_)
    return;
  auto* const anchor_view = help_bubble_view_->GetAnchorView();
  if (!anchor_view)
    return;
  anchor_view->SetProperty(kHasInProductHelpPromoKey, false);
}

void HelpBubbleViews::CloseBubbleImpl() {
  if (!help_bubble_view_)
    return;

  scoped_observation_.Reset();
  MaybeResetAnchorView();
  help_bubble_view_->GetWidget()->Close();
  help_bubble_view_ = nullptr;
}

void HelpBubbleViews::OnWidgetClosing(views::Widget* widget) {
  scoped_observation_.Reset();
  MaybeResetAnchorView();
  help_bubble_view_ = nullptr;
  NotifyBubbleClosed();
}

void HelpBubbleViews::OnWidgetDestroying(views::Widget* widget) {
  OnWidgetClosing(widget);
}

HelpBubbleFactoryViews::HelpBubbleFactoryViews() = default;
HelpBubbleFactoryViews::~HelpBubbleFactoryViews() = default;

std::unique_ptr<HelpBubble> HelpBubbleFactoryViews::CreateBubble(
    ui::TrackedElement* element,
    HelpBubbleParams params) {
  views::View* const anchor_view =
      element->AsA<views::TrackedElementViews>()->view();
  anchor_view->SetProperty(kHasInProductHelpPromoKey, true);
  return base::WrapUnique(
      new HelpBubbleViews(new HelpBubbleView(anchor_view, std::move(params))));
}

bool HelpBubbleFactoryViews::CanBuildBubbleForTrackedElement(
    const ui::TrackedElement* element) const {
  return element->IsA<views::TrackedElementViews>();
}
