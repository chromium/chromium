// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_views.h"

#include "base/functional/bind.h"
#include "components/user_education/common/help_bubble/custom_help_bubble.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/user_education/common/user_education_events.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/views/toggle_tracked_element_attention_utils.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/safe_castable.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/view_subregion_anchor.h"
#include "ui/views/view_tracker.h"

namespace user_education {

namespace {

bool IsFocusInHelpBubble(const views::BubbleDialogDelegateView* bubble) {
#if BUILDFLAG(IS_MAC)
  auto* const focused = bubble->GetFocusManager()->GetFocusedView();
  return focused && focused->GetWidget() == bubble->GetWidget();
#else
  return bubble->GetWidget()->IsActive();
#endif
}

}  // namespace

DEFINE_SAFE_CAST_TARGET(HelpBubbleViews)

HelpBubbleViews::HelpBubbleViews(HelpBubbleViewInfo info,
                                 ui::TrackedElement* anchor_element)
    : help_bubble_widget_(std::move(info.widget)),
      help_bubble_view_(info.bubble_view),
      anchor_element_(anchor_element) {
  CHECK(help_bubble_widget_);
  CHECK(help_bubble_view_);
  CHECK(anchor_element_);
  CHECK_EQ(help_bubble_widget_.get(), help_bubble_view_->GetWidget());

  // If the anchor is a view, ensure it gets the proper state.
  if (auto* const anchor = GetAnchorView()) {
    anchor->SetProperty(kHasInProductHelpPromoKey, true);
    MaybeApplyAttentionStateToTrackedElement(anchor);
  }

  help_bubble_widget_->MakeCloseSynchronous(base::BindOnce(
      &HelpBubbleViews::OnHelpBubbleClosing, base::Unretained(this)));

  anchor_hidden_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          anchor_element->identifier(), anchor_element->context(),
          base::BindRepeating(&HelpBubbleViews::OnElementHidden,
                              base::Unretained(this)));
  anchor_bounds_changed_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
          views::ViewSubregionAnchor::kAnchorBoundsChangedEvent,
          anchor_element->context(),
          base::BindRepeating(&HelpBubbleViews::OnElementBoundsChanged,
                              base::Unretained(this)));
}

HelpBubbleViews::~HelpBubbleViews() {
  // Needs to be called here while we still have access to class-specific logic.
  // Safe to call even if `Close()` has already been called; see the
  // implementation of `Close()`.
  Close(CloseReason::kBubbleDestroyed);
}

// static
views::BubbleBorder::Arrow HelpBubbleViews::TranslateArrow(
    HelpBubbleArrow arrow) {
  switch (arrow) {
    case HelpBubbleArrow::kNone:
      return views::BubbleBorder::NONE;
    case HelpBubbleArrow::kTopLeft:
      return views::BubbleBorder::TOP_LEFT;
    case HelpBubbleArrow::kTopRight:
      return views::BubbleBorder::TOP_RIGHT;
    case HelpBubbleArrow::kBottomLeft:
      return views::BubbleBorder::BOTTOM_LEFT;
    case HelpBubbleArrow::kBottomRight:
      return views::BubbleBorder::BOTTOM_RIGHT;
    case HelpBubbleArrow::kLeftTop:
      return views::BubbleBorder::LEFT_TOP;
    case HelpBubbleArrow::kRightTop:
      return views::BubbleBorder::RIGHT_TOP;
    case HelpBubbleArrow::kLeftBottom:
      return views::BubbleBorder::LEFT_BOTTOM;
    case HelpBubbleArrow::kRightBottom:
      return views::BubbleBorder::RIGHT_BOTTOM;
    case HelpBubbleArrow::kTopCenter:
      return views::BubbleBorder::TOP_CENTER;
    case HelpBubbleArrow::kBottomCenter:
      return views::BubbleBorder::BOTTOM_CENTER;
    case HelpBubbleArrow::kLeftCenter:
      return views::BubbleBorder::LEFT_CENTER;
    case HelpBubbleArrow::kRightCenter:
      return views::BubbleBorder::RIGHT_CENTER;
  }
}

bool HelpBubbleViews::ToggleFocusForAccessibility() {
  // // If the bubble isn't present or can't be meaningfully focused, stop.
  if (!help_bubble_view_) {
    return false;
  }

  // If the focus isn't in the help bubble, focus the help bubble.
  // Note that if is_focus_in_ancestor_widget is true, then anchor both exists
  // and has a widget, so anchor->GetWidget() will always be valid.
  if (!IsFocusInHelpBubble(help_bubble_view_)) {
    help_bubble_view_->GetWidget()->Activate();
    help_bubble_view_->RequestFocus();
    return true;
  }

  bool set_focus = false;

  if (auto* const anchor = GetAnchorView()) {
    if (anchor->GetViewAccessibility().IsAccessibilityFocusable()) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      // Mac and Linux do not automatically pass activation on focus, so we have
      // to do it manually.
      anchor->GetWidget()->Activate();
#endif
#if !BUILDFLAG(IS_MAC)
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
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      // Mac and Linux do not automatically pass activation on focus, so we have
      // to do it manually.
      anchor->GetWidget()->Activate();
#endif
#if !BUILDFLAG(IS_MAC)
      // You can't focus an accessible pane if it's already in accessibility
      // mode, so avoid doing that; the SetPaneFocus() call will go back into
      // accessibility navigation mode.
      anchor->GetFocusManager()->SetKeyboardAccessible(false);
#endif
      set_focus = static_cast<views::AccessiblePaneView*>(anchor)->SetPaneFocus(
          nullptr);
    }
  }

  return set_focus;
}

void HelpBubbleViews::OnAnchorBoundsChanged() {
  if (help_bubble_view_) {
    help_bubble_view_->OnAnchorBoundsChanged();
  }
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

void HelpBubbleViews::DestroyWidget() {
  if (!has_widget()) {
    return;
  }

  anchor_hidden_subscription_ = base::CallbackListSubscription();
  anchor_bounds_changed_subscription_ = base::CallbackListSubscription();

  // Capture the anchor. If it's still there after cleanup, its state will be
  // cleaned up later.
  views::ViewTracker anchor_view_tracker(GetAnchorView());

  help_bubble_view_ = nullptr;
  anchor_element_ = nullptr;
  help_bubble_widget_.reset();

  // Clean up anchor state after the bubble is gone. This prevents the anchor
  // trying to do something silly, like change its contents in a way that could
  // cause reentrancy into the bubble.
  if (auto* const anchor_view = anchor_view_tracker.view()) {
    anchor_view->SetProperty(kHasInProductHelpPromoKey, false);
    MaybeRemoveAttentionStateFromTrackedElement(anchor_view);
  }
}

bool HelpBubbleViews::Close(CloseReason reason) {
  // All of these are no-ops if called a second time.
  auto on_close = BeginClose(reason);
  DestroyWidget();
  return on_close.is_valid();
}

views::View* HelpBubbleViews::GetAnchorView() {
  return help_bubble_view_ ? help_bubble_view_->GetAnchorView() : nullptr;
}

void HelpBubbleViews::OnHelpBubbleClosing(
    views::Widget::ClosedReason closed_reason) {
  // At this point, everything is cleared and `this` may be safely deleted.
  // Call `Close()` [again]; this is a no-op if the help bubble is already
  // marked as closing.
  Close(CloseReason::kBubbleDestroyed);
}

void HelpBubbleViews::OnElementHidden(ui::TrackedElement* element) {
  // There could be other elements with the same identifier as the anchor
  // element, so don't close the bubble unless it is actually the anchor.
  if (element != anchor_element_) {
    return;
  }

  anchor_hidden_subscription_ = base::CallbackListSubscription();
  anchor_bounds_changed_subscription_ = base::CallbackListSubscription();
  anchor_element_ = nullptr;
  Close(CloseReason::kAnchorHidden);
}

void HelpBubbleViews::OnElementBoundsChanged(ui::TrackedElement* element) {
  if (help_bubble_view_ && element == anchor_element_) {
    // TODO(dfried): Support arbitrary anchor regions more generally in
    // BubbleDialogDelegateViews so that non-help bubble dialogs can be used
    // as help bubbles when attached to e.g. WebUI elements.
    if (HelpBubbleView::IsHelpBubble(help_bubble_view_)) {
      static_cast<HelpBubbleView*>(help_bubble_view_.get())
          ->SetForceAnchorRect(element->GetScreenBounds());
    }
    OnAnchorBoundsChanged();
  }
}

CustomHelpBubbleViews::CustomHelpBubbleViews(
    std::unique_ptr<views::Widget> widget,
    views::BubbleDialogDelegateView* bubble,
    CustomHelpBubbleUi& ui,
    ui::TrackedElement* anchor_element,
    std::optional<UserAction> accept_button_action,
    std::optional<UserAction> cancel_button_action)
    : HelpBubbleViews(HelpBubbleViewInfo(std::move(widget), bubble),
                      anchor_element),
      CustomHelpBubble(ui),
      accept_button_action_(accept_button_action),
      cancel_button_action_(cancel_button_action) {
  // Help bubbles should not close on deactivate.
  bubble->set_close_on_deactivate(false);

  // Help bubbles should always send "ESC Pressed" on escape key, not cancel.
  bubble->set_esc_should_cancel_dialog_override(false);
}

// Note that the `Close()` call in the base-class destructor will not trigger
// `OnHelpBubbleClosing()` for this class; however, since the close reason is
// "unspecified" there is no additional action to be taken in this class'
// override, so it's safe to just call it there.
CustomHelpBubbleViews::~CustomHelpBubbleViews() = default;

void CustomHelpBubbleViews::OnHelpBubbleClosing(
    views::Widget::ClosedReason reason) {
  // If called during teardown, ignore.
  if (!has_widget()) {
    return;
  }

  std::optional<UserAction> action;
  CloseReason actual_reason = CloseReason::kProgrammaticallyClosed;

  switch (reason) {
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      action = accept_button_action_;
      break;

    case views::Widget::ClosedReason::kCancelButtonClicked:
      action = cancel_button_action_;
      break;

    case views::Widget::ClosedReason::kCloseButtonClicked:
    case views::Widget::ClosedReason::kEscKeyPressed:
      action = UserAction::kCancel;
      break;

    case views::Widget::ClosedReason::kLostFocus:
    case views::Widget::ClosedReason::kUnspecified:
      actual_reason = CloseReason::kBubbleDestroyed;
      break;
  }

  auto on_close = BeginClose(actual_reason);

  // The following call may result in `this` being destroyed.
  auto weak_this = GetWeakPtr();
  if (auto* const ui = custom_bubble_ui(); ui && action) {
    ui->NotifyUserAction(*action);
  }

  // If `this` survived the call, destroy the widget.
  if (weak_this) {
    DestroyWidget();
  }
}

}  // namespace user_education
