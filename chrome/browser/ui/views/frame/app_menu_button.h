// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_APP_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_APP_MENU_BUTTON_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/user_education/common/feature_promo_handle.h"
#include "ui/base/metadata/metadata_header_macros.h"

class AppMenu;
class AppMenuButtonObserver;
class AppMenuModel;
class Browser;

namespace views {
class MenuButtonController;
}  // namespace views

// The app menu button lives in the top right of browser windows. It shows three
// dots and adds a status badge when there's a need to alert the user. Clicking
// displays the app menu.
class AppMenuButton : public ToolbarButton {
  METADATA_HEADER(AppMenuButton, ToolbarButton)

 public:
  explicit AppMenuButton(PressedCallback callback);

  AppMenuButton(const AppMenuButton&) = delete;
  AppMenuButton& operator=(const AppMenuButton&) = delete;

  ~AppMenuButton() override;

  views::MenuButtonController* menu_button_controller() const {
    return menu_button_controller_;
  }

  void AddObserver(AppMenuButtonObserver* observer);
  void RemoveObserver(AppMenuButtonObserver* observer);

  // Closes the app menu, if it's open.
  void CloseMenu();

  // Called by the app menu when it closes.
  void OnMenuClosed();

  // Whether the app menu is currently showing.
  bool IsMenuShowing() const;

  void SetMenuTimerForTesting(base::ElapsedTimer timer);

  AppMenu* app_menu() { return menu_.get(); }
  AppMenuModel* app_menu_model() { return menu_model_.get(); }

 protected:
  // Show the menu. |menu_model| should be a newly created AppMenuModel.  The
  // other params are forwarded to the created AppMenu.
  void RunMenu(std::unique_ptr<AppMenuModel> menu_model,
               Browser* browser,
               int run_flags);

 private:
  // App model and menu.
  // Note that the menu should be destroyed before the model it uses, so the
  // menu should be listed later.
  // TODO(mgiuca): Simplify this model so that correctness does not depend on
  // destruction order. https://crbug.com/831902
  std::unique_ptr<AppMenuModel> menu_model_;
  std::unique_ptr<AppMenu> menu_;

  base::ObserverList<AppMenuButtonObserver>::Unchecked observer_list_;

  raw_ptr<views::MenuButtonController> menu_button_controller_;
  user_education::FeaturePromoHandle promo_handle_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_APP_MENU_BUTTON_H_
