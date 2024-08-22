// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_interactive_uitest.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;
using ScriptingPermissionsModifier = extensions::ScriptingPermissionsModifier;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

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

  // Asserts there is exactly one menu item and then returns it.
  ExtensionMenuItemView* GetOnlyMenuItem();

  // Returns the extension ids in the message section. If it's empty,
  // the section displaying the extensions requesting site access is not
  // visible.
  std::vector<extensions::ExtensionId> GetExtensionsInRequestAccessSection();

  // Returns the extension ids in the request access button in the toolbar.
  std::vector<extensions::ExtensionId> GetExtensionsInRequestAccessButton();

  void ClickSiteSettingToggle();

  ExtensionsMenuMainPageView* main_page();
  std::vector<ExtensionMenuItemView*> menu_items();

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

ExtensionMenuItemView*
ExtensionsMenuMainPageViewInteractiveUITest::GetOnlyMenuItem() {
  std::vector<ExtensionMenuItemView*> items = menu_items();
  if (items.size() != 1u) {
    ADD_FAILURE() << "Not exactly one item; size is: " << items.size();
    return nullptr;
  }
  return *items.begin();
}

std::vector<extensions::ExtensionId>
ExtensionsMenuMainPageViewInteractiveUITest::
    GetExtensionsInRequestAccessSection() {
  ExtensionsMenuMainPageView* page = main_page();
  // No extensions are shown in the main page is not visible or if requests
  // access container is hidden.
  if (!page || !page->GetRequestsAccessContainerForTesting()->GetVisible()) {
    return std::vector<std::string>();
  }
  return page->GetExtensionsRequestingAccessForTesting();
}

std::vector<extensions::ExtensionId>
ExtensionsMenuMainPageViewInteractiveUITest::
    GetExtensionsInRequestAccessButton() {
  return GetExtensionsToolbarContainer()
      ->GetRequestAccessButton()
      ->GetExtensionIdsForTesting();
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

std::vector<ExtensionMenuItemView*>
ExtensionsMenuMainPageViewInteractiveUITest::menu_items() {
  ExtensionsMenuMainPageView* page = main_page();
  return page ? page->GetMenuItems() : std::vector<ExtensionMenuItemView*>();
}

void ExtensionsMenuMainPageViewInteractiveUITest::ShowUi(
    const std::string& name) {
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
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
  views::View* text_container = main_page()->GetTextContainerForTesting();
  views::View* reload_container = main_page()->GetReloadContainerForTesting();
  views::View* requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();

  // When the toggle button is ON and the extension has granted access (by
  // default):
  //   - user site setting is "customize by extension".
  //   - extension is injected.
  //   - message section is hidden, meaning all containers are hidden.
  auto* permissions_manager = PermissionsManager::Get(browser()->profile());
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_TRUE(DidInjectScript(web_contents));
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());

  // Toggling the button OFF blocks all extensions on this site:
  //   - user site setting is set to "block all extensions".
  //   - since extension was already injected in the site, it remains injected.
  //   - only reload container is visible with blocked access text, since a
  //   reload needs to happen to remove the extension injection.
  ClickSiteSettingToggle();
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  EXPECT_FALSE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_TRUE(DidInjectScript(web_contents));
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_TRUE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());
  EXPECT_EQ(views::AsViewClass<views::Label>(reload_container->children()[0])
                ->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MESSAGE_SECTION_USER_BLOCKED_ACCESS_TEXT));

  // Refresh the page, and reopen the menu.
  {
    content::TestNavigationObserver observer(web_contents);
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }
  ShowMenu();
  text_container = main_page()->GetTextContainerForTesting();
  reload_container = main_page()->GetReloadContainerForTesting();
  requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();

  // When a refresh happens after blocking all extensions, the user site setting
  // takes effect:
  //   - user site setting is "block all extensions".
  //   - extension is not injected.
  //   - text container is visible with user blocked access message.
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  EXPECT_FALSE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_FALSE(
      DidInjectScript(browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_TRUE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());
  EXPECT_EQ(views::AsViewClass<views::Label>(text_container->children()[0])
                ->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MESSAGE_SECTION_USER_BLOCKED_ACCESS_TEXT));

  // Toggling the button ON allows the extensions to request site access:
  //   - user site setting is "customize by extension".
  //   - extension is still not injected because there was no page
  //     refresh.
  //   - only reload container is visible with blocked access text, since a
  //   reload needs to happen to inject the extension.
  ClickSiteSettingToggle();
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_FALSE(DidInjectScript(web_contents));
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_TRUE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());
  EXPECT_EQ(
      views::AsViewClass<views::Label>(reload_container->children()[0])
          ->GetText(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MESSAGE_SECTION_USER_CUSTOMIZED_ACCESS_TEXT));

  // Refresh the page, and reopen the menu.
  {
    content::TestNavigationObserver observer(web_contents);
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }
  ShowMenu();
  text_container = main_page()->GetTextContainerForTesting();
  reload_container = main_page()->GetReloadContainerForTesting();
  requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();

  // Refreshing the page causes the site setting to take effect:
  //   - user site setting is "customize by extension".
  //   - extension is injected.
  //   - message section is hidden, meaning all containers are hidden. Note
  //   requests access container is not visible because there is no extension
  //   requesting site access.
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_TRUE(
      DidInjectScript(browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());
}

// Test that running an extension's action, when site permission were withheld,
// sets the extension's site access toggle on. It also tests that the menu's
// message section and the toolbar's request access button are properly
// updated.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveUITest,
                       SiteAccessToggle_RunAction) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto extension =
      InstallExtensionWithHostPermissions("Extension", "<all_urls>");
  auto extension_id = extension->id();
  extensions::ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  // Navigate to a.com and add a site access request for the extension.
  GURL urlA = embedded_test_server()->GetURL("a.com", "/title1.html");
  NavigateTo(urlA);
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  AddSiteAccessRequest(*extension, web_contents);

  ShowUi("");
  views::View* text_container = main_page()->GetTextContainerForTesting();
  views::View* reload_container = main_page()->GetReloadContainerForTesting();
  views::View* requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Verify user site setting is "customize by extension" (default) and
  // the extension has "on click" site access.
  auto* permissions_manager = PermissionsManager::Get(browser()->profile());
  ASSERT_EQ(permissions_manager->GetUserSiteSetting(url::Origin::Create(urlA)),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  ASSERT_EQ(permissions_manager->GetUserSiteAccess(*extension.get(), urlA),
            PermissionsManager::UserSiteAccess::kOnClick);

  // When extension has added a site access request:
  //   - site access toggle is visible and off.
  //   - message section only shows request access container and includes
  //   extension.
  //   - request access button, in the toolbar, includes extension.
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension_id));
  EXPECT_THAT(GetExtensionsInRequestAccessButton(),
              testing::ElementsAre(extension_id));

  // When extension has granted site access, after running the extension action:
  //   - site access toggle is visible and on.
  //   - message section is hidden, meaning all containers are not visible.
  //   - request access button, in the toolbar, does not include extension.
  ClickButton(menu_item->primary_action_button_for_testing());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());
  EXPECT_TRUE(GetExtensionsInRequestAccessSection().empty());
  EXPECT_TRUE(GetExtensionsInRequestAccessButton().empty());

  // Navigate back to the original site.
  // Note that we don't revoke permissions when navigation is to the same origin
  // (e.g refreshing the page). Thus, we navigate to other site and then back to
  // original one.
  GURL urlB = embedded_test_server()->GetURL("b.com", "/title1.html");
  NavigateTo(urlB);
  NavigateTo(urlA);
  ShowMenu();

  text_container = main_page()->GetTextContainerForTesting();
  reload_container = main_page()->GetReloadContainerForTesting();
  requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();
  menu_item = GetOnlyMenuItem();

  // When navigating back to the original site, after a cross-origin navigation:
  //   - site access toggle is visible and off.
  //   - message section is hidden, meaning all containers are not visible.
  //   - request access button, in the toolbar, does not include extension.
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());
  EXPECT_TRUE(GetExtensionsInRequestAccessSection().empty());
  EXPECT_TRUE(GetExtensionsInRequestAccessButton().empty());
}

// Tests that the extensions menu is updated only when the web contents update
// is for the same web contents the menu is been displayed for.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveUITest,
                       UpdatePageForActiveWebContentsChanges) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto extension =
      InstallExtensionWithHostPermissions("Extension", "<all_urls>");

  ASSERT_TRUE(AddTabAtIndex(0, embedded_test_server()->GetURL("/title1.html"),
                            ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL("chrome://extensions"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(1);

  ShowUi("");
  // TODO(crbug.com/40879945): This label should be 'No extensions need access
  // to chrome://extensions'. Change once such label is moved from the message
  // to the site settings label.
  EXPECT_EQ(main_page()->GetSiteSettingLabelForTesting(),
            u"Allow extensions on chrome://extensions");

  // Update the title of the unfocused tab.
  browser()->set_update_ui_immediately_for_testing();
  content::WebContents* unfocused_tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  std::u16string updated_title = u"Updated Title";
  content::TitleWatcher title_watcher(unfocused_tab, updated_title);
  ASSERT_TRUE(
      content::ExecJs(unfocused_tab, "document.title = 'Updated Title';"));
  ASSERT_EQ(title_watcher.WaitAndGetTitle(), updated_title);
  // The browser UI is updated by a PostTask() with a delay of zero
  // seconds. However, the update will be visible when the run loop next
  // idles after the title is updated. To ensure it ran, wait until it's idle.
  base::RunLoop().RunUntilIdle();

  // Verify extensions menu content wasn't affected by checking the site
  // displayed on the menu's subtitle.
  // TODO(crbug.com/40879945): This label should be 'No extensions need access
  // to chrome://extensions'. Change once such label is moved from the message
  // to the site settings label.
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 1);
  EXPECT_EQ(main_page()->GetSiteSettingLabelForTesting(),
            u"Allow extensions on chrome://extensions");
}

// Verifies extensions can add site access requests on active and inactive tabs,
// but the menu only shows extension's requests for the current tab.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveUITest,
                       SiteAccessRequestsForMultipleTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install two extensions and withhold their host permissions.
  auto extensionA =
      InstallExtensionWithHostPermissions("Extension A", "<all_urls>");
  auto extensionB =
      InstallExtensionWithHostPermissions("Extension B", "<all_urls>");
  ScriptingPermissionsModifier(profile(), extensionA)
      .SetWithholdHostPermissions(true);
  ScriptingPermissionsModifier(profile(), extensionB)
      .SetWithholdHostPermissions(true);

  // Open two tabs.
  int tab1_index = 0;
  int tab2_index = 1;
  const GURL tab1_url =
      embedded_test_server()->GetURL("first.com", "/title1.html");
  const GURL tab2_url =
      embedded_test_server()->GetURL("second.com", "/title1.html");
  ASSERT_TRUE(AddTabAtIndex(tab1_index, tab1_url, ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(AddTabAtIndex(tab2_index, tab1_url, ui::PAGE_TRANSITION_TYPED));

  // Retrieve tab's information.
  content::WebContents* tab1_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(tab1_index);
  content::WebContents* tab2_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(tab2_index);
  int tab1_id = extensions::ExtensionTabUtil::GetTabId(tab1_web_contents);
  int tab2_id = extensions::ExtensionTabUtil::GetTabId(tab2_web_contents);

  // Activate the first tab and open the menu. Verify there are no site access
  // requests on the menu.
  browser()->tab_strip_model()->ActivateTabAt(tab1_index);
  ShowUi("");
  views::View* requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();
  EXPECT_FALSE(requests_access_container->GetVisible());

  // Add a site access request for extension A on the (active) first tab.
  // Verify  extension A site access request is visible on the menu.
  auto* permissions_manager = PermissionsManager::Get(browser()->profile());
  permissions_manager->AddSiteAccessRequest(tab1_web_contents, tab1_id,
                                            *extensionA);
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extensionA->id()));

  // Add a site access request for extension B on the (inactive) second tab.
  // Verify only extension A site access request is visible on the menu.
  permissions_manager->AddSiteAccessRequest(tab2_web_contents, tab2_id,
                                            *extensionB);
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extensionA->id()));

  // Add a site access request for extension A on the (inactive) second tab.
  // Verify only extension A site access request is visible on the menu.
  permissions_manager->AddSiteAccessRequest(tab2_web_contents, tab2_id,
                                            *extensionA);
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extensionA->id()));

  // Remove the site access request for extension A on the (inactive) second
  // tab. Verify extension A site access request is still visible on the menu,
  // since request is still active for the first tab.
  permissions_manager->RemoveSiteAccessRequest(tab2_id, extensionA->id());
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extensionA->id()));
}
