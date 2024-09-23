// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_site_permissions_page_view.h"

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

class ExtensionsMenuSitePermissionsPageViewInteractiveUITest
    : public ExtensionsToolbarUITest {
 public:
  ExtensionsMenuSitePermissionsPageViewInteractiveUITest();
  ~ExtensionsMenuSitePermissionsPageViewInteractiveUITest() override = default;
  ExtensionsMenuSitePermissionsPageViewInteractiveUITest(
      const ExtensionsMenuSitePermissionsPageViewInteractiveUITest&) = delete;
  const ExtensionsMenuSitePermissionsPageViewInteractiveUITest& operator=(
      const ExtensionsMenuSitePermissionsPageViewInteractiveUITest&) = delete;

  // Opens menu and navigates to site permissions page for `extension_id`.
  void ShowSitePermissionsPage(extensions::ExtensionId extension_id);

  // Returns whether the menu has the main page opened.
  bool IsMainPageOpened();

  // Returns whether the menu has site permissions page for `extension_id`
  // opened.
  bool IsSitePermissionsPageOpened(extensions::ExtensionId extension_id);

  ExtensionsMenuMainPageView* main_page();
  ExtensionsMenuSitePermissionsPageView* site_permissions_page();

  // ExtensionsToolbarUITest:
  void ShowUi(const std::string& extension_id) override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

ExtensionsMenuSitePermissionsPageViewInteractiveUITest::
    ExtensionsMenuSitePermissionsPageViewInteractiveUITest() {
  scoped_feature_list_.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);
}

void ExtensionsMenuSitePermissionsPageViewInteractiveUITest::
    ShowSitePermissionsPage(extensions::ExtensionId extension_id) {
  menu_coordinator()->Show(extensions_button(),
                           GetExtensionsToolbarContainer());
  menu_coordinator()->GetControllerForTesting()->OpenSitePermissionsPage(
      extension_id);
}

bool ExtensionsMenuSitePermissionsPageViewInteractiveUITest::
    IsMainPageOpened() {
  ExtensionsMenuMainPageView* page = main_page();
  return !!page;
}

bool ExtensionsMenuSitePermissionsPageViewInteractiveUITest::
    IsSitePermissionsPageOpened(extensions::ExtensionId extension_id) {
  ExtensionsMenuSitePermissionsPageView* page = site_permissions_page();
  return page && page->extension_id() == extension_id;
}

ExtensionsMenuMainPageView*
ExtensionsMenuSitePermissionsPageViewInteractiveUITest::main_page() {
  ExtensionsMenuViewController* menu_controller =
      menu_coordinator()->GetControllerForTesting();
  DCHECK(menu_controller);
  return menu_controller->GetMainPageViewForTesting();
}

ExtensionsMenuSitePermissionsPageView*
ExtensionsMenuSitePermissionsPageViewInteractiveUITest::
    site_permissions_page() {
  ExtensionsMenuViewController* menu_controller =
      menu_coordinator()->GetControllerForTesting();
  DCHECK(menu_controller);
  return menu_controller->GetSitePermissionsPageForTesting();
}

void ExtensionsMenuSitePermissionsPageViewInteractiveUITest::ShowUi(
    const std::string& extension_id) {
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // The extensions menu can appear offscreen on Linux, so verifying bounds
  // makes the tests flaky (crbug.com/1050012).
  set_should_verify_dialog_bounds(false);
#endif

  ShowSitePermissionsPage(extension_id);
  ASSERT_TRUE(IsSitePermissionsPageOpened(extension_id));
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_UpdateSiteSetting DISABLED_UpdateSiteSetting
#else
#define MAYBE_UpdateSiteSetting UpdateSiteSetting
#endif
// Tests that updating the user site setting, outside the menu, properly updates
// the UI. Note: effects will not be visible if page needs refresh for site
// setting to take effect.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuSitePermissionsPageViewInteractiveUITest,
                       MAYBE_UpdateSiteSetting) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto extension =
      LoadTestExtension("extensions/blocked_actions/content_scripts");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = embedded_test_server()->GetURL("/simple.html");
  auto url_origin = url::Origin::Create(url);

  content::TestNavigationObserver observer(web_contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // By default, extension should have injected since site has "customize by
  // extension" site setting and is granted access.
  auto* permissions_manager = PermissionsManager::Get(browser()->profile());
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(url_origin),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(DidInjectScript(web_contents));

  // Open extension's site permissions page.
  ShowUi(extension->id());
  EXPECT_TRUE(IsSitePermissionsPageOpened(extension->id()));
  EXPECT_FALSE(IsMainPageOpened());

  // Changing the user site setting should navigate back to the main page.
  // However, since extension was already injected in the site, it remains
  // injected.
  extensions::PermissionsManagerWaiter waiter(permissions_manager);
  permissions_manager->UpdateUserSiteSetting(
      url_origin, PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  waiter.WaitForUserPermissionsSettingsChange();

  EXPECT_FALSE(IsSitePermissionsPageOpened(extension->id()));
  EXPECT_TRUE(IsMainPageOpened());
  EXPECT_TRUE(DidInjectScript(web_contents));
}
