// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/focus/browser_focus_controller_delegate_views.h"

#include "base/check_deref.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/user_education/common/help_bubble/help_bubble_factory_registry.h"
#include "components/user_education/views/help_bubble_view.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

BrowserFocusControllerDelegateViews::BrowserFocusControllerDelegateViews(
    Profile* profile,
    BrowserElements* browser_elements,
    ToolbarButtonProvider* toolbar_button_provider)
    : profile_(CHECK_DEREF(profile)),
      browser_elements_(CHECK_DEREF(browser_elements)),
      toolbar_button_provider_(CHECK_DEREF(toolbar_button_provider)) {}

BrowserFocusControllerDelegateViews::~BrowserFocusControllerDelegateViews() =
    default;

void BrowserFocusControllerDelegateViews::FocusWebContentsPane() {
  auto* multi_contents_view = views::AsViewClass<MultiContentsView>(
      GetViewForId(kMultiContentsViewElementId));
  if (multi_contents_view) {
    multi_contents_view->GetActiveContentsView()->RequestFocus();
  }
}

void BrowserFocusControllerDelegateViews::FocusInactivePopupForAccessibility() {
  if (ActivateFirstInactiveBubbleForAccessibility()) {
    return;
  }

  auto* infobar_container = views::AsViewClass<views::AccessiblePaneView>(
      GetViewForId(kInfoBarContainerElementId));
  if (infobar_container && !infobar_container->children().empty()) {
    infobar_container->SetPaneFocusAndFocusDefault();
  }
}

bool BrowserFocusControllerDelegateViews::
    ActivateFirstInactiveBubbleForAccessibility() {
  auto* const user_education =
      UserEducationServiceFactory::GetForBrowserContext(&*profile_);
  if (user_education &&
      user_education->help_bubble_factory_registry()
          .ToggleFocusForAccessibility(browser_elements_->GetContext())) {
    feature_engagement::TrackerFactory::GetForBrowserContext(&*profile_)
        ->NotifyEvent(
            feature_engagement::events::kFocusHelpBubbleAcceleratorPressed);
    return true;
  }

  // TODO: this fixes https://crbug.com/40668249 and https://crbug.com/40674460,
  // but a more general solution should be desirable to find any bubbles
  // anchored in the views hierarchy.
  views::DialogDelegate* bubble = nullptr;
  if (auto* control = toolbar_button_provider_->GetAppMenuControl()) {
    auto* dialog = control->GetDialogDelegate();
    if (dialog && !user_education::HelpBubbleView::IsHelpBubble(dialog)) {
      bubble = dialog;
    }
  }

  if (!bubble) {
    if (auto* avatar =
            toolbar_button_provider_->GetAvatarToolbarButtonInterface()) {
      auto* dialog = avatar->GetDialogDelegate();
      if (dialog && !user_education::HelpBubbleView::IsHelpBubble(dialog)) {
        bubble = dialog;
      }
    }
    for (auto* view : std::initializer_list<views::View*>{
             GetViewForId(kLocationBarElementId),
             toolbar_button_provider_->GetDownloadButton(),
             GetViewForId(kTopContainerElementId)}) {
      if (view) {
        if (auto* dialog = view->GetProperty(views::kAnchoredDialogKey);
            dialog && !user_education::HelpBubbleView::IsHelpBubble(dialog)) {
          bubble = dialog;
          break;
        }
      }
    }
  }

  if (bubble) {
    CHECK(!user_education::HelpBubbleView::IsHelpBubble(bubble));
    views::View* focusable = bubble->GetInitiallyFocusedView();

    if (!focusable) {
      focusable = bubble->GetCancelButton();
    }

    if (focusable) {
      focusable->RequestFocus();
#if BUILDFLAG(IS_MAC)
      views::Widget* const widget = bubble->GetWidget();
      if (widget && widget->IsVisible() && !widget->IsActive()) {
        widget->Activate();
      }
#endif
      return true;
    }
  }

  return false;
}

views::View* BrowserFocusControllerDelegateViews::GetViewForId(
    ui::ElementIdentifier element_id) {
  ui::TrackedElement* element = browser_elements_->GetElement(element_id);
  if (!element) {
    return nullptr;
  }

  views::TrackedElementViews* element_views =
      element->AsA<views::TrackedElementViews>();
  return element_views ? element_views->view() : nullptr;
}
