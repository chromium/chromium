// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_interactive_uitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "ui/views/controls/button/toggle_button.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;

}  // namespace

class ExtensionsMenuMainPageViewInteractiveUITest
    : public ExtensionsToolbarUITest {
 public:
  ExtensionsMenuMainPageViewInteractiveUITest();
  ~ExtensionsMenuMainPageViewInteractiveUITest() override = default;
  ExtensionsMenuMainPageViewInteractiveUITest(
      const ExtensionsMenuMainPageViewInteractiveUITest&) = delete;
  const ExtensionsMenuMainPageViewInteractiveUITest& operator=(
      const ExtensionsMenuMainPageViewInteractiveUITest&) = delete;

  // Opens menu on "main page" by default.
  void ShowMenu();

  void ClickSiteSettingToggle();

  ExtensionsMenuMainPageView* main_page();

  // ExtensionsToolbarUITest:
  void ShowUi(const std::string& name) override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

ExtensionsMenuMainPageViewInteractiveUITest::
    ExtensionsMenuMainPageViewInteractiveUITest() {
  scoped_feature_list_.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);
}

void ExtensionsMenuMainPageViewInteractiveUITest::ShowMenu() {
  menu_coordinator()->Show(extensions_button(),
                           GetExtensionsToolbarContainer());
  DCHECK(main_page());
}

void ExtensionsMenuMainPageViewInteractiveUITest::ClickSiteSettingToggle() {
  DCHECK(main_page());

  extensions::PermissionsManagerWaiter waiter(
      PermissionsManager::Get(browser()->profile()));
  ClickButton(main_page()->GetSiteSettingsToggleForTesting());
  waiter.WaitForUserPermissionsSettingsChange();

  WaitForAnimation();
}

ExtensionsMenuMainPageView*
ExtensionsMenuMainPageViewInteractiveUITest::main_page() {
  ExtensionsMenuViewController* menu_controller =
      menu_coordinator()->GetControllerForTesting();
  DCHECK(menu_controller);
  return menu_controller->GetMainPageViewForTesting();
}

void ExtensionsMenuMainPageViewInteractiveUITest::ShowUi(
    const std::string& name) {
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // The extensions menu can appear offscreen on Linux, so verifying bounds
  // makes the tests flaky (crbug.com/1050012).
  set_should_verify_dialog_bounds(false);
#endif

  ShowMenu();
  ASSERT_TRUE(main_page());
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_ToggleSiteSetting DISABLED_ToggleSiteSetting
#else
#define MAYBE_ToggleSiteSetting ToggleSiteSetting
#endif
// Tests that toggling the site setting button changes the user site setting and
// the UI is properly updated. Note: effects will not be visible if page needs
// refresh for site setting to take effect.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveUITest,
                       MAYBE_ToggleSiteSetting) {
  ASSERT_TRUE(embedded_test_server()->Start());
  LoadTestExtension("extensions/blocked_actions/content_scripts");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = embedded_test_server()->GetURL("/simple.html");
  auto origin = url::Origin::Create(url);

  {
    content::TestNavigationObserver observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  ShowUi("");

  // By default, extension should have injected since site has "customize by
  // extension" site setting (toggle button on).
  auto* permissions_manager = PermissionsManager::Get(browser()->profile());
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_TRUE(DidInjectScript(web_contents));

  // Toggling the button OFF changes site setting to "block all
  // extensions". However, since extension was already injected in the site, it
  // remains injected.
  ClickSiteSettingToggle();
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  EXPECT_FALSE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_TRUE(DidInjectScript(web_contents));

  // Refreshing the page causes the site setting to take effect and the
  // extension is not injected.
  {
    content::TestNavigationObserver observer(web_contents);
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }
  ShowMenu();
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  EXPECT_FALSE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_FALSE(
      DidInjectScript(browser()->tab_strip_model()->GetActiveWebContents()));

  // Toggling the button ON changes site setting to "customize by
  // extension". Extension is still not injected because there was no page
  // refresh.
  ClickSiteSettingToggle();
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_FALSE(DidInjectScript(web_contents));

  // Refreshing the page causes the site setting to take effect and the
  // extension is injected.
  {
    content::TestNavigationObserver observer(web_contents);
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }
  ShowMenu();
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_TRUE(
      DidInjectScript(browser()->tab_strip_model()->GetActiveWebContents()));
}
