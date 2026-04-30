// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/home_button.h"

#include "base/test/run_until.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_home_control.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/menu_model.h"

class HomeButtonUiTest : public DialogBrowserTest {
 public:
  HomeButtonUiTest() = default;
  HomeButtonUiTest(const HomeButtonUiTest&) = delete;
  HomeButtonUiTest& operator=(const HomeButtonUiTest&) = delete;
  ~HomeButtonUiTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto* const prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kShowHomeButton, true);
    HomePageUndoBubbleCoordinator coordinator(prefs);
    ui::TrackedElement* home_button_element = nullptr;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      home_button_element = BrowserElements::From(browser())->GetElement(
          kToolbarHomeButtonElementId);
      return home_button_element != nullptr;
    }));
    coordinator.Show(GURL(), false, views::BubbleAnchor(home_button_element));
  }
};

IN_PROC_BROWSER_TEST_F(HomeButtonUiTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(HomeButtonUiTest, ShowMenu) {
  ui::MenuModel* menu_model = nullptr;
  const int pin_menu_item_index = 0;
  const int unpin_menu_item_index = 1;
  const int customize_chrome_menu_index = 2;

  if (features::IsWebUIHomeButtonEnabled()) {
    menu_model = BrowserView::GetBrowserViewForBrowser(browser())
                     ->toolbar()
                     ->GetWebUIToolbarViewForTesting()
                     ->GetHomeControlForTesting()
                     ->GetMenuModelForTesting();
  } else {
    menu_model = BrowserView::GetBrowserViewForBrowser(browser())
                     ->toolbar()
                     ->home_button()
                     ->menu_model();
  }

  // While the home button is not pinned, the pin menu item should be visible.
  PrefService* const pref_service = browser()->GetProfile()->GetPrefs();
  ASSERT_FALSE(pref_service->GetBoolean(prefs::kShowHomeButton));
  EXPECT_TRUE(menu_model->IsVisibleAt(pin_menu_item_index));
  EXPECT_FALSE(menu_model->IsVisibleAt(unpin_menu_item_index));
  EXPECT_TRUE(menu_model->IsVisibleAt(customize_chrome_menu_index));

  // Activating the pin and unpin menu item should update the home button's pref
  menu_model->ActivatedAt(pin_menu_item_index);
  ASSERT_TRUE(pref_service->GetBoolean(prefs::kShowHomeButton));
  EXPECT_FALSE(menu_model->IsVisibleAt(pin_menu_item_index));
  EXPECT_TRUE(menu_model->IsVisibleAt(unpin_menu_item_index));
  EXPECT_TRUE(menu_model->IsVisibleAt(customize_chrome_menu_index));
  menu_model->ActivatedAt(unpin_menu_item_index);
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kShowHomeButton));
}
