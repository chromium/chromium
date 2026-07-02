// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/action_app_menu.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/menu/menu_runner.h"
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
  EXPECT_TRUE(menu_button->action_app_menu());
  EXPECT_FALSE(menu_button->app_menu());

  menu_button->CloseMenu();
  EXPECT_FALSE(menu_button->IsMenuShowing());
}
