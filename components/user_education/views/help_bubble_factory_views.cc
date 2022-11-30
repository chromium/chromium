// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_factory_views.h"

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/views/help_bubble_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"

namespace user_education {

DEFINE_FRAMEWORK_SPECIFIC_METADATA(HelpBubbleViews)
DEFINE_FRAMEWORK_SPECIFIC_METADATA(HelpBubbleFactoryViews)

HelpBubbleViews::HelpBubbleViews(HelpBubbleView* help_bubble_view,
                                 ui::TrackedElement* anchor_element)
    : help_bubble_view_(help_bubble_view), anchor_element_(anchor_element) {
  DCHECK(help_bubble_view);
  DCHECK(help_bubble_view->GetWidget());
  scoped_observation_.Observe(help_bubble_view->GetWidget());

  // Set up an event listener so that the bubble can be closed if the anchor
  // element disappears. The specific anchor view is not tracked because in a
  // few cases (e.g. Mac native menus) the anchor view is not the anchor
  // element itself but a placeholder.
  anchor_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          anchor_element->identifier(), anchor_element->context(),
          base::BindRepeating(&HelpBubbleViews::OnElementHidden,
                              base::Unretained(this)));
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

  anchor_subscription_ = base::CallbackListSubscription();
  scoped_observation_.Reset();
  MaybeResetAnchorView();
  help_bubble_view_->GetWidget()->Close();
  help_bubble_view_ = nullptr;
}

void HelpBubbleViews::OnWidgetDestroying(views::Widget* widget) {
  anchor_subscription_ = base::CallbackListSubscription();
  scoped_observation_.Reset();
  MaybeResetAnchorView();
  help_bubble_view_ = nullptr;
  NotifyBubbleClosed();
}

void HelpBubbleViews::OnElementHidden(ui::TrackedElement* element) {
  // There could be other elements with the same identifier as the anchor
  // element, so don't close the bubble unless it is actually the anchor.
  if (element != anchor_element_)
    return;

  anchor_subscription_ = base::CallbackListSubscription();
  anchor_element_ = nullptr;
  Close();
}

HelpBubbleFactoryViews::HelpBubbleFactoryViews(
    const HelpBubbleDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

HelpBubbleFactoryViews::~HelpBubbleFactoryViews() = default;

std::unique_ptr<HelpBubble> HelpBubbleFactoryViews::CreateBubble(
    ui::TrackedElement* element,
    HelpBubbleParams params) {
  views::View* const anchor_view =
      element->AsA<views::TrackedElementViews>()->view();
  anchor_view->SetProperty(kHasInProductHelpPromoKey, true);
  auto result = base::WrapUnique(new HelpBubbleViews(
      new HelpBubbleView(delegate_, anchor_view, std::move(params)), element));
  for (const auto& accelerator :
       delegate_->GetPaneNavigationAccelerators(element)) {
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

}  // namespace user_education
