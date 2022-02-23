// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/help_bubble_factory_views.h"

#include <memory>

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
#include "ui/views/interaction/element_tracker_views.h"

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
  if (is_focus_in_ancestor_widget) {
    help_bubble_view_->GetWidget()->Activate();
    help_bubble_view_->RequestFocus();
    return true;
  }

  // If the anchor isn't accessibility-focusable, we can't toggle focus.
  if (!anchor || !anchor->IsAccessibilityFocusable())
    return false;

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
