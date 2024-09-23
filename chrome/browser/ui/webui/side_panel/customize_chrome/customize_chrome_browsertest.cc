// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"

class CustomizeChromeSidePanelBrowserTest : public InProcessBrowserTest {
 protected:
  // Activates the browser tab at `index`.
  void ActivateTabAt(Browser* browser, int index) {
    browser->tab_strip_model()->ActivateTabAt(index);
  }

  // Appends a new tab with `url` to the end of the tabstrip.
  void AppendTab(Browser* browser, const GURL& url) {
    chrome::AddTabAt(browser, url, -1, true);
  }

  // Returns the CustomizeChromeTabHelper associated with the tab
  customize_chrome::SidePanelController* GetSidePanelController(
      Browser* browser) {
    return browser->GetActiveTabInterface()
        ->GetTabFeatures()
        ->customize_chrome_side_panel_controller();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       RegisterCustomizeChromeSidePanel) {
  auto* customize_chrome_side_panel_controller =
      GetSidePanelController(browser());

  // When navigating to the New Tab Page, the Customize Chrome entry should be
  // available
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  EXPECT_TRUE(customize_chrome_side_panel_controller
                  ->IsCustomizeChromeEntryAvailable());

  // After calling show, the customize chrome entry should be shown in the side
  // panel
  customize_chrome_side_panel_controller->OpenSidePanel(
      SidePanelOpenTrigger::kAppMenu, CustomizeChromeSection::kAppearance);
  EXPECT_TRUE(
      customize_chrome_side_panel_controller->IsCustomizeChromeEntryShowing());
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       DeregisterCustomizeChromeSidePanel) {
  // If the Customize Chrome side panel is open and you navigate away from the
  // NTP the side panel entry should not be in the tabs' registry and the side
  // panel should not show the customize chrome entry
  auto* customize_chrome_side_panel_controller =
      GetSidePanelController(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  customize_chrome_side_panel_controller->OpenSidePanel(
      SidePanelOpenTrigger::kAppMenu, CustomizeChromeSection::kAppearance);
  EXPECT_TRUE(
      customize_chrome_side_panel_controller->IsCustomizeChromeEntryShowing());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUISettingsURL)));

  // Toolbar Pinning allows the cusst
  if (features::IsToolbarPinningEnabled()) {
    EXPECT_TRUE(customize_chrome_side_panel_controller
                    ->IsCustomizeChromeEntryAvailable());
    EXPECT_TRUE(customize_chrome_side_panel_controller
                    ->IsCustomizeChromeEntryShowing());
  } else {
    EXPECT_FALSE(customize_chrome_side_panel_controller
                     ->IsCustomizeChromeEntryAvailable());
    EXPECT_FALSE(customize_chrome_side_panel_controller
                     ->IsCustomizeChromeEntryShowing());
  }
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       ContextualCustomizeChromeSidePanel) {
  browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
  // The Customize Chrome side panel should be contextual, opening on one tab
  // should not open it on other tabs.
  AppendTab(browser(), GURL(chrome::kChromeUINewTabURL));
  AppendTab(browser(), GURL(chrome::kChromeUINewTabURL));
  ActivateTabAt(browser(), 1);
  // Navigate to URL to allow WebUI to load, if not then callback that is set
  // in the New Tab Page constructor and run when
  // OpenSidePanel() is called will not be set.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  auto* customize_chrome_side_panel_controller1 =
      GetSidePanelController(browser());
  EXPECT_FALSE(
      customize_chrome_side_panel_controller1->IsCustomizeChromeEntryShowing());
  customize_chrome_side_panel_controller1->OpenSidePanel(
      SidePanelOpenTrigger::kAppMenu, CustomizeChromeSection::kAppearance);
  ActivateTabAt(browser(), 2);
  auto* customize_chrome_side_panel_controller2 =
      GetSidePanelController(browser());
  EXPECT_FALSE(
      customize_chrome_side_panel_controller2->IsCustomizeChromeEntryShowing());
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       HideCustomizeChromeSidePanel) {
  browser()->GetFeatures().side_panel_ui()->DisableAnimationsForTesting();
  auto* customize_chrome_side_panel_controller =
      GetSidePanelController(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  customize_chrome_side_panel_controller->OpenSidePanel(
      SidePanelOpenTrigger::kAppMenu, CustomizeChromeSection::kAppearance);
  EXPECT_TRUE(
      customize_chrome_side_panel_controller->IsCustomizeChromeEntryShowing());
  // After calling hide, the customize chrome entry should be hidden in the side
  // panel
  customize_chrome_side_panel_controller->CloseSidePanel();
  EXPECT_FALSE(
      customize_chrome_side_panel_controller->IsCustomizeChromeEntryShowing());
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       DeregisterEmptyCustomizeChromeEntry) {
  // When there is no customize chrome entry, calling deregister should
  // not crash.
  auto* customize_chrome_side_panel_controller =
      GetSidePanelController(browser());
  customize_chrome_side_panel_controller->DeregisterEntryForTesting();
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       RegisterCustomizeChromeEntry) {
  // When CreateAndRegisterEntry() is called, the current tabs side
  // panel registry should contain a kCustomizeChromeEntry.
  auto* customize_chrome_side_panel_controller =
      GetSidePanelController(browser());
  customize_chrome_side_panel_controller->CreateAndRegisterEntryForTesting();
  auto* registry = browser()
                       ->GetActiveTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();
  EXPECT_EQ(registry
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome))
                ->key()
                .id(),
            SidePanelEntry::Id::kCustomizeChrome);
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       DeregisterCustomizeChromeEntry) {
  // When Deregister() is called, there should be no side panel entry
  // in the registry.
  auto* customize_chrome_side_panel_controller =
      GetSidePanelController(browser());
  auto* registry = browser()
                       ->GetActiveTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();

  customize_chrome_side_panel_controller->CreateAndRegisterEntryForTesting();
  EXPECT_EQ(registry
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome))
                ->key()
                .id(),
            SidePanelEntry::Id::kCustomizeChrome);
  customize_chrome_side_panel_controller->DeregisterEntryForTesting();
  EXPECT_EQ(registry->GetEntryForKey(
                SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome)),
            nullptr);
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       CreateAndRegisterMultipleTimes) {
  // When CreateAndRegisterEntry() is called multiple times, only
  // one entry should be added to the registry.
  auto* customize_chrome_side_panel_controller =
      GetSidePanelController(browser());
  auto* registry = browser()
                       ->GetActiveTabInterface()
                       ->GetTabFeatures()
                       ->side_panel_registry();

  customize_chrome_side_panel_controller->CreateAndRegisterEntryForTesting();
  EXPECT_EQ(registry
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome))
                ->key()
                .id(),
            SidePanelEntry::Id::kCustomizeChrome);
  customize_chrome_side_panel_controller->CreateAndRegisterEntryForTesting();
  EXPECT_EQ(registry
                ->GetEntryForKey(
                    SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome))
                ->key()
                .id(),
            SidePanelEntry::Id::kCustomizeChrome);
  customize_chrome_side_panel_controller->DeregisterEntryForTesting();
  EXPECT_EQ(registry->GetEntryForKey(
                SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome)),
            nullptr);
}
