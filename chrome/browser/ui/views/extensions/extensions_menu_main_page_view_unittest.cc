// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_site_permissions_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_utils.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;

// Returns the extension names from the given `menu_items`.
std::vector<std::string> GetNamesFromMenuItems(
    std::vector<ExtensionMenuItemView*> menu_items) {
  std::vector<std::string> names;
  names.resize(menu_items.size());
  base::ranges::transform(
      menu_items, names.begin(), [](ExtensionMenuItemView* item) {
        return base::UTF16ToUTF8(item->primary_action_button_for_testing()
                                     ->label_text_for_testing());
      });
  return names;
}

}  // namespace

class ExtensionsMenuMainPageViewUnitTest : public ExtensionsToolbarUnitTest {
 public:
  ExtensionsMenuMainPageViewUnitTest();
  ~ExtensionsMenuMainPageViewUnitTest() override = default;
  ExtensionsMenuMainPageViewUnitTest(
      const ExtensionsMenuMainPageViewUnitTest&) = delete;
  ExtensionsMenuMainPageViewUnitTest& operator=(
      const ExtensionsMenuMainPageViewUnitTest&) = delete;

  // Opens menu on "main page" by default.
  void ShowMenu();

  // Asserts there is exactly one menu item and then returns it.
  ExtensionMenuItemView* GetOnlyMenuItem();

  // Since this is a unittest, the extensions menu widget sometimes needs a
  // nudge to re-layout the views.
  void LayoutMenuIfNecessary();

  void ClickSitePermissionsButton(ExtensionMenuItemView* menu_item);

  content::WebContentsTester* web_contents_tester() {
    return web_contents_tester_;
  }

  ExtensionsMenuMainPageView* main_page();
  ExtensionsMenuSitePermissionsPageView* site_permissions_page();
  std::vector<ExtensionMenuItemView*> menu_items();

  // ExtensionsToolbarUnitTest:
  void SetUp() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<content::WebContentsTester> web_contents_tester_;
};

ExtensionsMenuMainPageViewUnitTest::ExtensionsMenuMainPageViewUnitTest() {
  scoped_feature_list_.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);
}

void ExtensionsMenuMainPageViewUnitTest::ShowMenu() {
  menu_coordinator()->Show(extensions_button(), extensions_container());
}

ExtensionMenuItemView* ExtensionsMenuMainPageViewUnitTest::GetOnlyMenuItem() {
  std::vector<ExtensionMenuItemView*> items = menu_items();
  if (items.size() != 1u) {
    ADD_FAILURE() << "Not exactly one item; size is: " << items.size();
    return nullptr;
  }
  return *items.begin();
}

void ExtensionsMenuMainPageViewUnitTest::LayoutMenuIfNecessary() {
  menu_coordinator()->GetExtensionsMenuWidget()->LayoutRootViewIfNecessary();
}

void ExtensionsMenuMainPageViewUnitTest::ClickSitePermissionsButton(
    ExtensionMenuItemView* menu_item) {
  ClickButton(menu_item->site_permissions_button_for_testing());
  WaitForAnimation();
}

ExtensionsMenuMainPageView* ExtensionsMenuMainPageViewUnitTest::main_page() {
  ExtensionsMenuViewController* menu_controller =
      menu_coordinator()->GetControllerForTesting();
  return menu_controller ? menu_controller->GetMainPageViewForTesting()
                         : nullptr;
}

ExtensionsMenuSitePermissionsPageView*
ExtensionsMenuMainPageViewUnitTest::site_permissions_page() {
  ExtensionsMenuViewController* menu_controller =
      menu_coordinator()->GetControllerForTesting();
  return menu_controller ? menu_controller->GetSitePermissionsPageForTesting()
                         : nullptr;
}

std::vector<ExtensionMenuItemView*>
ExtensionsMenuMainPageViewUnitTest::menu_items() {
  ExtensionsMenuMainPageView* page = main_page();
  return page ? page->GetMenuItems() : std::vector<ExtensionMenuItemView*>();
}

void ExtensionsMenuMainPageViewUnitTest::SetUp() {
  ExtensionsToolbarUnitTest::SetUp();
  // Menu needs web contents at construction, so we need to add them to every
  // test.
  web_contents_tester_ = AddWebContentsAndGetTester();
}

TEST_F(ExtensionsMenuMainPageViewUnitTest, ExtensionsAreSorted) {
  constexpr char kExtensionZName[] = "Z Extension";
  InstallExtension(kExtensionZName);
  constexpr char kExtensionAName[] = "A Extension";
  InstallExtension(kExtensionAName);
  constexpr char kExtensionBName[] = "b Extension";
  InstallExtension(kExtensionBName);
  constexpr char kExtensionCName[] = "C Extension";
  InstallExtension(kExtensionCName);

  ShowMenu();

  std::vector<ExtensionMenuItemView*> items = menu_items();
  ASSERT_EQ(items.size(), 4u);

  // Basic std::sort would do A,C,Z,b however we want A,b,C,Z
  std::vector<std::string> expected_items{kExtensionAName, kExtensionBName,
                                          kExtensionCName, kExtensionZName};
  EXPECT_EQ(GetNamesFromMenuItems(items), expected_items);
}

// Verifies the site access toggle is always hidden for an extension that
// doesn't request site access.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       SiteAccessToggle_NoSiteAccessRequested) {
  auto extension = InstallExtension("Extension");

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Button is hidden when site is set to "customize by extension" (default
  // setting).
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());

  // Button is hidden when site setting is set to "block all extensions".
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetVisible());
}

// Verifies the site access toggle properties for an extension that
// requests site access and access is withheld.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       SiteAccessToggle_SiteAccessWithheld) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Button is visible and off when site setting is set to "customize by
  // extension" (default setting).
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());

  // Button is hidden when site setting is set to "block all extensions".
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
}

// Verifies the site access properties for an extension that
// requests access to a specific site.
TEST_F(ExtensionsMenuMainPageViewUnitTest, SiteAccessToggle_SiteAccessGranted) {
  const GURL url("http://www.example.com");
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {url.spec()});

  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Button is visible and on when site setting is set to "customize by
  // extension" (default setting).
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());

  // Button is hidden when site setting is set to "block all extensions".
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
}

// Verifies the site access toggle properties for an extension that
// requests access to all sites.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       SiteAccessToggle_AllSitesAccessGranted) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Button is visible and on when site setting is set to "customize by
  // extension" (default setting).
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());

  // Button is hidden when site setting is set to "block all extensions".
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
}

// Verifies the site access toggle properties for an extension that only
// requests active tab access.
TEST_F(ExtensionsMenuMainPageViewUnitTest, SiteAccessToggle_ActiveTab) {
  auto extension = InstallExtensionWithPermissions("Extension", {"activeTab"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Button is visible and off when site setting is set to "customize by
  // extension" (default setting) and active tab hasn't been granted.
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());

  // Button is visible and on when site setting is set to "customize by
  // extension" and active tab as been granted.
  extensions::ActiveTabPermissionGranter* active_tab_permission_granter =
      extensions::TabHelper::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents())
          ->active_tab_permission_granter();
  ASSERT_TRUE(active_tab_permission_granter);
  active_tab_permission_granter->GrantIfRequested(extension.get());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());

  // Button is hidden when site setting is set to "block all extensions".
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
}

// Verifies the site access toggle is always hidden for enterprise extensions,
// even if the extension has site access.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       SiteAccessToggle_EnterpriseExtension) {
  auto extension =
      InstallEnterpriseExtension("Extension",
                                 /*host_permissions=*/{"<all_urls>"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Button is hidden when site setting is set to "customize by extension"
  // (default setting) because extension has site access but user cannot change
  // it.
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());

  // Button is hidden when site setting is set to "block all extensions".
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
}

// Verifies the site permissions button properties for an extension that doesn't
// request site access.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       SitePermissionsButton_NoSiteAccessRequested) {
  auto extension = InstallExtension("Extension");

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Button is visible, disabled, has no icon and has "none" text when site
  // setting is set to "customize by extension" (default setting).
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_NONE));
  EXPECT_FALSE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());

  // Button is hidden when site setting is set to "block all extensions".
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetVisible());
}

// Verifies the site permissions button properties for an extension that
// requests site access and access is withheld.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       SitePermissionsButton_SiteAccessWithheld) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Button is visible, enabled, has icon and has "on click" text when site
  // setting is set to "customize by extension" (default setting).
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());

  // Button is hidden when site setting is set to "block all extensions".
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetVisible());
}

// Verifies the site permissions button properties for an extension that
// requests access to a specific site.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       SitePermissionsButton_SiteAccessGranted) {
  const GURL url("http://www.example.com");
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {url.spec()});

  web_contents_tester()->NavigateAndCommit(url);
  WaitForAnimation();

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Button is visible, enabled, has icon and has "on site" text when site
  // setting is set to "customize by extension" (default setting).
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_SITE));
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());

  // Button is hidden when site setting is set to "block all extensions".
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetVisible());
}

// Verifies the site permissions button properties for an extension that
// requests access to all sites.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       SitePermissionsButton_AllSitesAccessGranted) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Button is visible, enabled, has icon and has "on all sites" text when
  // site setting is set to "customize by extension" (default setting).
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(
      menu_item->site_permissions_button_for_testing()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_ALL_SITES));
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());

  // Button is hidden when site setting is set to "block all extensions".
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetVisible());
}

// Verifies the site permissions button properties for an extension that
// only requests activeTab permission.
TEST_F(ExtensionsMenuMainPageViewUnitTest, SitePermissionsButton_ActiveTab) {
  auto extension = InstallExtensionWithPermissions("Extension", {"activeTab"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Button is visible, enabled, has icon and has "on click" text when
  // site setting is set to "customize by extension" (default setting).
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());

  // Button is hidden when site setting is set to "block all extensions".
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetVisible());
}

// Verifies the site permissions button properties for an enterprise extension
// that doesn't request site access.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       SitePermissionsButton_EnterpriseExtension_NoSiteAccessRequested) {
  auto extension =
      InstallEnterpriseExtension("Extension", /*host_permissions*/ {});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Button is visible, disabled, has no icon and has "none" text when
  // site setting is set to "customize by extension" (default setting).
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_NONE));
  EXPECT_FALSE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());

  // Button is hidden when site setting is set to "block all extensions".
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetVisible());
}

// Verifies the site permissions button properties for an enterprise extension
// that requests access to all sites.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       SitePermissionsButton_EnterpriseExtension_OnAllSitesAccess) {
  auto extension =
      InstallEnterpriseExtension("Extension",
                                 /*host_permissions=*/{"<all_urls>"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Button is visible, disabled, has no icon and has "on all sites" text when
  // site setting is set to "customize by extension" (default setting).
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(
      menu_item->site_permissions_button_for_testing()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_ALL_SITES));
  EXPECT_FALSE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());

  // Button is visible, disabled, has no icon and has "on all sites" text when
  // site setting is set to "block all extensions".
  // Note: Policy-installed extension can still run on the site even if the user
  // blocked all extensions because enterprise-installed extensions take
  // priority over user settings. Therefore, the button is visible (so the user
  // can see that it can run), but not clickable (because the user can't modify
  // the settings).
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(
      menu_item->site_permissions_button_for_testing()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_ALL_SITES));
  EXPECT_FALSE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
}

// Verifies the site permissions button opens the site permissions page when it
// is enabled.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       SitePermissionsButton_OpenSitePermissionsPage) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // Button is visible, enabled and has an icon when site setting is set to
  // "customize by extension" (default setting).
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());

  // Clicking on an extension's site permission enabled button should open
  // its site permission page in the menu.
  ClickSitePermissionsButton(menu_item);
  EXPECT_FALSE(main_page());
  ExtensionsMenuSitePermissionsPageView* page = site_permissions_page();
  ASSERT_TRUE(page);
  EXPECT_EQ(page->extension_id(), extension->id());
}

TEST_F(ExtensionsMenuMainPageViewUnitTest,
       AddAndRemoveExtensionWhenMainPageIsOpen) {
  constexpr char kExtensionA[] = "A Extension";
  constexpr char kExtensionC[] = "C Extension";
  InstallExtension(kExtensionA);
  InstallExtension(kExtensionC);

  ShowMenu();

  // Verify the order of the extensions is A,C.
  {
    std::vector<ExtensionMenuItemView*> items = menu_items();
    ASSERT_EQ(items.size(), 2u);
    std::vector<std::string> expected_names{kExtensionA, kExtensionC};
    EXPECT_EQ(GetNamesFromMenuItems(items), expected_names);
  }

  // Add a new extension while the menu is open.
  constexpr char kExtensionB[] = "B Extension";
  auto extensionB = InstallExtension(kExtensionB);
  LayoutMenuIfNecessary();

  // Extension should be added in the correct place.
  // Verify the new order is A,B,C.
  {
    std::vector<ExtensionMenuItemView*> items = menu_items();
    ASSERT_EQ(items.size(), 3u);
    std::vector<std::string> expected_names{kExtensionA, kExtensionB,
                                            kExtensionC};
    EXPECT_EQ(GetNamesFromMenuItems(items), expected_names);
  }

  // Remove a extension while the menu is open
  UninstallExtension(extensionB->id());
  LayoutMenuIfNecessary();

  // Verify the new order is A,C.
  {
    std::vector<ExtensionMenuItemView*> items = menu_items();
    ASSERT_EQ(items.size(), 2u);
    std::vector<std::string> expected_names{kExtensionA, kExtensionC};
    EXPECT_EQ(GetNamesFromMenuItems(items), expected_names);
  }
}

// Verifies the pin button appears on the menu item, in place of context menu
// button when state is normal, when an extension is pinned.
TEST_F(ExtensionsMenuMainPageViewUnitTest, PinnedExtensions) {
  auto extension = InstallExtension("Test Extension");

  ShowMenu();
  EXPECT_EQ(menu_items().size(), 1u);
  HoverButton* context_menu_button =
      GetOnlyMenuItem()->context_menu_button_for_testing();

  const ui::ColorProvider* color_provider =
      context_menu_button->GetColorProvider();
  auto pin_icon = gfx::Image(gfx::CreateVectorIcon(
      views::kPinIcon,
      color_provider->GetColor(kColorExtensionMenuPinButtonIcon)));
  auto three_dot_icon = gfx::Image(gfx::CreateVectorIcon(
      kBrowserToolsIcon, color_provider->GetColor(kColorExtensionMenuIcon)));

  // Verify context menu button has three dot icon for all button states.
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(context_menu_button->GetImage(views::Button::STATE_NORMAL)),
      three_dot_icon));
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(context_menu_button->GetImage(views::Button::STATE_HOVERED)),
      three_dot_icon));
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(context_menu_button->GetImage(views::Button::STATE_PRESSED)),
      three_dot_icon));

  // Pin extension.
  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);
  toolbar_model->SetActionVisibility(extension->id(), true);
  LayoutMenuIfNecessary();

  // Verify context menu button changes to pin icon only in normal state.
  // Pin icon is tested by checking it is NOT the three dot icon because of a
  // weird reason when retrieving the pin icon. We can do this because a) no
  // other icon is expected, and b) pin icon correctly appears when manually
  // testing.
  EXPECT_FALSE(gfx::test::AreImagesEqual(
      gfx::Image(context_menu_button->GetImage(views::Button::STATE_NORMAL)),
      three_dot_icon));
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(context_menu_button->GetImage(views::Button::STATE_HOVERED)),
      three_dot_icon));
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(context_menu_button->GetImage(views::Button::STATE_PRESSED)),
      three_dot_icon));

  // Unpin extension.
  toolbar_model->SetActionVisibility(extension->id(), false);
  LayoutMenuIfNecessary();

  // Verify context menu button has three dot icon for all button states.
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(context_menu_button->GetImage(views::Button::STATE_NORMAL)),
      three_dot_icon));
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(context_menu_button->GetImage(views::Button::STATE_HOVERED)),
      three_dot_icon));
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(context_menu_button->GetImage(views::Button::STATE_PRESSED)),
      three_dot_icon));
}

TEST_F(ExtensionsMenuMainPageViewUnitTest, DisableAndEnableExtension) {
  constexpr char kName[] = "Test Extension";
  auto extension_id = InstallExtension(kName)->id();

  ShowMenu();
  EXPECT_EQ(menu_items().size(), 1u);

  DisableExtension(extension_id);
  LayoutMenuIfNecessary();

  EXPECT_EQ(menu_items().size(), 0u);

  EnableExtension(extension_id);
  LayoutMenuIfNecessary();

  EXPECT_EQ(menu_items().size(), 1u);
}

// Tests that when an extension is reloaded it remains visible in the extensions
// menu.
// TODO(crbug.com/1390952): Verify context menu button shows the correct icon as
// pinned state is also preserved when a reload happens. Add this functionality
// when showing pin icon instead of context menu when extension is pinned is
// added.
TEST_F(ExtensionsMenuMainPageViewUnitTest, ReloadExtension) {
  // The extension must have a manifest to be reloaded.
  extensions::TestExtensionDir extension_directory;
  constexpr char kManifest[] = R"({
        "name": "Test Extension",
        "version": "1",
        "manifest_version": 3
      })";
  extension_directory.WriteManifest(kManifest);
  extensions::ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_directory.UnpackedPath());

  ShowMenu();
  EXPECT_EQ(menu_items().size(), 1u);

  // Reload the extension.
  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile()));
  ReloadExtension(extension->id());
  ASSERT_TRUE(registry_observer.WaitForExtensionLoaded());
  LayoutMenuIfNecessary();

  // Verify the extension is visible in the menu.
  EXPECT_EQ(menu_items().size(), 1u);
}

// Tests that a when an extension is reloaded with manifest errors, and
// therefore fails to be loaded into Chrome, it's removed from the
// extensions menu.
TEST_F(ExtensionsMenuMainPageViewUnitTest, ReloadExtensionFailed) {
  extensions::TestExtensionDir extension_directory;
  constexpr char kManifest[] = R"({
        "name": "Test Extension",
        "version": "1",
        "manifest_version": 3
      })";
  extension_directory.WriteManifest(kManifest);
  extensions::ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_directory.UnpackedPath());

  ShowMenu();
  EXPECT_EQ(menu_items().size(), 1u);

  // Replace the extension's valid manifest with one containing errors. In this
  // case, 'version' keys is missing.
  constexpr char kManifestWithErrors[] = R"({
        "name": "Test",
        "manifest_version": 3,
      })";
  extension_directory.WriteManifest(kManifestWithErrors);

  // Reload the extension. It should fail due to the manifest errors.
  extension_service()->ReloadExtensionWithQuietFailure(extension->id());
  base::RunLoop().RunUntilIdle();
  LayoutMenuIfNecessary();

  // Verify the extension is no longer visible in the menu.
  EXPECT_EQ(menu_items().size(), 0u);
}

// Tests that extension's site permission button is always hidden when site is
// restricted.
TEST_F(ExtensionsMenuMainPageViewUnitTest, RestrictedSite) {
  constexpr char kExtension[] = "Extension";
  constexpr char kEnterpriseExtension[] = "Enterprise extension";
  InstallExtension(kExtension);
  InstallEnterpriseExtension(kEnterpriseExtension,
                             /*host_permissions=*/{"<all_urls>"});

  const GURL restricted_url("chrome://extensions");
  auto restricted_origin = url::Origin::Create(restricted_url);
  web_contents_tester()->NavigateAndCommit(restricted_url);

  // By default, site settings is set to "customize by extension".
  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_EQ(permissions_manager->GetUserSiteSetting(restricted_origin),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  ShowMenu();
  ASSERT_EQ(menu_items().size(), 2u);

  // Both extension's site permissions button should be hidden.
  EXPECT_FALSE(
      menu_items()[0]->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(
      menu_items()[1]->site_permissions_button_for_testing()->GetVisible());

  // Change site settings to "block all extensions".
  extensions::PermissionsManagerWaiter waiter(
      PermissionsManager::Get(browser()->profile()));
  permissions_manager->UpdateUserSiteSetting(
      restricted_origin,
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  waiter.WaitForUserPermissionsSettingsChange();

  // Both extension's site permission button should still be hidden (restricted
  // sites have priority over enterprise extensions).
  EXPECT_FALSE(
      menu_items()[0]->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(
      menu_items()[1]->site_permissions_button_for_testing()->GetVisible());
}
