// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_site_permissions_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
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

  // Returns the extension names in the request access section. If it's empty,
  // the section is not visible.
  std::vector<extensions::ExtensionId> GetExtensionsInRequestAccessSection();

  // Returns the extension ids in the request access button in the toolbar.
  std::vector<extensions::ExtensionId> GetExtensionsInRequestAccessButton();

  // Since this is a unittest, the extensions menu widget sometimes needs a
  // nudge to re-layout the views.
  void LayoutMenuIfNecessary();

  void ClickSitePermissionsButton(ExtensionMenuItemView* menu_item);

  // Clicks the site access toggle in the extension's menu main page. If
  // `active_tab_only` is false, it waits for user site access updated (toggling
  // an extension with just active tab grants tab permission, but doesn't change
  // the extension site access).
  void ClickSiteAccessToggle(ExtensionMenuItemView* menu_item,
                             bool active_tab_only = false);

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

std::vector<extensions::ExtensionId>
ExtensionsMenuMainPageViewUnitTest::GetExtensionsInRequestAccessSection() {
  ExtensionsMenuMainPageView* page = main_page();
  return page ? page->GetExtensionsRequestingAccessForTesting()
              : std::vector<std::string>();
}

std::vector<extensions::ExtensionId>
ExtensionsMenuMainPageViewUnitTest::GetExtensionsInRequestAccessButton() {
  return extensions_container()
      ->GetExtensionsToolbarControls()
      ->request_access_button_for_testing()
      ->GetExtensionIdsForTesting();
}

void ExtensionsMenuMainPageViewUnitTest::LayoutMenuIfNecessary() {
  menu_coordinator()->GetExtensionsMenuWidget()->LayoutRootViewIfNecessary();
}

void ExtensionsMenuMainPageViewUnitTest::ClickSitePermissionsButton(
    ExtensionMenuItemView* menu_item) {
  ClickButton(menu_item->site_permissions_button_for_testing());
  WaitForAnimation();
}

void ExtensionsMenuMainPageViewUnitTest::ClickSiteAccessToggle(
    ExtensionMenuItemView* menu_item,
    bool active_tab_only) {
  extensions::PermissionsManagerWaiter waiter(
      PermissionsManager::Get(browser()->profile()));
  ClickButton(menu_item->site_access_toggle_for_testing());
  if (!active_tab_only) {
    waiter.WaitForExtensionPermissionsUpdate();
  }

  WaitForAnimation();
  LayoutMenuIfNecessary();
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

// Verifies the site access toggle and site permissions button properties for an
// extension that doesn't request site access.
TEST_F(ExtensionsMenuMainPageViewUnitTest, NoSiteAccessRequested) {
  auto extension = InstallExtension("Extension");

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // When site setting is set to "customize by extension" (default):
  //   - site access toggle is hidden.
  //   - site permissions button is visible, disabled, has no icon and has
  //   "none" text.
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_FALSE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_NONE));

  // when site setting is set to "block all extensions":
  //   - site access toggle is hidden
  //   - site permissions button is hidden
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetVisible());
}

// Verifies the site access toggle and site permissions button properties for an
// enterprise extension that doesn't request host permissions.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       NoSiteAccessRequested_EnterpriseExtension) {
  auto extension =
      InstallEnterpriseExtension("Extension", /*host_permissions*/ {});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // When site setting is set to "customize by extension" (default):
  //   - site access toggle is hidden.
  //   - site permissions button is visible, disabled, has no icon and has
  //     "none" text.
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_FALSE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_NONE));

  // When site setting is set to "block all extensions":
  //   - site access toggle is hidden
  //   - site permissions button is hidden
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetVisible());
}

// Verifies the site access toggle and site permissions button properties when
// toggling site access for an extension that requests host permissions for a
// specific site.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       HostPermissionsRequested_ToggleSiteAccess_OnSite) {
  const GURL url("http://www.example.com");
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {url.spec()});

  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // By default, site setting is set to "customize by extension" (default) and
  // extension has granted "on site" access:
  //   - site access toggle is visible and on.
  //   - site permissions button is visible, enabled, with icon and has "on
  //     site" text.
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  ASSERT_EQ(GetUserSiteAccess(*extension.get(), url),
            PermissionsManager::UserSiteAccess::kOnSite);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_SITE));

  // When site access is toggled OFF:
  //   - extension site access is "on click".
  //   - site access toggle is visible and off.
  //   - site permissions button is visible, enabled, with icon and has "on
  //     click" text.
  ClickSiteAccessToggle(menu_item);
  EXPECT_EQ(GetUserSiteAccess(*extension.get(), url),
            PermissionsManager::UserSiteAccess::kOnClick);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  // When site access is toggled ON:
  //   - extension site access is "on site".
  //   - site access toggle is visible and on.
  //   - site permissions button is visible, enabled, with icon and has "on
  //     site" text.
  ClickSiteAccessToggle(menu_item);
  EXPECT_EQ(GetUserSiteAccess(*extension.get(), url),
            PermissionsManager::UserSiteAccess::kOnSite);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_SITE));
}

// Verifies the site access toggle and site permissions button properties when
// toggling site access for an extension that requests host permissions for all
// sites.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       HostPermissionsRequested_ToggleSiteAccess_OnAllSites) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // By default, site setting is set to "customize by extension" (default) and
  // extension has granted "on all sites" access:
  //   - site access toggle is visible and on.
  //   - site permissions button is visible, enabled, with icon and has "on
  //     all sites" text.
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  ASSERT_EQ(GetUserSiteAccess(*extension.get(), url),
            PermissionsManager::UserSiteAccess::kOnAllSites);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(
      menu_item->site_permissions_button_for_testing()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_ALL_SITES));

  // When site access is toggled OFF:
  //   - extension site access is changed to "on click".
  //   - site access toggle is visible and off.
  //   - site permissions button is visible, enabled, with icon and has "on
  //     click" text.
  ClickSiteAccessToggle(menu_item);
  EXPECT_EQ(GetUserSiteAccess(*extension.get(), url),
            PermissionsManager::UserSiteAccess::kOnClick);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  // When site access is toggled ON:
  //   - extension site access is "on site". Even though previously extension
  //     had on all sites, toggling site access on grants access just to this
  //     site. User can still grant all sites in the site permissions page.
  //   - site access toggle is visible and on.
  //   - site permissions button is visible, enabled, with icon and has "on
  //     site" text.
  ClickSiteAccessToggle(menu_item);
  EXPECT_EQ(GetUserSiteAccess(*extension.get(), url),
            PermissionsManager::UserSiteAccess::kOnSite);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_SITE));
}

// Verifies the site access toggle and site permissions button properties for an
// extension that requests host permissions when host permission change with the
// menu open.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       HostPermissionsRequested_DynamicUpdates) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // When site setting is set to "customize by extension" and extension site
  // permissions are granted "on all sites" (default):
  //   - site access toggle is visible and on.
  //   - site permissions button is visible, enabled, with icon and has "on all
  //     sites" text.
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  ASSERT_EQ(GetUserSiteAccess(*extension.get(), url),
            PermissionsManager::UserSiteAccess::kOnAllSites);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(
      menu_item->site_permissions_button_for_testing()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_ALL_SITES));

  // When site setting is set to "customize by extension" and extension site
  // permissions are withheld.
  //   - site access toggle is visible and off
  //   - site permissions button is visible, enabled, with icon and has "on
  //     click" text.
  WithholdHostPermissions(extension.get());
  LayoutMenuIfNecessary();
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  // When site setting is set to "block all extensions":
  //   - site access toggle is hidden.
  //   - site permissions button is hidden.
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  LayoutMenuIfNecessary();
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetVisible());
}

// Verifies the site access toggle and site permissions button properties for an
// extension that requests host permissions.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       HostPermissionsRequested_EnterpriseExtension) {
  auto extension =
      InstallEnterpriseExtension("Extension",
                                 /*host_permissions=*/{"<all_urls>"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // When site setting is set to "customize by extension" and has granted "on
  // all sites" access (default):
  //   - site access toggle is hidden, because extension has site access but
  //     user cannot withheld it.
  //   - site permissions button is visible, disabled, has no icon and has "on
  //     all sites".
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  ASSERT_EQ(GetUserSiteAccess(*extension.get(), url),
            PermissionsManager::UserSiteAccess::kOnAllSites);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(
      menu_item->site_permissions_button_for_testing()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_ALL_SITES));
  EXPECT_FALSE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());

  // When site setting is set to "block all extensions":
  //   - extension site access is still "on all sites".
  //   - site access toggle is hidden.
  //   - site permissions button is visible, disabled, has no icon and has "on
  //     all sites" text.
  // Note: Policy-installed extension can still run on the site even if the
  // user blocked all extensions because enterprise-installed extensions take
  // priority over user settings. Therefore, the button is visible (so the
  // user can see that it can run), but not clickable (because the user can't
  // modify the settings).
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_EQ(GetUserSiteAccess(*extension.get(), url),
            PermissionsManager::UserSiteAccess::kOnAllSites);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(
      menu_item->site_permissions_button_for_testing()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_ALL_SITES));
  EXPECT_FALSE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
}

// Verifies the site access toggle and site permissions button properties when
// toggling site access for an extension that only requests active tab.
// TODO(crbug.com/1445397): Flaky on various builders.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       DISABLED_ActiveTabRequested_ToggleSiteAccess) {
  auto extension = InstallExtensionWithPermissions("Extension", {"activeTab"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // By default, site setting is set to "customize by extension" (default) and
  // extension has not active tab granted.
  //   - site access toggle is visible and off.
  //   - site permissions button is visible, enabled, with icon and has "on
  //     click" text.
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  ASSERT_EQ(GetUserSiteAccess(*extension.get(), url),
            PermissionsManager::UserSiteAccess::kOnClick);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  // When site access toggle is toggled ON:
  //   - extension site access is "on click". Since extension only requested
  //     active tab, toggling site access on grants tab permissions but doesn't
  //     change the user site access.
  //   - site access toggle is visible and on.
  //   - site permissions button is visible, enabled, with icon and has "on
  //     click" text.
  ClickSiteAccessToggle(menu_item, /*active_tab_only=*/true);
  EXPECT_EQ(GetUserSiteAccess(*extension.get(), url),
            PermissionsManager::UserSiteAccess::kOnClick);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  // When site access toggle is toggled OFF:
  //   - extension site access is "on click".
  //   - site access toggle is visible and off.
  //   - site permissions button is visible, enabled, with icon and has "on
  //     click" text.
  EXPECT_EQ(GetUserSiteAccess(*extension.get(), url),
            PermissionsManager::UserSiteAccess::kOnClick);
  ClickSiteAccessToggle(menu_item, /*active_tab_only=*/true);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));
}

// Verifies the site access toggle and site permissions button properties for an
// extension that only requests active tab when site permissions change with the
// menu open.
TEST_F(ExtensionsMenuMainPageViewUnitTest, ActiveTabRequested_DynamicUpdates) {
  auto extension = InstallExtensionWithPermissions("Extension", {"activeTab"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // When site setting is set to "customize by extension" (default) and active
  // tab is not granted:
  //   - site access toggle is visible and off.
  //   - site permissions button is visible, enabled, with icon and has "on
  //     click" text.
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  // When site setting is set to "customize by extension" active tab is granted:
  //   - site access toggle is visible and on.
  //   - site permissions button is visible, enabled, with icon and has "on
  //     click" text. Since extension only requested active tab, toggling site
  //     access on grants tab permissions but doesn't change the user site
  //     access.
  extensions::ActiveTabPermissionGranter* active_tab_permission_granter =
      extensions::TabHelper::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents())
          ->active_tab_permission_granter();
  ASSERT_TRUE(active_tab_permission_granter);
  active_tab_permission_granter->GrantIfRequested(extension.get());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  // Navigating to the same url after navigating to other url should remove tab
  // permissions. Therefore:
  //   - site access toggle is visible and off.
  //   - site permissions button is visible, enabled, with icon and has "on
  //     click" text.
  // Note: refreshing the page doesn't revoke tab permissions, thus we
  // need to re navigate to the url.
  web_contents_tester()->NavigateAndCommit(GURL("http://other-url.com"));
  web_contents_tester()->NavigateAndCommit(url);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_TRUE(
      menu_item->site_permissions_button_icon_for_testing()->GetVisible());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  // When site setting is set to "block all extensions":
  //   - site access toggle is hidden.
  //   - site permissions button is hidden.
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetVisible());
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

// Tests that the message section only displays the text container when the
// site restricts access to all extensions.
TEST_F(ExtensionsMenuMainPageViewUnitTest, MessageSection_RestrictedAccess) {
  InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL restricted_url("chrome://extensions");
  web_contents_tester()->NavigateAndCommit(restricted_url);
  ShowMenu();

  // Only the text container is displayed with restricted site message, when
  // site restricts access to all extensions.
  views::Label* text_container = main_page()->GetTextContainerForTesting();
  EXPECT_TRUE(text_container->GetVisible());
  EXPECT_EQ(text_container->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MESSAGE_SECTION_RESTRICTED_ACCESS_TEXT));
  EXPECT_FALSE(
      main_page()->GetRequestsAccessContainerForTesting()->GetVisible());
}

// Tests that the message section only displays the text container when the
// user has blocked all extensions on the site.
TEST_F(ExtensionsMenuMainPageViewUnitTest, MessageSection_UserBlockedAccess) {
  InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  // Block all extensions on `url`.
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kBlockAllExtensions);

  ShowMenu();

  // Only the text container is displayed with user blocked site message, when
  // all the extensions are blocked on this site.
  views::Label* text_container = main_page()->GetTextContainerForTesting();
  EXPECT_TRUE(text_container->GetVisible());
  EXPECT_EQ(text_container->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MESSAGE_SECTION_USER_BLOCKED_ACCESS_TEXT));
  EXPECT_FALSE(
      main_page()->GetRequestsAccessContainerForTesting()->GetVisible());
}

// Tests that all the containers in the message section are hidden when the user
// can customize the extensions site access but no extension is requesting site
// access.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       MessageSection_UserCustomizedAccess_NoExtensions) {
  // Install extensions that cannot request site access in the menu because
  // they don't request host permissions, or because they are granted host
  // permissions by enterprise.
  InstallExtensionWithHostPermissions("Extension", {});
  InstallEnterpriseExtension("Enterprise extension",
                             /*host_permissions=*/{"<all_urls>"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);
  ShowMenu();

  // Message section is hidden when user can customize the site access of each
  // extension, but no extension is requesting site access.
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  EXPECT_FALSE(main_page()->GetTextContainerForTesting()->GetVisible());
  EXPECT_FALSE(
      main_page()->GetRequestsAccessContainerForTesting()->GetVisible());
}

// Test that the message section only displays the requests access container
// when the user can customize the extensions site access and at least 1+
// extensios are requesting site access.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       MessageSection_UserCustomizedAccess_Extensions) {
  // Install two extension that requests host permissions.
  auto extension_A =
      InstallExtensionWithHostPermissions("Extension A", {"<all_urls>"});
  auto extension_B =
      InstallExtensionWithHostPermissions("Extension B", {"<all_urls>"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  // By default, user can customize the site access of each extension and site
  // access will be granted
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  ShowMenu();

  // Message section is hidden when user can customize site access but all
  // extensions have granted access.
  EXPECT_FALSE(main_page()->GetTextContainerForTesting()->GetVisible());
  EXPECT_FALSE(
      main_page()->GetRequestsAccessContainerForTesting()->GetVisible());
  EXPECT_TRUE(GetExtensionsInRequestAccessSection().empty());

  // Message section shows request access container with extension A
  // when its site access is withheld.
  WithholdHostPermissions(extension_A.get());
  LayoutMenuIfNecessary();
  EXPECT_FALSE(main_page()->GetTextContainerForTesting()->GetVisible());
  EXPECT_TRUE(
      main_page()->GetRequestsAccessContainerForTesting()->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension_A->id()));

  // Message section shows requests access container with extension A if its
  // site access is still withheld when any other extension is updated.
  // Extension B is not included because it has granted site access.
  UpdateUserSiteAccess(*extension_B.get(),
                       browser()->tab_strip_model()->GetActiveWebContents(),
                       extensions::PermissionsManager::UserSiteAccess::kOnSite);
  LayoutMenuIfNecessary();
  EXPECT_FALSE(main_page()->GetTextContainerForTesting()->GetVisible());
  EXPECT_TRUE(
      main_page()->GetRequestsAccessContainerForTesting()->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension_A->id()));
}

TEST_F(ExtensionsMenuMainPageViewUnitTest,
       MessageSection_UserCustomizedAccess_AllowExtension) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();

  constexpr char kActivatedUserAction[] =
      "Extensions.Toolbar.ExtensionActivatedFromAllowingRequestAccessInMenu";
  base::UserActionTester user_action_tester;
  auto* permissions = PermissionsManager::Get(profile());

  // When extension is requesting site access:
  //   - message section (menu) includes extension and is visible.
  //   - request access button (toolbar) includes extension.
  //   - action has not been run.
  //   - site access is "on click".
  EXPECT_TRUE(
      main_page()->GetRequestsAccessContainerForTesting()->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension->id()));
  EXPECT_THAT(GetExtensionsInRequestAccessButton(),
              testing::ElementsAre(extension->id()));
  EXPECT_EQ(user_action_tester.GetActionCount(kActivatedUserAction), 0);
  EXPECT_EQ(permissions->GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnClick);

  // Click on the allow button for the extension.
  views::View* extension_entry =
      main_page()->GetExtensionRequestingAccessEntryForTesting(extension->id());
  ASSERT_TRUE(extension_entry);
  views::Button* extension_allow_button =
      static_cast<views::Button*>(extension_entry->children()[2]);
  ClickButton(extension_allow_button);

  WaitForAnimation();
  LayoutContainerIfNecessary();
  LayoutMenuIfNecessary();

  // When extension is granted site access via 'allow' button:
  //   - message section (menu) does not include extension and is hidden
  //   - request access button (toolbar) does not include extension
  //   - action has been run
  //   - site access is still "on click" since clicking the button grants one
  //   time access
  EXPECT_FALSE(
      main_page()->GetRequestsAccessContainerForTesting()->GetVisible());
  EXPECT_TRUE(GetExtensionsInRequestAccessSection().empty());
  EXPECT_TRUE(GetExtensionsInRequestAccessButton().empty());
  EXPECT_EQ(user_action_tester.GetActionCount(kActivatedUserAction), 1);
  EXPECT_EQ(permissions->GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnClick);
}
