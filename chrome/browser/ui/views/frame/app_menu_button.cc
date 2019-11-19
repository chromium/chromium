// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/app_menu_button.h"

#include <utility>

#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/app_menu_button_observer.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/view_class_properties.h"

AppMenuButton::AppMenuButton(views::ButtonListener* button_listener)
    : ToolbarButton(nullptr) {
  std::unique_ptr<views::MenuButtonController> menu_button_controller =
      std::make_unique<views::MenuButtonController>(
          this, button_listener,
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              this));
  menu_button_controller_ = menu_button_controller.get();
  SetButtonController(std::move(menu_button_controller));
  SetProperty(views::kInternalPaddingKey, gfx::Insets());
}

AppMenuButton::~AppMenuButton() {}

void AppMenuButton::AddObserver(AppMenuButtonObserver* observer) {
  observer_list_.AddObserver(observer);
}

void AppMenuButton::RemoveObserver(AppMenuButtonObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void AppMenuButton::CloseMenu() {
  if (menu_)
    menu_->CloseMenu();
  menu_.reset();
}

void AppMenuButton::OnMenuClosed() {
  for (AppMenuButtonObserver& observer : observer_list_)
    observer.AppMenuClosed();
}

bool AppMenuButton::IsMenuShowing() const {
  return menu_ && menu_->IsShowing();
}

void AppMenuButton::RunMenu(std::unique_ptr<AppMenuModel> menu_model,
                            Browser* browser,
                            int run_flags,
                            bool alert_reopen_tab_items) {
  // |menu_| must be reset before |menu_model_| is destroyed, as per the comment
  // in the class declaration.
  menu_.reset();
  menu_model_ = std::move(menu_model);
  menu_model_->Init();
  menu_ = std::make_unique<AppMenu>(browser, run_flags, alert_reopen_tab_items);
  menu_->Init(menu_model_.get());

  menu_->RunMenu(menu_button_controller_);

  for (AppMenuButtonObserver& observer : observer_list_)
    observer.AppMenuShown();
}
