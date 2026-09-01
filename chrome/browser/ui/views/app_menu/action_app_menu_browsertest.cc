// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_search_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"

class ActionAppMenuBrowserTest : public InProcessBrowserTest {
 public:
  ActionAppMenuBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAppMenuGlowUp);
  }
  ~ActionAppMenuBrowserTest() override = default;

  BrowserAppMenuButton* GetMenuButton() {
    return views::AsViewClass<BrowserAppMenuButton>(
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kToolbarAppMenuButtonElementId,
            BrowserView::GetBrowserViewForBrowser(browser())
                ->GetElementContext()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActionAppMenuBrowserTest, ShowActionAppMenu) {
  BrowserAppMenuButton* menu_button = GetMenuButton();
  ASSERT_TRUE(menu_button);

  EXPECT_FALSE(menu_button->IsMenuShowing());
  EXPECT_FALSE(menu_button->action_app_menu());

  menu_button->ShowMenu(views::MenuRunner::NO_FLAGS);

  EXPECT_TRUE(menu_button->IsMenuShowing());
  ActionAppMenu* action_menu = menu_button->action_app_menu();
  ASSERT_TRUE(action_menu);
  EXPECT_FALSE(menu_button->app_menu());

  views::MenuItemView* root = action_menu->root_menu_item_for_testing();
  ASSERT_TRUE(root);

  // Verify that the Action items have been converted into visual menu items.
  views::MenuItemView* password_item =
      root->GetMenuItemByID(kActionPasswordsAndAutofillSubmenu);
  ASSERT_TRUE(password_item);

  views::MenuItemView* print_item = root->GetMenuItemByID(kActionPrint);
  ASSERT_TRUE(print_item);

  views::MenuItemView* block_item = root->GetSubmenu()->GetMenuItemAt(0);
  ASSERT_TRUE(block_item);

  // Check if the menu items have background styling
  EXPECT_TRUE(password_item->GetMenuItemBackground().has_value());
  EXPECT_TRUE(print_item->GetMenuItemBackground().has_value());

  menu_button->CloseMenu();
  EXPECT_FALSE(menu_button->IsMenuShowing());
}

IN_PROC_BROWSER_TEST_F(ActionAppMenuBrowserTest, ShowActionAppMenuDarkMode) {
  ThemeServiceFactory::GetForProfile(browser()->GetProfile())
      ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kDark);

  BrowserAppMenuButton* menu_button = GetMenuButton();
  ASSERT_TRUE(menu_button);

  menu_button->ShowMenu(views::MenuRunner::NO_FLAGS);
  EXPECT_TRUE(menu_button->IsMenuShowing());
  ActionAppMenu* action_menu = menu_button->action_app_menu();
  ASSERT_TRUE(action_menu);

  views::MenuItemView* root = action_menu->root_menu_item_for_testing();
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->HasSubmenu());
  views::SubmenuView* submenu = root->GetSubmenu();
  const ui::ColorProvider* color_provider = submenu->GetColorProvider();
  ASSERT_TRUE(color_provider);

  for (views::MenuItemView* child : submenu->GetMenuItems()) {
    if (child->GetMenuItemBackground().has_value()) {
      ui::ColorId bg_id = child->GetMenuItemBackground()->background_color_id;
      SkColor bg_color = color_provider->GetColor(bg_id);
      EXPECT_NE(bg_color, gfx::kPlaceholderColor)
          << "Item with id " << child->GetCommand()
          << " has placeholder color red background!";
    }
  }

  menu_button->CloseMenu();
  EXPECT_FALSE(menu_button->IsMenuShowing());
}

class ActionAppMenuWithSearchBrowserTest : public ActionAppMenuBrowserTest {
 public:
  ActionAppMenuWithSearchBrowserTest() {
    search_feature_list_.InitAndEnableFeature(features::kChroMenuSearch);
  }
  ~ActionAppMenuWithSearchBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList search_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActionAppMenuWithSearchBrowserTest,
                       ShowActionAppMenuWithSearch) {
  BrowserAppMenuButton* menu_button = GetMenuButton();
  ASSERT_TRUE(menu_button);

  EXPECT_FALSE(menu_button->IsMenuShowing());
  EXPECT_FALSE(menu_button->action_app_menu());

  menu_button->ShowMenu(views::MenuRunner::NO_FLAGS);

  EXPECT_TRUE(menu_button->IsMenuShowing());
  ActionAppMenu* action_menu = menu_button->action_app_menu();
  ASSERT_TRUE(action_menu);

  ActionAppMenuSearchBarView* search_bar =
      action_menu->search_bar_for_testing();
  ASSERT_TRUE(search_bar);
  EXPECT_TRUE(search_bar->search_icon_for_testing()->GetVisible());

  menu_button->CloseMenu();
  EXPECT_FALSE(menu_button->IsMenuShowing());
}
