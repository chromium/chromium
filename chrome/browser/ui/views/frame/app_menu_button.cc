// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/app_menu_button.h"

#include <utility>

#include "base/feature_list.h"
#include "base/observer_list.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/app_menu_button_observer.h"
#include "chrome/browser/ui/views/toolbar/action_app_menu.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/view_class_properties.h"

AppMenuButton::AppMenuButton(PressedCallback callback)
    : ToolbarButton(PressedCallback()) {
  std::unique_ptr<views::MenuButtonController> menu_button_controller =
      std::make_unique<views::MenuButtonController>(
          this, std::move(callback),
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              this));
  menu_button_controller_ = menu_button_controller.get();
  SetButtonController(std::move(menu_button_controller));
  SetProperty(views::kInternalPaddingKey, gfx::Insets());
  SetProperty(views::kElementIdentifierKey, kToolbarAppMenuButtonElementId);

  if (menu_model()) {
    GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
  }
}

AppMenuButton::~AppMenuButton() = default;

views::BubbleAnchor AppMenuButton::GetAnchor() {
  return views::BubbleAnchor(this);
}

bool AppMenuButton::IsDrawn() const {
  return views::View::IsDrawn();
}

bool AppMenuButton::IsMenuShowing() const {
  if (base::FeatureList::IsEnabled(features::kAppMenuGlowUp)) {
    return action_menu_ && action_menu_->IsShowing();
  }
  return menu_ && menu_->IsShowing();
}

views::DialogDelegate* AppMenuButton::GetDialogDelegate() {
  return GetProperty(views::kAnchoredDialogKey);
}

void AppMenuButton::CloseMenu() {
  if (base::FeatureList::IsEnabled(features::kAppMenuGlowUp)) {
    if (action_menu_) {
      action_menu_->CloseMenu();
    }
    action_menu_.reset();
    return;
  }
  if (menu_) {
    menu_->CloseMenu();
  }
  menu_.reset();
}

void AppMenuButton::ShowMenu() {
  menu_button_controller_->Activate(nullptr);
}

void AppMenuButton::AddObserver(AppMenuButtonObserver* observer) {
  observer_list_.AddObserver(observer);
}

void AppMenuButton::RemoveObserver(AppMenuButtonObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void AppMenuButton::OnMenuClosed() {
  observer_list_.Notify(&AppMenuButtonObserver::AppMenuClosed);
}

void AppMenuButton::RunMenu(std::unique_ptr<AppMenuModel> menu_model,
                            Browser* browser,
                            int run_flags) {
  // |menu_| must be reset before |menu_model_| is destroyed, as per the comment
  // in the class declaration.
  menu_.reset();
  menu_model_ = std::move(menu_model);
  highlighter_.MaybeHighlight(browser, this, menu_model_.get());
  menu_model_->Init();

  menu_ = std::make_unique<AppMenu>(
      browser, menu_model_.get(), run_flags,
      base::BindRepeating(&AppMenuButton::OnMenuClosed,
                          weak_ptr_factory_.GetWeakPtr()));
  menu_->RunMenu(menu_button_controller_);

  observer_list_.Notify(&AppMenuButtonObserver::AppMenuShown);
}

void AppMenuButton::RunActionMenu(
    BrowserWindowInterface* browser_window_interface,
    int run_flags) {
  action_menu_.reset();
  action_menu_ = std::make_unique<ActionAppMenu>(
      browser_window_interface,
      base::BindRepeating(&AppMenuButton::OnMenuClosed,
                          weak_ptr_factory_.GetWeakPtr()));
  action_menu_->RunMenu(menu_button_controller_);

  observer_list_.Notify(&AppMenuButtonObserver::AppMenuShown);
}

void AppMenuButton::SetMenuTimerForTesting(base::ElapsedTimer timer) {
  menu_->SetTimerForTesting(std::move(timer));  // IN-TEST
}

bool AppMenuButton::HasFocus() const {
  return views::View::HasFocus();
}

void AppMenuButton::Focus(views::AccessiblePaneView* pane) {
  pane->SetPaneFocus(this);
}

void AppMenuButton::SetTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  // Empty default implementation. BrowserAppMenuButton overrides this to
  // handle update severity badges, while WebAppMenuButton does not need
  // this functionality.
}

void AppMenuButton::SetTrailingMargin(int margin) {
  ToolbarButton::SetTrailingMargin(margin);
}

views::View* AppMenuButton::GetFocusablePaneView() {
  return this;
}

BEGIN_METADATA(AppMenuButton)
END_METADATA
