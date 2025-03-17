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
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/interaction/element_tracker_views.h"

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

DEFINE_FRAMEWORK_SPECIFIC_METADATA(HelpBubbleViews)

HelpBubbleViews::HelpBubbleViews(
    views::BubbleDialogDelegateView* help_bubble_view,
    ui::TrackedElement* anchor_element)
    : help_bubble_view_(help_bubble_view), anchor_element_(anchor_element) {
  CHECK(help_bubble_view);
  CHECK(help_bubble_view->GetWidget());
  CHECK(anchor_element);
  scoped_observation_.Observe(help_bubble_view->GetWidget());

  anchor_hidden_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          anchor_element->identifier(), anchor_element->context(),
          base::BindRepeating(&HelpBubbleViews::OnElementHidden,
                              base::Unretained(this)));
  anchor_bounds_changed_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
          kHelpBubbleAnchorBoundsChangedEvent, anchor_element->context(),
          base::BindRepeating(&HelpBubbleViews::OnElementBoundsChanged,
                              base::Unretained(this)));
}

HelpBubbleViews::~HelpBubbleViews() {
  // Needs to be called here while we still have access to HelpBubbleViews-
  // specific logic.
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

  auto* const anchor = help_bubble_view_->GetAnchorView();
  if (!anchor) {
    return false;
  }

  bool set_focus = false;
  if (anchor->GetViewAccessibility().IsAccessibilityFocusable()) {
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

void HelpBubbleViews::MaybeResetAnchorView() {
  if (!help_bubble_view_) {
    return;
  }
  auto* const anchor_view = help_bubble_view_->GetAnchorView();
  if (!anchor_view) {
    return;
  }
  anchor_view->SetProperty(kHasInProductHelpPromoKey, false);
  MaybeRemoveAttentionStateFromTrackedElement(anchor_view);
}

void HelpBubbleViews::CloseBubbleImpl() {
  anchor_hidden_subscription_ = base::CallbackListSubscription();
  anchor_bounds_changed_subscription_ = base::CallbackListSubscription();
  scoped_observation_.Reset();
  MaybeResetAnchorView();

  // Reset the anchor view. Closing the widget could cause callbacks which could
  // theoretically destroy `this`, so
  auto* const help_bubble_view = help_bubble_view_.get();
  help_bubble_view_ = nullptr;
  anchor_element_ = nullptr;
  if (help_bubble_view && help_bubble_view->GetWidget()) {
    help_bubble_view->GetWidget()->Close();
  }
}

void HelpBubbleViews::OnWidgetDestroying(views::Widget* widget) {
  Close(CloseReason::kBubbleElementDestroyed);
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
    : HelpBubbleViews(bubble, anchor_element),
      CustomHelpBubble(ui),
      help_bubble_widget_(std::move(widget)),
      accept_button_action_(accept_button_action),
      cancel_button_action_(cancel_button_action) {
  CHECK(help_bubble_widget_);

  // Help bubbles should not close on deactivate.
  bubble->set_close_on_deactivate(false);

  // Help bubbles should always send "ESC Pressed" on escape key, not cancel.
  bubble->set_esc_should_cancel_dialog_override(false);

  bubble->GetWidget()->MakeCloseSynchronous(base::BindOnce(
      &CustomHelpBubbleViews::OnHelpBubbleClosing, base::Unretained(this)));
}

CustomHelpBubbleViews::~CustomHelpBubbleViews() {
  // Ensure that all closing of help bubbles goes through the same logic path.
  //
  // Due to upstream logic in HelpBubbleViews, `OnHelpBubbleClosing()` ends up
  // getting called in a state where the widget cannot correctly be destroyed,
  // leading to a CHECK().
  //
  // This will be unnecessary when HelpBubbleViews is migrated to ownership mode
  // CLIENT_OWNS_WIDGET.
  if (help_bubble_widget_) {
    help_bubble_widget_->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
  }
}

void CustomHelpBubbleViews::OnHelpBubbleClosing(
    views::Widget::ClosedReason reason) {
  // The calls below could also destroy `this`, so save off widget in a local.
  // This both guarantees that the widget will get properly destroyed at the end
  // of this method (as is required by `MakeCloseSynchronous()`) and also
  // prevents re-entrancy in the destructor as `help_bubble_widget_` will be
  // null.
  std::unique_ptr<views::Widget> widget = std::move(help_bubble_widget_);

  if (auto* const ui = custom_bubble_ui()) {
    switch (reason) {
      case views::Widget::ClosedReason::kAcceptButtonClicked:
        if (accept_button_action_) {
          ui->NotifyUserAction(*accept_button_action_);
        }
        break;

      case views::Widget::ClosedReason::kCancelButtonClicked:
        if (cancel_button_action_) {
          ui->NotifyUserAction(*cancel_button_action_);
        }
        break;

      case views::Widget::ClosedReason::kCloseButtonClicked:
      case views::Widget::ClosedReason::kEscKeyPressed:
        ui->NotifyUserAction(UserAction::kCancel);
        break;

      case views::Widget::ClosedReason::kLostFocus:
      case views::Widget::ClosedReason::kUnspecified:
        // Do nothing.
        break;
    }
  }

  // This is required when responding to `OnHelpBubbleClosing()`; the widget
  // must be destroyed before this method returns.
  widget.reset();
}

}  // namespace user_education
