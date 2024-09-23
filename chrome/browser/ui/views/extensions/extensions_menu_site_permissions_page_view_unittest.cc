// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include "base/feature_list.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_site_permissions_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/controls/button/toggle_button.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

}  // namespace

class ExtensionsSitePermissionsPageViewUnitTest
    : public ExtensionsToolbarUnitTest {
 public:
  ExtensionsSitePermissionsPageViewUnitTest();
  ~ExtensionsSitePermissionsPageViewUnitTest() override = default;
  ExtensionsSitePermissionsPageViewUnitTest(
      const ExtensionsSitePermissionsPageViewUnitTest&) = delete;
  ExtensionsSitePermissionsPageViewUnitTest& operator=(
      const ExtensionsSitePermissionsPageViewUnitTest&) = delete;

  // Opens menu and navigates to site permissions page for `extension_id`. This
  // will CHECK if extension cannot have a site permissions page (e.g
  // restricted site).
  void ShowSitePermissionsPage(extensions::ExtensionId extension_id);

  // Returns whether me menu has the main page opened.
  bool IsMainPageOpened();

  // Returns whether the menu has the `extension_id` site permissions page
  // opened.
  bool IsSitePermissionsPageOpened(extensions::ExtensionId extension_id);

  // Returns the extensions that are showing site access requests in the
  // toolbar.
  std::vector<extensions::ExtensionId> GetExtensionsShowingRequests();

  // Navigates to `string_url`.
  void NavigateAndCommit(const std::string& string_url);

  // Since this is a unittest, the extensions menu widget sometimes needs a
  // nudge to re-layout the views.
  void LayoutMenuIfNecessary();

  ExtensionsMenuMainPageView* main_page();
  ExtensionsMenuSitePermissionsPageView* site_permissions_page();

  // ExtensionsToolbarUnitTest:
  void SetUp() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<content::WebContentsTester, DanglingUntriaged> web_contents_tester_;
};

ExtensionsSitePermissionsPageViewUnitTest::
    ExtensionsSitePermissionsPageViewUnitTest() {
  scoped_feature_list_.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);
}

void ExtensionsSitePermissionsPageViewUnitTest::ShowSitePermissionsPage(
    extensions::ExtensionId extension_id) {
  menu_coordinator()->Show(extensions_button(), extensions_container());
  menu_coordinator()->GetControllerForTesting()->OpenSitePermissionsPage(
      extension_id);
}

bool ExtensionsSitePermissionsPageViewUnitTest::IsMainPageOpened() {
  ExtensionsMenuMainPageView* page = main_page();
  return !!page;
}

bool ExtensionsSitePermissionsPageViewUnitTest::IsSitePermissionsPageOpened(
    extensions::ExtensionId extension_id) {
  ExtensionsMenuSitePermissionsPageView* page = site_permissions_page();
  return page && page->extension_id() == extension_id;
}

std::vector<extensions::ExtensionId>
ExtensionsSitePermissionsPageViewUnitTest::GetExtensionsShowingRequests() {
  return extensions_container()
      ->GetRequestAccessButton()
      ->GetExtensionIdsForTesting();
}

void ExtensionsSitePermissionsPageViewUnitTest::NavigateAndCommit(
    const std::string& string_url) {
  const GURL url(string_url);
  web_contents_tester_->NavigateAndCommit(url);
  WaitForAnimation();
}

void ExtensionsSitePermissionsPageViewUnitTest::LayoutMenuIfNecessary() {
  menu_coordinator()->GetExtensionsMenuWidget()->LayoutRootViewIfNecessary();
}

ExtensionsMenuMainPageView*
ExtensionsSitePermissionsPageViewUnitTest::main_page() {
  ExtensionsMenuViewController* menu_controller =
      menu_coordinator()->GetControllerForTesting();
  return menu_controller ? menu_controller->GetMainPageViewForTesting()
                         : nullptr;
}

ExtensionsMenuSitePermissionsPageView*
ExtensionsSitePermissionsPageViewUnitTest::site_permissions_page() {
  ExtensionsMenuViewController* menu_controller =
      menu_coordinator()->GetControllerForTesting();
  return menu_controller ? menu_controller->GetSitePermissionsPageForTesting()
                         : nullptr;
}

void ExtensionsSitePermissionsPageViewUnitTest::SetUp() {
  ExtensionsToolbarUnitTest::SetUp();
  // Menu needs web contents at construction, so we need to add them to every
  // test.
  web_contents_tester_ = AddWebContentsAndGetTester();
}

TEST_F(ExtensionsSitePermissionsPageViewUnitTest,
       AddAndRemoveExtensionWhenSitePermissionsPageIsOpen) {
  auto extensionA =
      InstallExtensionWithHostPermissions("A Extension", {"<all_urls>"});

  NavigateAndCommit("http://www.url.com");
  ShowSitePermissionsPage(extensionA->id());

  // Verify site permissions page is open for extension A.
  EXPECT_TRUE(IsSitePermissionsPageOpened(extensionA->id()));

  // Adding a new extension doesn't affect the opened site permissions page for
  // extension A.
  auto extensionB =
      InstallExtensionWithHostPermissions("B Extension", {"<all_urls>"});
  EXPECT_TRUE(IsSitePermissionsPageOpened(extensionA->id()));

  // Removing extension B doesn't affect the opened site permissions page for
  // extension A.
  UninstallExtension(extensionB->id());
  EXPECT_TRUE(IsSitePermissionsPageOpened(extensionA->id()));

  // Removing extension A closes its open site permissions page and menu
  // navigates back to the main page.
  UninstallExtension(extensionA->id());
  EXPECT_FALSE(IsSitePermissionsPageOpened(extensionA->id()));
  EXPECT_TRUE(IsMainPageOpened());
}

// Tests that menu navigates back to the main page when an extension, whose site
// permissions page is open, is disabled.
TEST_F(ExtensionsSitePermissionsPageViewUnitTest, DisableAndEnableExtension) {
  auto extension =
      InstallExtensionWithHostPermissions("Test Extension", {"<all_urls>"});

  NavigateAndCommit("http://www.url.com");
  ShowSitePermissionsPage(extension->id());
  EXPECT_TRUE(IsSitePermissionsPageOpened(extension->id()));

  DisableExtension(extension->id());
  LayoutMenuIfNecessary();
  WaitForAnimation();

  EXPECT_FALSE(IsSitePermissionsPageOpened(extension->id()));
  EXPECT_TRUE(IsMainPageOpened());
}

// Tests that menu navigates back to the main page when an extension, whose site
// permissions page is open, is reloaded.
TEST_F(ExtensionsSitePermissionsPageViewUnitTest, ReloadExtension) {
  // The extension must have a manifest to be reloaded.
  extensions::TestExtensionDir extension_directory;
  constexpr char kManifest[] = R"({
        "name": "Test Extension",
        "version": "1",
        "manifest_version": 3,
        "host_permissions": [
          "<all_urls>"
        ]
      })";
  extension_directory.WriteManifest(kManifest);
  extensions::ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_directory.UnpackedPath());

  NavigateAndCommit("http://www.url.com");
  ShowSitePermissionsPage(extension->id());
  EXPECT_TRUE(IsSitePermissionsPageOpened(extension->id()));

  // Reload the extension.
  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile()));
  ReloadExtension(extension->id());
  ASSERT_TRUE(registry_observer.WaitForExtensionLoaded());
  LayoutMenuIfNecessary();

  EXPECT_FALSE(IsSitePermissionsPageOpened(extension->id()));
  EXPECT_TRUE(IsMainPageOpened());
}

// Tests that toggling the show requests button changes whether an extension can
// show site access requests in the toolbar, and the UI is properly updated.
TEST_F(ExtensionsSitePermissionsPageViewUnitTest, ShowRequestsTogglePressed) {
  auto extensionA =
      InstallExtensionWithHostPermissions("Extension A", {"<all_urls>"});
  auto extensionB =
      InstallExtensionWithHostPermissions("Extension B", {"<all_urls>"});
  WithholdHostPermissions(extensionA.get());
  WithholdHostPermissions(extensionB.get());

  NavigateAndCommit("http://www.url.com");
  ShowSitePermissionsPage(extensionA->id());
  EXPECT_TRUE(IsSitePermissionsPageOpened(extensionA->id()));

  // By default, extensions are allowed to show request access in the toolbar.
  // However, request is only shown if extension adds a request for the site.
  EXPECT_TRUE(
      site_permissions_page()->GetShowRequestsToggleForTesting()->GetIsOn());
  EXPECT_THAT(GetExtensionsShowingRequests(), testing::IsEmpty());

  // Add site access requests for both extensions.
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  AddSiteAccessRequest(*extensionA, web_contents);
  AddSiteAccessRequest(*extensionB, web_contents);

  // Both extensions should have a visible request in the toolbar.
  EXPECT_THAT(GetExtensionsShowingRequests(),
              testing::ElementsAre(extensionA->id(), extensionB->id()));

  // Toggle off the show requests button for extension A and verify it is not
  // requesting access in the toolbar.
  ClickButton(site_permissions_page()->GetShowRequestsToggleForTesting());
  WaitForAnimation();
  EXPECT_THAT(GetExtensionsShowingRequests(),
              testing::ElementsAre(extensionB->id()));

  // Toggle on the shows requests button for extension A and verify it is
  // requesting access in the toolbar.
  ClickButton(site_permissions_page()->GetShowRequestsToggleForTesting());
  WaitForAnimation();
  EXPECT_THAT(GetExtensionsShowingRequests(),
              testing::ElementsAre(extensionA->id(), extensionB->id()));
}

// Tests that the UI is properly updated when an extension pref for showing site
// access requests in the toolbar changes while the menu is open.
TEST_F(ExtensionsSitePermissionsPageViewUnitTest,
       ShowRequestsPrefChangedWithMenuOpen) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  NavigateAndCommit("http://www.url.com");
  ShowSitePermissionsPage(extension->id());
  EXPECT_TRUE(IsSitePermissionsPageOpened(extension->id()));

  // Add site access request for extension.
  AddSiteAccessRequest(*extension,
                       browser()->tab_strip_model()->GetActiveWebContents());

  // By default, extensions are allowed to show request access in the toolbar.
  EXPECT_TRUE(
      site_permissions_page()->GetShowRequestsToggleForTesting()->GetIsOn());
  EXPECT_THAT(GetExtensionsShowingRequests(),
              testing::ElementsAre(extension->id()));

  // Directly change the show access requests pref for extension, since it can
  // be changed when menu is open, and verify toggle is updated and extension is
  // not requesting access in the toolbar.
  SitePermissionsHelper(browser()->profile())
      .SetShowAccessRequestsInToolbar(extension->id(), false);
  EXPECT_FALSE(
      site_permissions_page()->GetShowRequestsToggleForTesting()->GetIsOn());
  EXPECT_THAT(GetExtensionsShowingRequests(), testing::IsEmpty());
}

// Tests that selecting a site acces option in the menu updates the extension
// site access.
TEST_F(ExtensionsSitePermissionsPageViewUnitTest, SiteAccessUpdated) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  NavigateAndCommit("http://www.url.com");

  ShowSitePermissionsPage(extension->id());
  EXPECT_TRUE(IsSitePermissionsPageOpened(extension->id()));

  auto* on_click_button =
      site_permissions_page()->GetSiteAccessButtonForTesting(
          PermissionsManager::UserSiteAccess::kOnClick);
  auto* on_site_button = site_permissions_page()->GetSiteAccessButtonForTesting(
      PermissionsManager::UserSiteAccess::kOnSite);
  auto* on_all_sites_button =
      site_permissions_page()->GetSiteAccessButtonForTesting(
          PermissionsManager::UserSiteAccess::kOnAllSites);

  // By default, an extension has site access to all sites.
  EXPECT_FALSE(on_click_button->GetChecked());
  EXPECT_FALSE(on_site_button->GetChecked());
  EXPECT_TRUE(on_all_sites_button->GetChecked());

  extensions::PermissionsManagerWaiter waiter(
      PermissionsManager::Get(browser()->profile()));
  ClickButton(on_click_button);
  waiter.WaitForExtensionPermissionsUpdate();

  // Selecting a different site access updates the extension site access, which
  // is reflected in the UI.
  EXPECT_TRUE(on_click_button->GetChecked());
  EXPECT_FALSE(on_site_button->GetChecked());
  EXPECT_FALSE(on_all_sites_button->GetChecked());
}

// Tests that the menu UI is properly updated when the extension's site access
// is changed to "on click" while the menu is open.
TEST_F(ExtensionsSitePermissionsPageViewUnitTest,
       SiteAccessUpdatedWithMenuOpen_OnClick) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  NavigateAndCommit("http://www.url.com");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ShowSitePermissionsPage(extension->id());
  EXPECT_TRUE(IsSitePermissionsPageOpened(extension->id()));

  UpdateUserSiteAccess(*extension, web_contents,
                       PermissionsManager::UserSiteAccess::kOnClick);

  auto* on_click_button =
      site_permissions_page()->GetSiteAccessButtonForTesting(
          PermissionsManager::UserSiteAccess::kOnClick);
  auto* on_site_button = site_permissions_page()->GetSiteAccessButtonForTesting(
      PermissionsManager::UserSiteAccess::kOnSite);
  auto* on_all_sites_button =
      site_permissions_page()->GetSiteAccessButtonForTesting(
          PermissionsManager::UserSiteAccess::kOnAllSites);
  EXPECT_TRUE(on_click_button->GetChecked());
  EXPECT_FALSE(on_site_button->GetChecked());
  EXPECT_FALSE(on_all_sites_button->GetChecked());
}

// Tests that the menu UI is properly updated when the extension's site access
// is changed to "on site" while the menu is open.
TEST_F(ExtensionsSitePermissionsPageViewUnitTest,
       SiteAccessUpdatedWithMenuOpen_OnSite) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  NavigateAndCommit("http://www.url.com");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ShowSitePermissionsPage(extension->id());
  EXPECT_TRUE(IsSitePermissionsPageOpened(extension->id()));

  UpdateUserSiteAccess(*extension, web_contents,
                       PermissionsManager::UserSiteAccess::kOnSite);

  auto* on_click_button =
      site_permissions_page()->GetSiteAccessButtonForTesting(
          PermissionsManager::UserSiteAccess::kOnClick);
  auto* on_site_button = site_permissions_page()->GetSiteAccessButtonForTesting(
      PermissionsManager::UserSiteAccess::kOnSite);
  auto* on_all_sites_button =
      site_permissions_page()->GetSiteAccessButtonForTesting(
          PermissionsManager::UserSiteAccess::kOnAllSites);
  EXPECT_FALSE(on_click_button->GetChecked());
  EXPECT_TRUE(on_site_button->GetChecked());
  EXPECT_FALSE(on_all_sites_button->GetChecked());
}

// Tests that the menu UI is properly updated when the extension's site access
// is changed to "on all sites" while the menu is open.
TEST_F(ExtensionsSitePermissionsPageViewUnitTest,
       SiteAccessUpdatedWithMenuOpen_OnAllSites) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  // Withhold the extension host permissions since they are granted by default,
  // so we can test changing site access to "on all sites".
  extensions::PermissionsManagerWaiter waiter(
      PermissionsManager::Get(profile()));
  extensions::ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);
  waiter.WaitForExtensionPermissionsUpdate();

  NavigateAndCommit("http://www.url.com");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ShowSitePermissionsPage(extension->id());
  EXPECT_TRUE(IsSitePermissionsPageOpened(extension->id()));

  UpdateUserSiteAccess(*extension, web_contents,
                       PermissionsManager::UserSiteAccess::kOnAllSites);

  auto* on_click_button =
      site_permissions_page()->GetSiteAccessButtonForTesting(
          PermissionsManager::UserSiteAccess::kOnClick);
  auto* on_site_button = site_permissions_page()->GetSiteAccessButtonForTesting(
      PermissionsManager::UserSiteAccess::kOnSite);
  auto* on_all_sites_button =
      site_permissions_page()->GetSiteAccessButtonForTesting(
          PermissionsManager::UserSiteAccess::kOnAllSites);
  EXPECT_FALSE(on_click_button->GetChecked());
  EXPECT_FALSE(on_site_button->GetChecked());
  EXPECT_TRUE(on_all_sites_button->GetChecked());
}

// Test that navigating to a new site where the user doesn't have runtime host
// permissions controls (e.g restricted site) closes the site permissions page.
TEST_F(ExtensionsSitePermissionsPageViewUnitTest,
       PageNavigationWithMenuOpen_UserLosesRuntimeHostPermissionsControls) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  NavigateAndCommit("http://www.non-restricted.com");

  ShowSitePermissionsPage(extension->id());
  EXPECT_FALSE(IsMainPageOpened());
  EXPECT_TRUE(IsSitePermissionsPageOpened(extension->id()));

  // While the menu is open, navigate to an url where extension should not have
  // a site permissions page.
  NavigateAndCommit("chrome://extensions");

  // Menu should navigate back to main page since site permissions page should
  // not be visible for the new url.
  EXPECT_TRUE(IsMainPageOpened());
  EXPECT_FALSE(IsSitePermissionsPageOpened(extension->id()));
}

// Test that navigating to a new site where the user still has runtime host
// permissions controls updates the page contents.
TEST_F(ExtensionsSitePermissionsPageViewUnitTest,
       PageNavigationWithMenuOpen_UserMaintainsRuntimeHostPermissionsControls) {
  constexpr char kUrlA[] = "http://www.a.com";
  const GURL url_A(kUrlA);
  auto extension = InstallExtension("Extension", {"activeTab"}, {url_A.spec()});

  NavigateAndCommit(kUrlA);
  ShowSitePermissionsPage(extension->id());

  // Menu should be open in site permissions page because the extension has site
  // permissions.
  EXPECT_FALSE(IsMainPageOpened());
  EXPECT_TRUE(IsSitePermissionsPageOpened(extension->id()));

  auto* on_click_button =
      site_permissions_page()->GetSiteAccessButtonForTesting(
          PermissionsManager::UserSiteAccess::kOnClick);
  auto* on_site_button = site_permissions_page()->GetSiteAccessButtonForTesting(
      PermissionsManager::UserSiteAccess::kOnSite);
  auto* on_all_sites_button =
      site_permissions_page()->GetSiteAccessButtonForTesting(
          PermissionsManager::UserSiteAccess::kOnAllSites);

  // Extension requested access to url A, thus user can select "on site" or "on
  // click" access.
  EXPECT_TRUE(on_click_button->GetEnabled());
  EXPECT_TRUE(on_site_button->GetEnabled());
  EXPECT_FALSE(on_all_sites_button->GetEnabled());

  // While the menu is open, navigate to an url where the extension should
  // also have a site permissions page.
  NavigateAndCommit("http://www.b.com");

  // Menu should stay open in site permissions page for `extension`.
  EXPECT_FALSE(IsMainPageOpened());
  EXPECT_TRUE(IsSitePermissionsPageOpened(extension->id()));

  // Extension didn't request specific access to url B, but it has active tab
  // access. Thus, user can only select "on click" access.
  EXPECT_TRUE(on_click_button->GetEnabled());
  EXPECT_FALSE(on_site_button->GetEnabled());
  EXPECT_FALSE(on_all_sites_button->GetEnabled());
}
