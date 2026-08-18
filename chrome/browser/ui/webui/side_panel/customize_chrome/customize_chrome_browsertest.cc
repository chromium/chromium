// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

class CustomizeChromeSidePanelBrowserTest : public PlatformBrowserTest {
 protected:
  // Activates the browser tab at `index`.
  void ActivateTabAt(int index) {
    TabListInterface* tab_list = GetTabListInterface();
    tab_list->ActivateTab(tab_list->GetTab(index)->GetHandle());
  }

  // Appends a new tab with `url` to the end of the tabstrip.
  void AppendTab(const GURL& url) {
    GetTabListInterface()->OpenTab(url, -1, true);
  }

  // Returns the Customize Chrome side panel controller for the active tab.
  customize_chrome::SidePanelController* GetSidePanelController() {
    return customize_chrome::SidePanelController::Get(
        GetTabListInterface()->GetActiveTab()->GetUnownedUserDataHost());
  }
};

class UnsupportedCustomizeChromeSidePanelBrowserTest
    : public CustomizeChromeSidePanelBrowserTest {
 protected:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    PlatformBrowserTest::SetUpBrowserContextKeyedServices(context);
    NtpCustomBackgroundServiceFactory::GetInstance()->SetTestingFactory(context,
                                                                        {});
  }
};

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       RegisterCustomizeChromeSidePanel) {
  auto* customize_chrome_side_panel_controller = GetSidePanelController();

  // When navigating to the New Tab Page, the Customize Chrome entry should be
  // available
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           chrome::ChromeUINewTabURLAsGURL()));
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
                       CustomizeChromeSidePanelRemainsOpenAfterNavigation) {
  // Toolbar pinning keeps an open Customize Chrome side panel registered and
  // visible after navigating away from the NTP.
  auto* customize_chrome_side_panel_controller = GetSidePanelController();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           chrome::ChromeUINewTabURLAsGURL()));
  customize_chrome_side_panel_controller->OpenSidePanel(
      SidePanelOpenTrigger::kAppMenu, CustomizeChromeSection::kAppearance);
  EXPECT_TRUE(
      customize_chrome_side_panel_controller->IsCustomizeChromeEntryShowing());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GURL(chrome::kChromeUISettingsURL)));

  EXPECT_TRUE(customize_chrome_side_panel_controller
                  ->IsCustomizeChromeEntryAvailable());
  EXPECT_TRUE(
      customize_chrome_side_panel_controller->IsCustomizeChromeEntryShowing());
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       ContextualCustomizeChromeSidePanel) {
  SidePanelUI::From(GetBrowserWindowInterface())->DisableAnimationsForTesting();
  // The Customize Chrome side panel should be contextual, opening on one tab
  // should not open it on other tabs.
  AppendTab(chrome::ChromeUINewTabURLAsGURL());
  AppendTab(chrome::ChromeUINewTabURLAsGURL());
  ActivateTabAt(1);
  // Navigate to URL to allow WebUI to load, if not then callback that is set
  // in the New Tab Page constructor and run when
  // OpenSidePanel() is called will not be set.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           chrome::ChromeUINewTabURLAsGURL()));
  auto* customize_chrome_side_panel_controller1 = GetSidePanelController();
  EXPECT_FALSE(
      customize_chrome_side_panel_controller1->IsCustomizeChromeEntryShowing());
  customize_chrome_side_panel_controller1->OpenSidePanel(
      SidePanelOpenTrigger::kAppMenu, CustomizeChromeSection::kAppearance);
  ActivateTabAt(2);
  auto* customize_chrome_side_panel_controller2 = GetSidePanelController();
  EXPECT_FALSE(
      customize_chrome_side_panel_controller2->IsCustomizeChromeEntryShowing());
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       HideCustomizeChromeSidePanel) {
  SidePanelUI::From(GetBrowserWindowInterface())->DisableAnimationsForTesting();
  auto* customize_chrome_side_panel_controller = GetSidePanelController();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           chrome::ChromeUINewTabURLAsGURL()));
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

IN_PROC_BROWSER_TEST_F(UnsupportedCustomizeChromeSidePanelBrowserTest,
                       DoesNotRegisterCustomizeChromeEntryWhenUnsupported) {
  auto* customize_chrome_side_panel_controller = GetSidePanelController();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GURL("about:blank")));
  EXPECT_FALSE(customize_chrome_side_panel_controller
                   ->IsCustomizeChromeEntryAvailable());
}

IN_PROC_BROWSER_TEST_F(CustomizeChromeSidePanelBrowserTest,
                       RepeatedNavigationsDoNotReregisterCustomizeChromeEntry) {
  // Repeated navigations keep the original entry instead of replacing it.
  auto* registry =
      SidePanelRegistry::From(GetTabListInterface()->GetActiveTab());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           chrome::ChromeUINewTabURLAsGURL()));
  SidePanelEntry* entry = registry->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome));
  ASSERT_NE(entry, nullptr);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowserWindowInterface(),
                                           GURL(chrome::kChromeUISettingsURL)));
  EXPECT_EQ(entry, registry->GetEntryForKey(SidePanelEntry::Key(
                       SidePanelEntry::Id::kCustomizeChrome)));
}
