// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_interactive_uitest.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;
using ScriptingPermissionsModifier = extensions::ScriptingPermissionsModifier;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

enum class ExtensionHostState { kNone = 0, kLoaded = 1, kDestroyed = 2 };

class ExtensionHostObserver : public ui::test::ObservationStateObserver<
                                  ExtensionHostState,
                                  extensions::ExtensionHostRegistry,
                                  extensions::ExtensionHostRegistry::Observer> {
 public:
  explicit ExtensionHostObserver(
      extensions::ExtensionHostRegistry* host_registry,
      const extensions::ExtensionId& extension_id)
      : ObservationStateObserver(host_registry), extension_id_(extension_id) {
    host_state_ = ExtensionHostState::kNone;
  }
  ~ExtensionHostObserver() override = default;

 protected:
  // ExtensionHostRegistry::Observer:
  void OnExtensionHostCompletedFirstLoad(
      content::BrowserContext* browser_context,
      extensions::ExtensionHost* host) override {
    if (host->extension_id() != extension_id_) {
      return;
    }

    host_state_ = ExtensionHostState::kLoaded;
    OnStateObserverStateChanged(host_state_);
  }

  void OnExtensionHostDestroyed(content::BrowserContext* browser_context,
                                extensions::ExtensionHost* host) override {
    if (host->extension_id() != extension_id_) {
      return;
    }

    host_state_ = ExtensionHostState::kDestroyed;
    OnStateObserverStateChanged(host_state_);
  }

 private:
  extensions::ExtensionId extension_id_;
  ExtensionHostState host_state_;
};

class PermissionsUpdatesObserver
    : public ui::test::ObservationStateObserver<
          extensions::PermissionsManager::UserSiteAccess,
          extensions::PermissionsManager,
          extensions::PermissionsManager::Observer> {
 public:
  explicit PermissionsUpdatesObserver(
      extensions::PermissionsManager* permissions_manager,
      const extensions::ExtensionId& extension_id,
      const GURL& url)
      : ObservationStateObserver(permissions_manager),
        permissions_manager_(permissions_manager),
        extension_id_(extension_id),
        url_(url) {}
  ~PermissionsUpdatesObserver() override = default;

 protected:
  // PermissionsManager::Observer
  void OnExtensionPermissionsUpdated(
      const extensions::Extension& extension,
      const extensions::PermissionSet& permissions,
      PermissionsManager::UpdateReason reason) override {
    if (extension.id() == extension_id_) {
      extensions::PermissionsManager::UserSiteAccess site_access =
          permissions_manager_->GetUserSiteAccess(extension, url_);
      OnStateObserverStateChanged(site_access);
    }
  }

 private:
  raw_ptr<extensions::PermissionsManager> permissions_manager_;
  const extensions::ExtensionId extension_id_;
  const GURL url_;
};

// Returns the command id in the context menu corresponding to `site_access`.
ui::ElementIdentifier GetSiteAccessCommandId(
    extensions::PermissionsManager::UserSiteAccess site_access) {
  switch (site_access) {
    case extensions::PermissionsManager::UserSiteAccess::kOnClick:
      return extensions::ExtensionContextMenuModel::
          kPageAccessRunOnClickSubmenuItem;
    case extensions::PermissionsManager::UserSiteAccess::kOnSite:
      return extensions::ExtensionContextMenuModel::
          kPageAccessRunOnSiteSubmenuItem;
    case extensions::PermissionsManager::UserSiteAccess::kOnAllSites:
      return extensions::ExtensionContextMenuModel::
          kPageAccessRunOnAllSitesSubmenuItem;
  }
}

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
  // No extensions are shown in the main page if main page is not visible or if
  // requests section is hidden.
  if (!page || !page->requests_section()->GetVisible()) {
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
#if BUILDFLAG(IS_LINUX)
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

  // When the toggle button is ON and the extension has granted access (by
  // default):
  //   - user site setting is "customize by extension".
  //   - extension is injected.
  //   - reload section is hidden
  //   - requests section is hidden
  auto* permissions_manager = PermissionsManager::Get(browser()->profile());
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_TRUE(DidInjectScript(web_contents));
  EXPECT_FALSE(main_page()->reload_section()->GetVisible());
  EXPECT_FALSE(main_page()->requests_section()->GetVisible());

  // Toggling the button OFF blocks all extensions on this site:
  //   - user site setting is set to "block all extensions".
  //   - since extension was already injected in the site, it remains injected.
  //   - reload section is visible, since a page refresh is needed to apply
  //     changes
  //   - requests section is hidden
  ClickSiteSettingToggle();
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  EXPECT_FALSE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_TRUE(DidInjectScript(web_contents));
  EXPECT_TRUE(main_page()->reload_section()->GetVisible());
  EXPECT_FALSE(main_page()->requests_section()->GetVisible());

  // Refresh the page, and reopen the menu.
  {
    content::TestNavigationObserver observer(web_contents);
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }
  ShowMenu();

  // When a refresh happens after blocking all extensions, the user site setting
  // takes effect:
  //   - user site setting is "block all extensions".
  //   - extension is not injected.
  //   - reload section is hidden
  //   - requests section is hidden
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  EXPECT_FALSE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_FALSE(
      DidInjectScript(browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_FALSE(main_page()->reload_section()->GetVisible());
  EXPECT_FALSE(main_page()->requests_section()->GetVisible());

  // Toggling the button ON allows the extensions to request site access:
  //   - user site setting is "customize by extension".
  //   - extension is still not injected because there was no page
  //     refresh.
  //   - reload section is visible, since a page refresh is needed to apply
  //     changes
  //   - requests section is hidden
  ClickSiteSettingToggle();
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_FALSE(DidInjectScript(web_contents));
  EXPECT_TRUE(main_page()->reload_section()->GetVisible());
  EXPECT_FALSE(main_page()->requests_section()->GetVisible());

  // Refresh the page, and reopen the menu.
  {
    content::TestNavigationObserver observer(web_contents);
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }
  ShowMenu();

  // Refreshing the page causes the site setting to take effect:
  //   - user site setting is "customize by extension".
  //   - extension is injected.
  //   - reload section is hidden
  //   - requests section is hidden
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(main_page()->GetSiteSettingsToggleForTesting()->GetIsOn());
  EXPECT_TRUE(
      DidInjectScript(browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_FALSE(main_page()->reload_section()->GetVisible());
  EXPECT_FALSE(main_page()->requests_section()->GetVisible());
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
  AddHostAccessRequest(*extension, web_contents);

  ShowUi("");
  const views::View* reload_section = main_page()->reload_section();
  const views::View* requests_section = main_page()->requests_section();
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
  //   - reload section is hidden.
  //   - requests section is visible and has extension.
  //   - request access button, in the toolbar, includes extension.
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_FALSE(reload_section->GetVisible());
  EXPECT_TRUE(requests_section->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension_id));
  EXPECT_THAT(GetExtensionsInRequestAccessButton(),
              testing::ElementsAre(extension_id));

  // When extension has granted site access, after running the extension action:
  //   - site access toggle is visible and on.
  //   - reload section is hidden.
  //   - requests section is hidden
  //   - request access button, in the toolbar, does not include extension.
  ClickButton(menu_item->primary_action_button_for_testing());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_FALSE(reload_section->GetVisible());
  EXPECT_FALSE(requests_section->GetVisible());
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

  reload_section = main_page()->reload_section();
  requests_section = main_page()->requests_section();
  menu_item = GetOnlyMenuItem();

  // When navigating back to the original site, after a cross-origin navigation:
  //   - site access toggle is visible and off.
  //   - reload section is hidden.
  //   - requests section is hidden.
  //   - request access button, in the toolbar, does not include extension.
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_FALSE(reload_section->GetVisible());
  EXPECT_FALSE(requests_section->GetVisible());
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

  EXPECT_EQ(main_page()->GetSiteSettingLabelForTesting(),
            u"Extensions are not allowed on chrome://extensions");

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
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 1);
  EXPECT_EQ(main_page()->GetSiteSettingLabelForTesting(),
            u"Extensions are not allowed on chrome://extensions");
}

// Verifies extensions can add site access requests on active and inactive tabs,
// but the menu only shows extension's requests for the current tab.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveUITest,
                       HostAccessRequestsForMultipleTabs) {
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
  const views::View* requests_section = main_page()->requests_section();
  EXPECT_FALSE(requests_section->GetVisible());

  // Add a site access request for extension A on the (active) first tab.
  // Verify extension A site access request is visible on the menu.
  auto* permissions_manager = PermissionsManager::Get(browser()->profile());
  permissions_manager->AddHostAccessRequest(tab1_web_contents, tab1_id,
                                            *extensionA);
  EXPECT_TRUE(requests_section->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extensionA->id()));

  // Add a site access request for extension B on the (inactive) second tab.
  // Verify only extension A site access request is visible on the menu.
  permissions_manager->AddHostAccessRequest(tab2_web_contents, tab2_id,
                                            *extensionB);
  EXPECT_TRUE(requests_section->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extensionA->id()));

  // Add a site access request for extension A on the (inactive) second tab.
  // Verify only extension A site access request is visible on the menu.
  permissions_manager->AddHostAccessRequest(tab2_web_contents, tab2_id,
                                            *extensionA);
  EXPECT_TRUE(requests_section->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extensionA->id()));

  // Remove the site access request for extension A on the (inactive) second
  // tab. Verify extension A site access request is still visible on the menu,
  // since request is still active for the first tab.
  permissions_manager->RemoveHostAccessRequest(tab2_id, extensionA->id());
  EXPECT_TRUE(requests_section->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extensionA->id()));
}

class ExtensionsMenuMainPageViewInteractiveTest
    : public InteractiveBrowserTestT<extensions::ExtensionBrowserTest> {
 public:
  ExtensionsMenuMainPageViewInteractiveTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsMenuAccessControl);
  }
  ExtensionsMenuMainPageViewInteractiveTest(
      const ExtensionsMenuMainPageViewInteractiveTest&) = delete;
  ExtensionsMenuMainPageViewInteractiveTest& operator=(
      const ExtensionsMenuMainPageViewInteractiveTest&) = delete;

  // Installs extension with `name` and `host_permission`.
  scoped_refptr<const extensions::Extension>
  InstallExtensionWithHostPermissions(const std::string& name,
                                      const std::string& host_permission) {
    extensions::TestExtensionDir extension_dir;

    extension_dir.WriteManifest(base::StringPrintf(
        R"({
            "name": "%s",
            "manifest_version": 3,
            "host_permissions": ["%s"],
            "version": "0.1"
          })",
        name.c_str(), host_permission.c_str()));
    scoped_refptr<const extensions::Extension> extension =
        extensions::ChromeTestExtensionLoader(profile()).LoadExtension(
            extension_dir.UnpackedPath());
    return extension;
  }

  ExtensionsToolbarContainer* extensions_container() {
    return browser()->GetBrowserView().toolbar()->extensions_container();
  }

  content::WebContents* active_web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  extensions::ExtensionActionRunner* active_action_runner() {
    return extensions::ExtensionActionRunner::GetForWebContents(
        active_web_contents());
  }

  // Opens the extensions menu and waits it is visible.
  auto OpenExtensionsMenu() {
    return Steps(PressButton(kExtensionsMenuButtonElementId),
                 WaitForShow(kExtensionsMenuMainPageElementId));
  }

  // Opens the context menu for `extension_id` by pressing the context menu
  // button on `menu_item_element_id` corresponding to the given extension.
  auto OpenContextMenu(const extensions::ExtensionId& extension_id,
                       ui::ElementIdentifier menu_item_element_id) {
    constexpr char kExtensionContextMenuButton[] =
        "extension_context_menu_button";
    return Steps(
        // Open the extension's context menu from its menu item.
        NameDescendantViewByType<HoverButton>(menu_item_element_id,
                                              kExtensionContextMenuButton, 1u),
        PressButton(kExtensionContextMenuButton),

        // Verify context menu is opened.
        WaitForShow(extensions::ExtensionContextMenuModel::kHomePageMenuItem),
        CheckResult(
            [&]() {
              return GetMenuItemViewFor(extension_id)
                  ->IsContextMenuRunningForTesting();
            },
            true));
  }

  // Clicks on the menu item button corresponding to `extension`.
  auto PressExtensionMenuItemButton(const extensions::Extension& extension) {
    constexpr char kExtensionMenuItemActionButton[] =
        "extension_menu_item_action_button";

    return Steps(
        CheckView(kExtensionMenuItemViewElementId,
                  [&extension](ExtensionMenuItemView* menu_item) {
                    return menu_item->view_controller()->GetId() ==
                           extension.id();
                  }),
        NameDescendantViewByType<ExtensionsMenuButton>(
            kExtensionMenuItemViewElementId, kExtensionMenuItemActionButton),
        PressButton(kExtensionMenuItemActionButton));
  }

  // Selects the `page_access_command_id` in the extension's context menu opened
  // from `menu_item_element_id` corresponding to the given extension.
  auto SelectSiteAccessUsingContextMenu(
      const extensions::ExtensionId& extension_id,
      ui::ElementIdentifier menu_item_element_id,
      extensions::PermissionsManager::UserSiteAccess site_access,
      ui::test::StateIdentifier<PermissionsUpdatesObserver> state_identifier) {
    return Steps(
        OpenContextMenu(extension_id, menu_item_element_id),
        SelectMenuItem(
            extensions::ExtensionContextMenuModel::kPageAccessMenuItem),
        SelectMenuItem(GetSiteAccessCommandId(site_access)),
        WaitForState(state_identifier, site_access));
  }

  // Verifies whether the context menu for `extension_id` opened from
  // `context_menu_source` has `command_id` with `label_id`.
  auto CheckExtensionContextMenuEntryLabel(
      const extensions::ExtensionId& extension_id,
      extensions::ExtensionContextMenuModel::ContextMenuSource
          context_menu_source,
      int command_id,
      int label_id) {
    return CheckResult(
        [this, extension_id, context_menu_source,
         command_id]() -> std::u16string {
          auto* context_menu =
              static_cast<extensions::ExtensionContextMenuModel*>(
                  extensions_container()
                      ->GetActionForId(extension_id)
                      ->GetContextMenu(context_menu_source));
          std::optional<size_t> command_index =
              context_menu->GetIndexOfCommandId(command_id);
          return command_index.has_value()
                     ? context_menu->GetLabelAt(command_index.value())
                     : std::u16string();
        },
        l10n_util::GetStringUTF16(label_id));
  }

  // Verifies whether `extension_id` has its action popped out in the extensions
  // container.
  auto CheckPoppedOutAction(
      const std::optional<extensions::ExtensionId>& extension_id) {
    return CheckResult(
        [&]() { return extensions_container()->GetPoppedOutActionId(); },
        extension_id);
  }

  // Verifies whether `extension` wants to run on the current web contents given
  // `expected_result`.
  auto CheckActionWantsToRun(const extensions::Extension& extension,
                             bool expected_result) {
    return CheckResult(
        [&]() { return active_action_runner()->WantsToRun(&extension); },
        expected_result);
  }

  // Verifies whether `extension` has `expected_site_interaction` on the current
  // web contents.
  auto CheckSiteInteraction(const extensions::Extension& extension,
                            extensions::SitePermissionsHelper::SiteInteraction
                                expected_site_interaction) {
    return CheckResult(
        [&]() {
          return extensions::SitePermissionsHelper(profile())
              .GetSiteInteraction(extension, active_web_contents());
        },
        expected_site_interaction);
  }

  // Returns the menu item view for `extension_id` in the menu's main page, if
  // existent.
  ExtensionMenuItemView* GetMenuItemViewFor(
      const extensions::ExtensionId& extension_id) {
    ExtensionsMenuMainPageView* main_page =
        extensions_container()
            ->GetExtensionsMenuCoordinatorForTesting()
            ->GetControllerForTesting()
            ->GetMainPageViewForTesting();
    if (!main_page) {
      return nullptr;
    }

    std::vector<ExtensionMenuItemView*> menu_items = main_page->GetMenuItems();

    auto iter = std::ranges::find(menu_items, extension_id,
                                  [](ExtensionMenuItemView* view) {
                                    return view->view_controller()->GetId();
                                  });
    return (iter == menu_items.end()) ? nullptr : *iter;
  }

 protected:
  void SetUpOnMainThread() override {
    InteractiveBrowserTestT<
        extensions::ExtensionBrowserTest>::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that opening the extensions menu highlight the extension toolbar
// button.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       DISABLED_ExtensionsMenuButtonHighlight) {
  LoadExtension(test_data_dir_.AppendASCII("simple_with_icon"));

  RunTestSequence(
      OpenExtensionsMenu(),
      CheckResult(
          [this]() {
            return views::InkDrop::Get(
                       extensions_container()->GetExtensionsButton())
                ->GetInkDrop()
                ->GetTargetInkDropState();
          },
          views::InkDropState::ACTIVATED));
}

// Tests clicking on the 'manage extensions' button opens chrome://extensions.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       ManageExtensionsOpensExtensionsPpage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);
  LoadExtension(test_data_dir_.AppendASCII("simple_with_icon"));

  RunTestSequence(InstrumentTab(kTab), OpenExtensionsMenu(),
                  PressButton(kExtensionsMenuManageExtensionsElementId),
                  WaitForWebContentsReady(kTab, GURL("chrome://extensions")));
}

// Tests clicking on the 'context menu' button opens the extension's context
// menu.
// TODO(crbug.com/400536589): Re-enable this flaky test.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       DISABLED_ContextMenuButtonOpensContextMenu) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_icon"));

  RunTestSequence(
      InstrumentTab(kTab), OpenExtensionsMenu(),
      CheckResult(
          [&]() {
            return GetMenuItemViewFor(extension->id())
                ->IsContextMenuRunningForTesting();
          },
          false),

      // Open the extension's context menu.
      CheckView(kExtensionMenuItemViewElementId,
                [extension](ExtensionMenuItemView* menu_item) {
                  return menu_item->view_controller()->GetId() ==
                         extension->id();
                }),
      OpenContextMenu(extension->id(), kExtensionMenuItemViewElementId),

      // Verify context menu is opened.
      CheckResult(
          [&]() {
            return GetMenuItemViewFor(extension->id())
                ->IsContextMenuRunningForTesting();
          },
          true)

  );
}

// Tests triggering the extension's action closes the extensions menu, even when
// there is no extension action to pop out.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       TriggeringExtensionClosesMenu) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);
  constexpr char kExtensionMenuItemActionButton[] =
      "extension_menu_item_action_button";

  // This test should not use a popped-out action, as we want to make sure that
  // the menu closes on its own and not because a popup dialog replaces it.
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/trigger_actions/browser_action"));

  RunTestSequence(
      InstrumentTab(kTab),

      // Trigger the extension's action by clicking on its menu entry.
      OpenExtensionsMenu(),
      CheckView(kExtensionMenuItemViewElementId,
                [extension](ExtensionMenuItemView* menu_item) {
                  return menu_item->view_controller()->GetId() ==
                         extension->id();
                }),
      NameDescendantViewByType<ExtensionsMenuButton>(
          kExtensionMenuItemViewElementId, kExtensionMenuItemActionButton),
      PressButton(kExtensionMenuItemActionButton),

      // Verify extension menu is closed.
      WaitForHide(kExtensionsMenuMainPageElementId),
      CheckResult(
          [&]() { return extensions_container()->IsExtensionsMenuShowing(); },
          false),
      CheckPoppedOutAction(std::nullopt));
}

// Tests triggering the extension's action while the extensions menu is opened
// records the correct invocation source.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       InvocationSourceMetrics) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);
  constexpr char kMenuItemActionButton[] = "menu_item_action_button";

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("uitest/extension_with_action_and_command"));
  base::HistogramTester histogram_tester;

  RunTestSequence(
      InstrumentTab(kTab), Do([&]() {
        histogram_tester.ExpectTotalCount("Extensions.Toolbar.InvocationSource",
                                          0);
      }),

      // Trigger the extension's action by clicking on its menu entry.
      OpenExtensionsMenu(),
      CheckView(kExtensionMenuItemViewElementId,
                [extension](ExtensionMenuItemView* menu_item) {
                  return menu_item->view_controller()->GetId() ==
                         extension->id();
                }),
      NameDescendantViewByType<ExtensionsMenuButton>(
          kExtensionMenuItemViewElementId, kMenuItemActionButton),
      PressButton(kMenuItemActionButton),

      Do([&]() {
        histogram_tester.ExpectTotalCount("Extensions.Toolbar.InvocationSource",
                                          1);
        histogram_tester.ExpectBucketCount(
            "Extensions.Toolbar.InvocationSource",
            ToolbarActionViewController::InvocationSource::kMenuEntry, 1);
      }));

  // TODO(crbug.com/40684492): Add a test for command invocation once
  // triggering an action via command with extensions menu opened is
  // fixed.
}

// Tests that clicking on the extension menu item for an extension with a popup
// pops out its action on the toolbar and loads the popup, and when the popup
// is dismissed the popup is closed and the action pops in on the toolbar.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       TriggerExtensionPopup) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ExtensionHostObserver,
                                      kExtensionHostState);

  constexpr char kExtensionMenuItemActionButton[] =
      "extension_menu_item_action_button";
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_popup"));

  RunTestSequence(
      InstrumentTab(kTab), OpenExtensionsMenu(),

      // Trigger the extension's action by clicking on its menu
      // entry.
      CheckView(kExtensionMenuItemViewElementId,
                [extension](ExtensionMenuItemView* menu_item) {
                  return menu_item->view_controller()->GetId() ==
                         extension->id();
                }),
      NameDescendantViewByType<ExtensionsMenuButton>(
          kExtensionMenuItemViewElementId, kExtensionMenuItemActionButton),
      ObserveState(kExtensionHostState,
                   extensions::ExtensionHostRegistry::Get(profile()),
                   extension->id()),
      PressButton(kExtensionMenuItemActionButton),

      // Verify extension's action is popped out, and the extension's popup is
      // loaded on the toolbar.
      WaitForShow(kToolbarActionViewElementId),
      CheckPoppedOutAction(extension->id()),
      WaitForState(kExtensionHostState, ExtensionHostState::kLoaded),

      // Hide the extension's popup.
      Do([this]() { extensions_container()->HideActivePopup(); }),

      // Verify the extension's popup is destroyed, and the extension's action
      // is hidden on the toolbar.
      WaitForState(kExtensionHostState, ExtensionHostState::kDestroyed),
      WaitForHide(kToolbarActionViewElementId),
      CheckPoppedOutAction(std::nullopt));
}

// Tests that removing an extension while it's action is showing a popup removes
// the action from the toolbar.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       RemoveExtensionShowingPopup) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);
  constexpr char kExtensionMenuItemActionButton[] =
      "extension_menu_item_action_button";

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_popup"));

  RunTestSequence(
      InstrumentTab(kTab), OpenExtensionsMenu(),

      // Trigger the extension's action by clicking on its menu entry.
      CheckView(kExtensionMenuItemViewElementId,
                [extension](ExtensionMenuItemView* menu_item) {
                  return menu_item->view_controller()->GetId() ==
                         extension->id();
                }),
      NameDescendantViewByType<ExtensionsMenuButton>(
          kExtensionMenuItemViewElementId, kExtensionMenuItemActionButton),
      PressButton(kExtensionMenuItemActionButton),

      // Verify extension's action is popped out.
      WaitForShow(kToolbarActionViewElementId).SetTransitionOnlyOnEvent(true),
      CheckPoppedOutAction(extension->id()),

      // Disable the extension.
      Do([&]() { DisableExtension(extension->id()); }),

      // Verify extension's action is not popped out.
      WaitForHide(kToolbarActionViewElementId).SetTransitionOnlyOnEvent(true),
      CheckPoppedOutAction(std::nullopt));
}

// Tests that removing multiple extensions while one of the extension's action
// is showing a popup removes such action from the toolbar.
// Test for crbug.com/1099456.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       RemoveMultipleExtensionsWhileShowingPopup) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);
  constexpr char kExtensionMenuItemActionButton[] =
      "extension_menu_item_action_button";

  const extensions::Extension* extension_A =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_popup"));
  const extensions::Extension* extension_B =
      LoadExtension(test_data_dir_.AppendASCII("uitest/window_open"));

  RunTestSequence(
      InstrumentTab(kTab), OpenExtensionsMenu(),

      // Trigger the extension A action by clicking on its menu entry. Entries
      // are in alphabetical order, therefore the first
      // kExtensionMenuItemViewElementId match should be extension A.
      CheckView(kExtensionMenuItemViewElementId,
                [extension_A](ExtensionMenuItemView* menu_item) {
                  return menu_item->view_controller()->GetId() ==
                         extension_A->id();
                }),
      NameDescendantViewByType<ExtensionsMenuButton>(
          kExtensionMenuItemViewElementId, kExtensionMenuItemActionButton),
      PressButton(kExtensionMenuItemActionButton),

      // Verify extension A action is popped out.
      WaitForShow(kToolbarActionViewElementId).SetTransitionOnlyOnEvent(true),
      CheckPoppedOutAction(extension_A->id()),

      // Disable both extensions.
      Do([&]() {
        DisableExtension(extension_A->id());
        DisableExtension(extension_B->id());
      }),

      // Verify extension A action is not popped out.
      WaitForHide(kToolbarActionViewElementId).SetTransitionOnlyOnEvent(true),
      CheckPoppedOutAction(std::nullopt));
}

// Tests that right clicking on an extension's popped-out action in the toolbar
// keeps the action popped out and opens the extension's context menu.
// TODO(crbug.com/384759463): Disabled on mac because context menus on Mac take
// over the main message loop, which means the right-click mouse event release
// is sent asynchronously causing the test to be flaky.
#if BUILDFLAG(IS_MAC)
#define MAYBE_RightClickOnPoppedOutExtension \
  DISABLED_RightClickOnPoppedOutExtension
#else
#define MAYBE_RightClickOnPoppedOutExtension RightClickOnPoppedOutExtension
#endif
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       MAYBE_RightClickOnPoppedOutExtension) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ExtensionHostObserver,
                                      kExtensionHostState);
  constexpr char kExtensionMenuItemActionButton[] =
      "extension_menu_item_action_button";

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_popup"));

  RunTestSequence(
      InstrumentTab(kTab), OpenExtensionsMenu(),

      // Trigger the extension's action by clicking on its menu entry.
      CheckView(kExtensionMenuItemViewElementId,
                [extension](ExtensionMenuItemView* menu_item) {
                  return menu_item->view_controller()->GetId() ==
                         extension->id();
                }),
      NameDescendantViewByType<ExtensionsMenuButton>(
          kExtensionMenuItemViewElementId, kExtensionMenuItemActionButton),
      ObserveState(kExtensionHostState,
                   extensions::ExtensionHostRegistry::Get(profile()),
                   extension->id()),
      PressButton(kExtensionMenuItemActionButton),

      // Verify extension's action is popped out and its popup is loaded.
      WaitForShow(kToolbarActionViewElementId).SetTransitionOnlyOnEvent(true),
      CheckPoppedOutAction(extension->id()),
      WaitForState(kExtensionHostState, ExtensionHostState::kLoaded),

      // Right click on the extension's action to open the context menu.
      MoveMouseTo(kToolbarActionViewElementId), ClickMouse(ui_controls::RIGHT),

      // Verify extension's popup is destroyed, its context menu is visible and
      // its action is still popped out.
      WaitForState(kExtensionHostState, ExtensionHostState::kDestroyed),
      WaitForShow(extensions::ExtensionContextMenuModel::kHomePageMenuItem),
      CheckResult(
          [&]() {
            return extensions_container()
                ->GetExtensionWithOpenContextMenuForTesting();
          },
          extension->id()),
      EnsurePresent(kToolbarActionViewElementId),
      // Note: This is misleading. The action is no longer labeled as "popped
      // out" in the extensions container when it's showing the context menu
      // instead of the popup, even though the action is popped out and visible.
      // It's not a bug because the action visibility also looks whether the
      // context menu is opened. However, we may look into updating the logic so
      // it's more correct.
      CheckPoppedOutAction(std::nullopt));
}

// Test that an extension's context menu shows the correct label when the
// extension is pinned.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       PinnedExtensionShowsCorrectContextMenuPinOption) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_popup"));

  RunTestSequence(
      InstrumentTab(kTab), OpenExtensionsMenu(),
      // Open the extension's context menu from the extensions menu.
      CheckView(kExtensionMenuItemViewElementId,
                [extension](ExtensionMenuItemView* menu_item) {
                  return menu_item->view_controller()->GetId() ==
                         extension->id();
                }),
      OpenContextMenu(extension->id(), kExtensionMenuItemViewElementId),

      // Verify the toggle visibility entry has "pin to toolbar" label and
      // select it.
      CheckExtensionContextMenuEntryLabel(
          extension->id(),
          extensions::ExtensionContextMenuModel::ContextMenuSource::kMenuItem,
          extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY,
          IDS_EXTENSIONS_CONTEXT_MENU_PIN_TO_TOOLBAR),
      SelectMenuItem(
          extensions::ExtensionContextMenuModel::kToggleVisibilityMenuItem),

      // Verify extension is pinned but not stored as the popped out action.
      WaitForShow(kToolbarActionViewElementId)
          .SetTransitionOnlyOnEvent(/*transition_only_on_event=*/true),
      CheckPoppedOutAction(std::nullopt),

      // Verify the toggle visibility entry is "unpin from toolbar" label when
      // context menu is opened from the toolbar action or the extensions menu.
      CheckExtensionContextMenuEntryLabel(
          extension->id(),
          extensions::ExtensionContextMenuModel::ContextMenuSource::kMenuItem,
          extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY,
          IDS_EXTENSIONS_CONTEXT_MENU_UNPIN_FROM_TOOLBAR),
      CheckExtensionContextMenuEntryLabel(
          extension->id(),
          extensions::ExtensionContextMenuModel::ContextMenuSource::
              kToolbarAction,
          extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY,
          IDS_EXTENSIONS_CONTEXT_MENU_UNPIN_FROM_TOOLBAR)

  );
}

// Test that an extension's context menu shows the correct label when the
// extension is unpinned and popped out.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       UnpinnedExtensionShowsCorrectContextMenuPinOption) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);
  constexpr char kExtensionMenuItemActionButton[] =
      "extension_menu_item_action_button";

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_popup"));

  RunTestSequence(
      InstrumentTab(kTab), OpenExtensionsMenu(),

      // Trigger the extension's action by clicking on its menu entry.
      CheckView(kExtensionMenuItemViewElementId,
                [extension](ExtensionMenuItemView* menu_item) {
                  return menu_item->view_controller()->GetId() ==
                         extension->id();
                }),
      NameDescendantViewByType<ExtensionsMenuButton>(
          kExtensionMenuItemViewElementId, kExtensionMenuItemActionButton),
      PressButton(kExtensionMenuItemActionButton),

      // Verify extension appears on the toolbar and is stored as the popped out
      // action.
      WaitForShow(kToolbarActionViewElementId)
          .SetTransitionOnlyOnEvent(/*transition_only_on_event=*/true),
      CheckPoppedOutAction(extension->id()),

      // Verify the toggle visibility entry when opened from the toolbar is to
      // pin the extension, since the extension is not pinned (just popped out).
      CheckExtensionContextMenuEntryLabel(
          extension->id(),
          extensions::ExtensionContextMenuModel::ContextMenuSource::kMenuItem,
          extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY,
          IDS_EXTENSIONS_CONTEXT_MENU_PIN_TO_TOOLBAR),

      // TODO(crbug.com/378724154): Test crashes if popup is left opened at the
      // end of the test. For now, close the popup so don't lose test coverage.
      Do([&]() { extensions_container()->HideActivePopup(); }));
}

IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       PinningDisabledInIncognito) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);
  Browser* const incognito_browser = CreateIncognitoBrowser(profile());
  ui_test_utils::BrowserActivationWaiter(incognito_browser).WaitForActivation();

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("uitest/window_open"),
                    {.allow_in_incognito = true});

  RunTestSequence(InContext(
      incognito_browser->window()->GetElementContext(),
      Steps(InstrumentTab(kTab), OpenExtensionsMenu(),
            // Verify toggle visibility entry in context menu is disabled.
            CheckResult(
                [&]() {
                  auto* context_menu =
                      static_cast<extensions::ExtensionContextMenuModel*>(
                          incognito_browser->GetBrowserView()
                              .toolbar()
                              ->extensions_container()
                              ->GetActionForId(extension->id())
                              ->GetContextMenu(
                                  extensions::ExtensionContextMenuModel::
                                      ContextMenuSource::kMenuItem));
                  return context_menu->IsCommandIdEnabled(
                      extensions::ExtensionContextMenuModel::TOGGLE_VISIBILITY);
                },
                false))));
}

// Tests that triggering the reload page dialog from the extension menu (by
// revoking site access for an extension) closes the extension menu and pops out
// the extension action on the toolbar.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       ReloadPageDialog) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);
  constexpr char kExtensionMenuItemToggle[] = "extension_menu_item_toggle";

  // Load an extension that injects a script.
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(
      R"({
           "name": "Extension",
           "manifest_version": 3,
           "version": "0.1",
           "content_scripts": [{
             "matches": ["*://*/*"],
             "js": ["script.js"]
           }]
         })");
  extension_dir.WriteFile(FILE_PATH_LITERAL("script.js"),
                          "console.log('injected!');");
  scoped_refptr<const extensions::Extension> extension =
      extensions::ChromeTestExtensionLoader(browser()->profile())
          .LoadExtension(extension_dir.UnpackedPath());

  RunTestSequence(
      // Navigate to a site where the extension's script will be injected.
      InstrumentTab(kTab),
      NavigateWebContents(
          kTab, embedded_test_server()->GetURL("example.com", "/title1.html")),

      // Verify extension is not visible on the toolbar.
      EnsureNotPresent(kToolbarActionViewElementId),

      OpenExtensionsMenu(),

      // Revoke site access for the extension by toggling the extension off.
      CheckView(kExtensionMenuItemViewElementId,
                [extension](ExtensionMenuItemView* menu_item) {
                  return menu_item->view_controller()->GetId() ==
                         extension->id();
                }),
      NameDescendantViewByType<views::ToggleButton>(
          kExtensionMenuItemViewElementId, kExtensionMenuItemToggle),
      PressButton(kExtensionMenuItemToggle),

      // Verify this causes the extension menu to close, extension's action to
      // pop out on the toolbar and the reload page dialog to appear.
      WaitForHide(kExtensionsMenuMainPageElementId),
      WaitForShow(kToolbarActionViewElementId),
      // Note: We cannot add an element identifier to the dialog when it's built
      // using DialogModel::Builder. Thus, we check for its existence by
      // checking the visibility of one of its elements.
      WaitForShow(extensions::kReloadPageDialogOkButtonElementId));
}

// Tests that accepting the reload page dialog when clicking on an action runs
// the script/blocked actions.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       ExecuteAction_AcceptReload) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);

  // Load an extension that injects a script, and withhold its permissions.
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("blocked_actions/content_scripts"));
  extensions::ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  RunTestSequence(
      InstrumentTab(kTab),
      NavigateWebContents(kTab, embedded_test_server()->GetURL("/simple.html")),

      // Verify extension wants to run in the current site.
      CheckActionWantsToRun(*extension, true),

      // Trigger the extension's action by clicking on its menu entry.
      OpenExtensionsMenu(), PressExtensionMenuItemButton(*extension),

      // Accept the reload dialog.
      WaitForShow(extensions::kReloadPageDialogOkButtonElementId),
      PressButton(extensions::kReloadPageDialogOkButtonElementId),
      WaitForWebContentsNavigation(kTab),

      // The extension permission should have been applied at this point, and
      // the extension's script and blocked actions should have run since there
      // was a page reload.
      CheckResult(
          [&]() {
            return extensions::browsertest_util::DidChangeTitle(
                *active_web_contents(), /*original_title=*/u"OK",
                /*changed_title=*/u"success");
          },
          true),
      CheckActionWantsToRun(*extension, false),
      CheckSiteInteraction(
          *extension,
          extensions::SitePermissionsHelper::SiteInteraction::kGranted)

  );
}

// Tests that canceling the reload page dialog when clicking on an action
// doesn't run the script/blocked actions until there is a manual reload.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       ExecuteAction_CancelReload) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);

  // Load an extension that injects a script, and withhold its permissions.
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("blocked_actions/content_scripts"));
  extensions::ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  RunTestSequence(
      InstrumentTab(kTab),
      NavigateWebContents(kTab, embedded_test_server()->GetURL("/simple.html")),

      // Verify extension wants access to the current site.
      CheckResult(
          [&]() { return active_action_runner()->WantsToRun(extension); },
          true),

      // Trigger the extension's action by clicking on its menu entry.
      OpenExtensionsMenu(), PressExtensionMenuItemButton(*extension),

      // Accept the reload dialog.
      WaitForShow(extensions::kReloadPageDialogCancelButtonElementId),
      PressButton(extensions::kReloadPageDialogCancelButtonElementId),

      // The extension permission should have been applied at this point, but
      // the extension's script and blocked actions should not run since a
      // reload is needed.
      CheckResult(
          [&]() {
            return extensions::browsertest_util::DidChangeTitle(
                *active_web_contents(), /*original_title=*/u"OK",
                /*changed_title=*/u"success");
          },
          false),
      CheckActionWantsToRun(*extension, true),
      CheckSiteInteraction(
          *extension,
          extensions::SitePermissionsHelper::SiteInteraction::kGranted),

      // Refresh the page manually.
      Do([&]() {
        chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
      }),
      WaitForWebContentsNavigation(kTab),

      // Extension's script and blocked action should have run.
      CheckResult(
          [&]() {
            return extensions::browsertest_util::DidChangeTitle(
                *active_web_contents(), /*original_title=*/u"OK",
                /*changed_title=*/u"success");
          },
          true),
      CheckActionWantsToRun(*extension, false));
}

// Tests that the extension entry in the extensions menu gets updated after
// site access changes.
// TODO(crbug.com/384759463): Disabled on mac because context menus on Mac take
// over the main message loop, which causes the test to be flaky when waiting
// for other events.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MenuGetsUpdatedAfterSiteAccessChanges \
  DISABLED_MenuGetsUpdatedAfterSiteAccessChanges
#else
#define MAYBE_MenuGetsUpdatedAfterSiteAccessChanges \
  MenuGetsUpdatedAfterSiteAccessChanges
#endif
IN_PROC_BROWSER_TEST_F(ExtensionsMenuMainPageViewInteractiveTest,
                       MAYBE_MenuGetsUpdatedAfterSiteAccessChanges) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(PermissionsUpdatesObserver,
                                      kPermissionsUpdates);
  constexpr char kExtensionSitePermissionsButton[] =
      "extension_site_permissions_button";

  auto extension =
      InstallExtensionWithHostPermissions("All Urls Extension", "<all_urls>");

  RunTestSequence(
      InstrumentTab(kTab),
      NavigateWebContents(
          kTab, embedded_test_server()->GetURL("example.com", "/title1.html")),
      // Automatically accept the reload page dialog that appears when
      // changing site access.
      Do([&]() {
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetActiveWebContents();
        extensions::ExtensionActionRunner::GetForWebContents(web_contents)
            ->accept_bubble_for_testing(true);
      }),

      OpenExtensionsMenu(),
      CheckView(
          kExtensionMenuItemViewElementId,
          [extension](ExtensionMenuItemView* menu_item) {
            return menu_item->view_controller()->GetId();
          },
          extension->id()),
      NameDescendantViewByType<HoverButton>(kExtensionMenuItemViewElementId,
                                            kExtensionSitePermissionsButton,
                                            /*index=*/2u),

      // Add observer for permissions updates.
      ObserveState(
          kPermissionsUpdates, extensions::PermissionsManager::Get(profile()),
          extension->id(),
          embedded_test_server()->GetURL("example.com", "/title1.html")),

      // Verify extension has "on all sites" site permissions label.
      CheckView(
          kExtensionSitePermissionsButton,
          [](HoverButton* site_permissions_button) {
            return site_permissions_button->GetText();
          },
          u"Always on all sites"),

      // Change site access to run "on site" using the context menu.
      SelectSiteAccessUsingContextMenu(
          extension->id(), kExtensionMenuItemViewElementId,
          extensions::PermissionsManager::UserSiteAccess::kOnSite,
          kPermissionsUpdates),

      // Verify extension has "on site" site permissions label.
      CheckView(
          kExtensionSitePermissionsButton,
          [](HoverButton* site_permissions_button) {
            return site_permissions_button->GetText();
          },
          u"Always on this site"),

      // Change extension's site access to run "on click" using the context
      // menu.
      SelectSiteAccessUsingContextMenu(
          extension->id(), kExtensionMenuItemViewElementId,
          extensions::PermissionsManager::UserSiteAccess::kOnClick,
          kPermissionsUpdates),

      // Verify extension has "on click" site permissions label.
      CheckView(
          kExtensionSitePermissionsButton,
          [](HoverButton* site_permissions_button) {
            return site_permissions_button->GetText();
          },
          u"Ask on every visit"));
}
