// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_app_menu_control.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/frame/app_menu_button_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {

toolbar_ui_api::mojom::AppMenuIconType ToMojomIconType(
    AppMenuIconController::IconType type) {
  switch (type) {
    case AppMenuIconController::IconType::kNone:
      return toolbar_ui_api::mojom::AppMenuIconType::kNone;
    case AppMenuIconController::IconType::kUpgradeNotification:
      return toolbar_ui_api::mojom::AppMenuIconType::kUpgradeNotification;
    case AppMenuIconController::IconType::kGlobalError:
      return toolbar_ui_api::mojom::AppMenuIconType::kGlobalError;
  }
}

toolbar_ui_api::mojom::AppMenuSeverity ToMojomSeverity(
    AppMenuIconController::Severity severity) {
  switch (severity) {
    case AppMenuIconController::Severity::kNone:
      return toolbar_ui_api::mojom::AppMenuSeverity::kNone;
    case AppMenuIconController::Severity::kLow:
      return toolbar_ui_api::mojom::AppMenuSeverity::kLow;
    case AppMenuIconController::Severity::kMedium:
      return toolbar_ui_api::mojom::AppMenuSeverity::kMedium;
    case AppMenuIconController::Severity::kHigh:
      return toolbar_ui_api::mojom::AppMenuSeverity::kHigh;
  }
}

}  // namespace

WebUIAppMenuControl::WebUIAppMenuControl(WebUIToolbarControlDelegate& delegate)
    : delegate_(delegate) {}

WebUIAppMenuControl::~WebUIAppMenuControl() = default;

toolbar_ui_api::mojom::AppMenuControlStatePtr WebUIAppMenuControl::GetState()
    const {
  auto state = toolbar_ui_api::mojom::AppMenuControlState::New();
  state->icon_type = ToMojomIconType(type_and_severity_.type);
  state->severity = ToMojomSeverity(type_and_severity_.severity);
  state->is_context_menu_visible = IsMenuShowing();
  state->window_is_maximized_or_fullscreen = window_is_maximized_or_fullscreen_;

  const std::u16string descriptive_name =
      AppMenuIconController::GetIconAccessibleName(type_and_severity_.type);

  if (type_and_severity_.severity != AppMenuIconController::Severity::kNone) {
    const std::u16string label = AppMenuIconController::GetIconLabel(
        type_and_severity_.type, type_and_severity_.severity);
    state->label_text = label;
    state->accessibility_text = label.empty() ? descriptive_name : label;
  } else {
    state->accessibility_text = descriptive_name;
  }

  state->tooltip = AppMenuIconController::GetIconTooltip(
      type_and_severity_.type, type_and_severity_.severity);

  return state;
}

views::BubbleAnchor WebUIAppMenuControl::GetAnchor() {
  if (ui::TrackedElement* element =
          ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
              kToolbarAppMenuButtonElementId,
              views::ElementTrackerViews::GetContextForView(
                  delegate_->GetView()))) {
    return views::BubbleAnchor(element);
  }
  // Fallback to the toolbar if the app menu button element is not found.
  // This results in anchoring in the same place as if it was anchored to the
  // app menu button since the WebUI control is part of the toolbar.
  // This also ensures we never return a null anchor.
  return views::BubbleAnchor(delegate_->GetView());
}

bool WebUIAppMenuControl::IsDrawn() const {
  return delegate_->GetView()->IsDrawn();
}

bool WebUIAppMenuControl::IsMenuShowing() const {
  return menu_ && menu_->IsShowing();
}

views::DialogDelegate* WebUIAppMenuControl::GetDialogDelegate() {
  // TODO(crbug.com/510825650): Implement dialog delegate from WebUI if needed.
  return nullptr;
}

void WebUIAppMenuControl::CloseMenu() {
  if (menu_) {
    menu_->CloseMenu();
  }
}

void WebUIAppMenuControl::ShowMenu() {
  HandleContextMenu(GetAnchor().GetAnchorRect(),
                    ui::mojom::MenuSourceType::kKeyboard);
}

void WebUIAppMenuControl::AddObserver(AppMenuButtonObserver* observer) {
  observer_list_.AddObserver(observer);
}

void WebUIAppMenuControl::RemoveObserver(AppMenuButtonObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void WebUIAppMenuControl::Focus(views::AccessiblePaneView* pane) {
  pane->SetPaneFocus(GetFocusablePaneView());
  delegate_->OnFocusRequested(
      toolbar_ui_api::mojom::FocusRequestTarget::kAppMenu);
}

bool WebUIAppMenuControl::HasFocus() const {
  return focused_;
}

void WebUIAppMenuControl::SetFocused(bool focused) {
  focused_ = focused;
}

void WebUIAppMenuControl::HandleContextMenu(const gfx::Rect& anchor_bounds,
                                            ui::mojom::MenuSourceType source) {
  if (IsMenuShowing()) {
    CloseMenu();
    return;
  }

  BrowserWindowInterface* browser_window = delegate_->GetBrowser();
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_window);
  CHECK(browser_view);
  ToolbarView* toolbar_view = browser_view->toolbar();
  CHECK(toolbar_view);
  Browser* browser = browser_window->GetBrowserForMigrationOnly();

  // Explicitly destroy the old menu UI before recreating the model. `AppMenu`
  // holds a `raw_ptr` to `AppMenuModel`. If we don't reset `menu_` first,
  // recreating `menu_model_` would temporarily leave `menu_` with a dangling
  // pointer to the destroyed model.
  menu_.reset();
  menu_model_ = std::make_unique<AppMenuModel>(
      toolbar_view, browser, toolbar_view->app_menu_icon_controller(),
      AppMenuModel::GetAlertItemForRunningTutorial(browser));
  menu_model_->Init();

  int run_flags = views::MenuRunner::HAS_MNEMONICS;
  if (source == ui::mojom::MenuSourceType::kKeyboard) {
    run_flags |= views::MenuRunner::SHOULD_SHOW_MNEMONICS |
                 views::MenuRunner::INVOKED_FROM_KEYBOARD;
  }

  menu_ = std::make_unique<AppMenu>(
      browser, menu_model_.get(), run_flags,
      base::BindRepeating(&WebUIAppMenuControl::UpdateOpenState,
                          weak_ptr_factory_.GetWeakPtr()));

  menu_->RunMenu(delegate_->GetView()->GetWidget(), anchor_bounds, source);
  UpdateOpenState();
}

void WebUIAppMenuControl::SetTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  if (type_and_severity_ == type_and_severity) {
    return;
  }

  type_and_severity_ = type_and_severity;
  delegate_->OnAppMenuControlStateChanged(GetState());
  delegate_->OnPreferredSizeChanged();
}

void WebUIAppMenuControl::SetIsMaximizedOrFullscreen(
    bool maximized_or_fullscreen) {
  if (window_is_maximized_or_fullscreen_ == maximized_or_fullscreen) {
    return;
  }
  window_is_maximized_or_fullscreen_ = maximized_or_fullscreen;
  delegate_->OnAppMenuControlStateChanged(GetState());
}

views::View* WebUIAppMenuControl::GetFocusablePaneView() {
  // Returns the parent WebUIToolbarWebView (the entire toolbar) rather than
  // the individual button, because the button itself is rendered inside the
  // WebUI and is not a Views View.
  return delegate_->GetView();
}

void WebUIAppMenuControl::UpdateOpenState() {
  delegate_->OnAppMenuControlStateChanged(GetState());

  const bool is_open = IsMenuShowing();
  for (auto& observer : observer_list_) {
    if (is_open) {
      observer.AppMenuShown();
    } else {
      observer.AppMenuClosed();
    }
  }
}
