// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/app_menu_button.h"

#include <utility>

#include "base/observer_list.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/app_menu_button_observer.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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
}

AppMenuButton::~AppMenuButton() = default;

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
  promo_handle_.Release();
  observer_list_.Notify(&AppMenuButtonObserver::AppMenuClosed);
}

bool AppMenuButton::IsMenuShowing() const {
  return menu_ && menu_->IsShowing();
}

void AppMenuButton::RunMenu(std::unique_ptr<AppMenuModel> menu_model,
                            Browser* browser,
                            int run_flags) {
  // |menu_| must be reset before |menu_model_| is destroyed, as per the comment
  // in the class declaration.
  menu_.reset();
  menu_model_ = std::move(menu_model);
  if (BrowserWindow* browser_window = browser->window()) {
    if (auto* controller = browser_window->GetFeaturePromoController(
            base::PassKey<AppMenuButton>())) {
      if (auto* promo_specification =
              controller->GetCurrentPromoSpecificationForAnchor(
                  GetProperty(views::kElementIdentifierKey))) {
        if (auto highlighted_identifier =
                promo_specification->highlighted_menu_identifier()) {
          promo_handle_ = browser_window->CloseFeaturePromoAndContinue(
              *controller->GetCurrentPromoFeature());

          if (promo_handle_.is_valid()) {
            menu_model_->SetHighlightedIdentifier(highlighted_identifier);
          }
        }
      }
    }
  }
  menu_model_->Init();

  menu_ = std::make_unique<AppMenu>(browser, menu_model_.get(), run_flags);
  menu_->RunMenu(menu_button_controller_);

  observer_list_.Notify(&AppMenuButtonObserver::AppMenuShown);
}

void AppMenuButton::SetMenuTimerForTesting(base::ElapsedTimer timer) {
  menu_->SetTimerForTesting(std::move(timer));  // IN-TEST
}

BEGIN_METADATA(AppMenuButton)
END_METADATA
