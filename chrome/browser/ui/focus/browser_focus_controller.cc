// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/focus/browser_focus_controller.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
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
#include "ui/base/base_window.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

DEFINE_USER_DATA(BrowserFocusController);

namespace {

views::View* GetViewForId(BrowserWindowInterface* browser,
                          ui::ElementIdentifier element_id) {
  auto* browser_elements = BrowserElements::From(browser);
  if (!browser_elements) {
    return nullptr;
  }

  ui::TrackedElement* element = browser_elements->GetElement(element_id);
  if (!element) {
    return nullptr;
  }

  views::TrackedElementViews* element_views =
      element->AsA<views::TrackedElementViews>();
  return element_views ? element_views->view() : nullptr;
}

}  // namespace

BrowserFocusController::BrowserFocusController(BrowserWindowInterface& browser)
    : browser_(browser),
      scoped_data_holder_(browser.GetUnownedUserDataHost(), *this) {}

BrowserFocusController::~BrowserFocusController() = default;

// static
BrowserFocusController* BrowserFocusController::From(
    BrowserWindowInterface* browser) {
  CHECK(browser);
  return ui::ScopedUnownedUserData<BrowserFocusController>::Get(
      browser->GetUnownedUserDataHost());
}

// static
const BrowserFocusController* BrowserFocusController::From(
    const BrowserWindowInterface* browser) {
  CHECK(browser);
  return ui::ScopedUnownedUserData<BrowserFocusController>::Get(
      browser->GetUnownedUserDataHost());
}

void BrowserFocusController::RotatePaneFocus(bool forwards) {
  gfx::NativeWindow native_window = browser_->GetWindow()->GetNativeWindow();
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(native_window);
  if (!widget) {
    return;
  }
  widget->GetFocusManager()->RotatePaneFocus(
      forwards ? views::FocusManager::Direction::kForward
               : views::FocusManager::Direction::kBackward,
      views::FocusManager::FocusCycleWrapping::kEnabled);
}

void BrowserFocusController::FocusWebContentsPane() {
  auto* multi_contents_view = views::AsViewClass<MultiContentsView>(
      GetViewForId(&*browser_, kMultiContentsViewElementId));
  if (multi_contents_view) {
    multi_contents_view->GetActiveContentsView()->RequestFocus();
  }
}

void BrowserFocusController::FocusInactivePopupForAccessibility() {
  if (ActivateFirstInactiveBubbleForAccessibility()) {
    return;
  }

  auto* infobar_container = views::AsViewClass<views::AccessiblePaneView>(
      GetViewForId(&*browser_, kInfoBarContainerElementId));
  if (infobar_container && !infobar_container->children().empty()) {
    infobar_container->SetPaneFocusAndFocusDefault();
  }
}

bool BrowserFocusController::ActivateFirstInactiveBubbleForAccessibility() {
  Profile* profile = browser_->GetProfile();
  auto* const user_education =
      UserEducationServiceFactory::GetForBrowserContext(profile);
  if (user_education &&
      user_education->help_bubble_factory_registry()
          .ToggleFocusForAccessibility(
              BrowserElements::From(&*browser_)->GetContext())) {
    feature_engagement::TrackerFactory::GetForBrowserContext(profile)
        ->NotifyEvent(
            feature_engagement::events::kFocusHelpBubbleAcceleratorPressed);
    return true;
  }

  auto* toolbar_button_provider = ToolbarButtonProvider::From(&*browser_);
  if (!toolbar_button_provider) {
    return false;
  }

  // TODO: this fixes https://crbug.com/40668249 and https://crbug.com/40674460,
  // but a more general solution should be desirable to find any bubbles
  // anchored in the views hierarchy.
  views::DialogDelegate* bubble = nullptr;
  if (auto* control = toolbar_button_provider->GetAppMenuControl()) {
    auto* dialog = control->GetDialogDelegate();
    if (dialog && !user_education::HelpBubbleView::IsHelpBubble(dialog)) {
      bubble = dialog;
    }
  }

  if (!bubble) {
    if (auto* avatar =
            toolbar_button_provider->GetAvatarToolbarButtonInterface()) {
      auto* dialog = avatar->GetDialogDelegate();
      if (dialog && !user_education::HelpBubbleView::IsHelpBubble(dialog)) {
        bubble = dialog;
      }
    }
    for (auto* view : std::initializer_list<views::View*>{
             GetViewForId(&*browser_, kLocationBarElementId),
             toolbar_button_provider->GetDownloadButton(),
             GetViewForId(&*browser_, kTopContainerElementId)}) {
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
        DCHECK(browser_->IsActive());
        widget->Activate();
      }
#endif
      return true;
    }
  }

  return false;
}
