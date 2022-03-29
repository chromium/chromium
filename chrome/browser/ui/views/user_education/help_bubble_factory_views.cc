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

  // If the focus isn't in the help bubble, focus the help bubble.
  // Note that if is_focus_in_ancestor_widget is true, then anchor both exists
  // and has a widget, so anchor->GetWidget() will always be valid.
  if (!help_bubble_view_->IsFocusInHelpBubble()) {
    help_bubble_view_->GetWidget()->Activate();
    help_bubble_view_->RequestFocus();
    return true;
  }

  auto* const anchor = help_bubble_view_->GetAnchorView();
  if (!anchor)
    return false;

  bool set_focus = false;
  if (anchor->IsAccessibilityFocusable()) {
#if BUILDFLAG(IS_MAC)
    // Mac does not automatically pass activation on focus, so we have to do it
    // manually.
    anchor->GetWidget()->Activate();
#else
    // Focus the anchor. We can't request focus for an accessibility-only view
    // until we turn on keyboard accessibility for its focus manager.
    anchor->GetFocusManager()->SetKeyboardAccessible(true);
#endif
    anchor->RequestFocus();
    set_focus = true;
  } else if (views::IsViewClass<views::AccessiblePaneView>(anchor)) {
    // An AccessiblePaneView can receive focus, but is not necessarily itself
    // accessibility focusable. Use the built-in functionality for focusing
    // elements of AccessiblePaneView instead.
#if BUILDFLAG(IS_MAC)
    // Mac does not automatically pass activation on focus, so we have to do it
    // manually.
    anchor->GetWidget()->Activate();
#else
    // You can't focus an accessible pane if it's already in accessibility
    // mode, so avoid doing that; the SetPaneFocus() call will go back into
    // accessibility navigation mode.
    anchor->GetFocusManager()->SetKeyboardAccessible(false);
#endif
    set_focus =
        static_cast<views::AccessiblePaneView*>(anchor)->SetPaneFocus(nullptr);
  }

  return set_focus;
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

bool HelpBubbleViews::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (CanHandleAccelerators()) {
    ToggleFocusForAccessibility();
    return true;
  }

  return false;
}

bool HelpBubbleViews::CanHandleAccelerators() const {
  return help_bubble_view_ && help_bubble_view_->GetWidget() &&
         help_bubble_view_->GetWidget()->IsActive();
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

HelpBubbleFactoryViews::HelpBubbleFactoryViews(
    HelpBubbleAcceleratorDelegate* accelerator_delegate)
    : accelerator_delegate_(accelerator_delegate) {
  DCHECK(accelerator_delegate_);
}

HelpBubbleFactoryViews::~HelpBubbleFactoryViews() = default;

std::unique_ptr<HelpBubble> HelpBubbleFactoryViews::CreateBubble(
    ui::TrackedElement* element,
    HelpBubbleParams params) {
  views::View* const anchor_view =
      element->AsA<views::TrackedElementViews>()->view();
  anchor_view->SetProperty(kHasInProductHelpPromoKey, true);
  auto result = base::WrapUnique(
      new HelpBubbleViews(new HelpBubbleView(anchor_view, std::move(params))));
  for (const auto& accelerator :
       accelerator_delegate_->GetPaneNavigationAccelerators(element)) {
    result->bubble_view()->GetFocusManager()->RegisterAccelerator(
        accelerator, ui::AcceleratorManager::HandlerPriority::kNormalPriority,
        result.get());
  }
  return result;
}

bool HelpBubbleFactoryViews::CanBuildBubbleForTrackedElement(
    const ui::TrackedElement* element) const {
  return element->IsA<views::TrackedElementViews>();
}
