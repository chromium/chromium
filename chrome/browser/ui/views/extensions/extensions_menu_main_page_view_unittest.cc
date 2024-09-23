// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/permissions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
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
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_utils.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

// Returns the extension names from the given `menu_items`.
std::vector<std::string> GetNamesFromMenuItems(
    std::vector<ExtensionMenuItemView*> menu_items) {
  return base::ToVector(menu_items, [](ExtensionMenuItemView* item) {
    return base::UTF16ToUTF8(
        item->primary_action_button_for_testing()->label_text_for_testing());
  });
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
  raw_ptr<content::WebContentsTester, DanglingUntriaged> web_contents_tester_;
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
      ->GetRequestAccessButton()
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
  //   - site permissions button is visible, disabled, and has the corresponding
  //     strings.
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            u"No access needed");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetTooltipText(),
            std::u16string());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()
                ->GetViewAccessibility()
                .GetCachedName(),
            u"No access needed");

  // When site setting is set to "block all extensions":
  //   - site access toggle is hidden.
  //   - site permissions button is hidden.
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
  //   - site permissions button is visible, disabled and has the corresponding
  //     strings.
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            u"No access needed");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetTooltipText(),
            u"Installed by your administrator");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()
                ->GetViewAccessibility()
                .GetCachedName(),
            u"No access needed. Installed by your administrator");

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
  //   - site permissions button is visible, enabled and has "on site" text.
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  ASSERT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnSite);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_SITE));

  // When site access is toggled OFF:
  //   - extension site access is "on click".
  //   - site access toggle is visible and off.
  //   - site permissions button is visible, enabled and has the corresponding
  //     strings.
  ClickSiteAccessToggle(menu_item);
  EXPECT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnClick);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            u"Ask on every visit");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetTooltipText(),
            u"Change site permissions");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()
                ->GetViewAccessibility()
                .GetCachedName(),
            u"Ask on every visit. Select to change site permissions");

  // When site access is toggled ON:
  //   - extension site access is "on site", since that was the previous
  //     granted site access state.
  //   - site access toggle is visible and on.
  //   - site permissions button is visible, enabled and has the corresponding
  //     strings.
  ClickSiteAccessToggle(menu_item);
  EXPECT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnSite);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            u"Always on this site");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetTooltipText(),
            u"Change site permissions");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()
                ->GetViewAccessibility()
                .GetCachedName(),
            u"Always on this site. Select to change site permissions");
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
  //   - site permissions button is visible, enabled and has the corresponding
  //     strings.
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  ASSERT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnAllSites);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            u"Always on all sites");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetTooltipText(),
            u"Change site permissions");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()
                ->GetViewAccessibility()
                .GetCachedName(),
            u"Always on all sites. Select to change site permissions");

  // When site access is toggled OFF:
  //   - extension site access is changed to "on click".
  //   - site access toggle is visible and off.
  //   - site permissions button is visible, enabled and has the corresponding
  //     strings.
  ClickSiteAccessToggle(menu_item);
  EXPECT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnClick);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            u"Ask on every visit");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetTooltipText(),
            u"Change site permissions");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()
                ->GetViewAccessibility()
                .GetCachedName(),
            u"Ask on every visit. Select to change site permissions");

  // When site access is toggled ON:
  //   - extension site access is "on all sites", since that was the previous
  //     granted site access state.
  //   - site access toggle is visible and on.
  //   - site permissions button is visible, enabled and has the corresponding
  //     strings.
  ClickSiteAccessToggle(menu_item);
  EXPECT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnAllSites);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            u"Always on all sites");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetTooltipText(),
            u"Change site permissions");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()
                ->GetViewAccessibility()
                .GetCachedName(),
            u"Always on all sites. Select to change site permissions");
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
  //   - site permissions button is visible, enabled, and has "on all sites"
  //   text.
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  ASSERT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnAllSites);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(
      menu_item->site_permissions_button_for_testing()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_ALL_SITES));

  // When site setting is set to "customize by extension" and extension site
  // permissions are withheld.
  //   - site access toggle is visible and off
  //   - site permissions button is visible, enabled and has "on click" text.
  WithholdHostPermissions(extension.get());
  LayoutMenuIfNecessary();
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
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

// Verifies the site access toggle persists its previous state when toggling
// site access on.
TEST_F(
    ExtensionsMenuMainPageViewUnitTest,
    HostPermissionsRequested_DynamicUpdates_TogglePersistsPreviousSiteAccess) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // By default, site setting is set to "customize by extension" (default) and
  // extension has granted "on all sites" access:
  //   - site access toggle is on.
  //   - site permissions button has "on all sites" text.
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  ASSERT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnAllSites);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_EQ(
      menu_item->site_permissions_button_for_testing()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_ALL_SITES));

  // Update site access to be "on click".
  //   - site access toggle is off
  //   - site permissions button has "on click" text.
  UpdateUserSiteAccess(*extension,
                       browser()->tab_strip_model()->GetActiveWebContents(),
                       PermissionsManager::UserSiteAccess::kOnClick);
  LayoutMenuIfNecessary();
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  // Toggle extension site access ON:
  //   - site access toggle is on
  //   - site permissions button has "on all sites" text, since that was the
  //     previous granted site access state.
  ClickSiteAccessToggle(menu_item);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_EQ(
      menu_item->site_permissions_button_for_testing()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_ALL_SITES));
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
  //   - site permissions button is visible, disabled and has the corresponding
  //     strings.
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  ASSERT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnAllSites);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            u"Always on all sites");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetTooltipText(),
            u"Installed by your administrator");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()
                ->GetViewAccessibility()
                .GetCachedName(),
            u"Always on all sites. Installed by your administrator");

  // When site setting is set to "block all extensions":
  //   - extension site access is still "on all sites".
  //   - site access toggle is hidden.
  //   - site permissions button is visible, disabled and has the corresponding
  //     strings
  // Note: Policy-installed extension can still run on the site even if the
  // user blocked all extensions because enterprise-installed extensions take
  // priority over user settings. Therefore, the button is visible (so the
  // user can see that it can run), but not clickable (because the user can't
  // modify the settings).
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  EXPECT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnAllSites);
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            u"Always on all sites");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetTooltipText(),
            u"Installed by your administrator");
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()
                ->GetViewAccessibility()
                .GetCachedName(),
            u"Always on all sites. Installed by your administrator");
}

// Verifies the site access toggle and site permissions button properties when
// toggling site access for an extension that only requests active tab.
// TODO(crbug.com/40268140): Flaky on various builders.
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
  //   - site permissions button is visible, enabled and has "on click" text.
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  ASSERT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnClick);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  // When site access toggle is toggled ON:
  //   - extension site access is "on click". Since extension only requested
  //     active tab, toggling site access on grants tab permissions but doesn't
  //     change the user site access.
  //   - site access toggle is visible and on.
  //   - site permissions button is visible, enabled and has "on click" text.
  ClickSiteAccessToggle(menu_item, /*active_tab_only=*/true);
  EXPECT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnClick);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  // When site access toggle is toggled OFF:
  //   - extension site access is "on click".
  //   - site access toggle is visible and off.
  //   - site permissions button is visible, enabled and has "on click" text.
  EXPECT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnClick);
  ClickSiteAccessToggle(menu_item, /*active_tab_only=*/true);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));
}

// Verifies the site access toggle and site permissions button properties when
// toggling an extension off that has "on click" site access and granted tab
// permissions (meaning it has access to the current site).
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       ActiveTabRequested_ToggleSiteAccess_UngrantTabPermissions) {
  auto extension = InstallExtensionWithPermissions("Extension", {"activeTab"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ShowMenu();
  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();

  // By default, extension has not active tab granted. Thus, extension:
  //   - site interaction is "active tab".
  //   - site access is "on click".
  //   - site access toggle is visible and off.
  //   - site permissions button is visible, enabled, and has "on click" text.
  EXPECT_EQ(GetSiteInteraction(*extension, web_contents),
            SitePermissionsHelper::SiteInteraction::kActiveTab);
  EXPECT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnClick);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  auto* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(action_runner);
  action_runner->accept_bubble_for_testing(false);

  // When extension is granted tab permissions it has:
  //   - site interaction is "granted".
  //   - site access is "on click" since granting tab permissions doesn't change
  //   site access.
  //   - site access toggle is visible and on.
  //   - site permissions button is visible, enabled, and has "on click" text.
  action_runner->GrantTabPermissions({extension.get()});
  EXPECT_EQ(GetSiteInteraction(*extension, web_contents),
            SitePermissionsHelper::SiteInteraction::kGranted);
  EXPECT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnClick);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  // When extension is toggled OFF:
  //   - site interaction is "active tab".
  //   - site access is "on click".
  //   - site access toggle is visible and off.
  //   - site permissions button is visible, enabled, and has "on click" text.
  ClickSiteAccessToggle(menu_item, /*active_tab_only=*/true);
  EXPECT_EQ(GetSiteInteraction(*extension, web_contents),
            SitePermissionsHelper::SiteInteraction::kActiveTab);
  EXPECT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnClick);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
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
  //   - site permissions button is visible, enabled, and has "on click" text.
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  // When site setting is set to "customize by extension" active tab is granted:
  //   - site access toggle is visible and on.
  //   - site permissions button is visible, enabled, and has "on click" text.
  //   Since extension only requested active tab, toggling site
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
  EXPECT_EQ(menu_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));

  // Navigating to the same url after navigating to other url should remove tab
  // permissions. Therefore:
  //   - site access toggle is visible and off.
  //   - site permissions button is visible, enabled, and has "on click" text.
  // Note: refreshing the page doesn't revoke tab permissions, thus we
  // need to re navigate to the url.
  web_contents_tester()->NavigateAndCommit(GURL("http://other-url.com"));
  web_contents_tester()->NavigateAndCommit(url);
  EXPECT_TRUE(menu_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_item->site_access_toggle_for_testing()->GetIsOn());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());
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

  // Button is visible and enabled when site setting is set to "customize by
  // extension" (default setting).
  EXPECT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(menu_item->site_permissions_button_for_testing()->GetEnabled());

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

// Tests that the extensions menu is dynamically updated when there is a
// navigation while the menu is opened.
TEST_F(ExtensionsMenuMainPageViewUnitTest, NavigationWhenMainPageIsOpen) {
  auto extension_A =
      InstallExtensionWithHostPermissions("Extension A", {"<all_urls>"});
  auto extension_B = InstallExtensionWithHostPermissions(
      "Extension B", {"*://www.other.com/"});

  web_contents_tester()->NavigateAndCommit(GURL("http://www.site.com"));

  // Withhold extension A's host permissions and add a site access
  // request.
  WithholdHostPermissions(extension_A.get());
  AddSiteAccessRequest(*extension_A,
                       browser()->tab_strip_model()->GetActiveWebContents());

  ShowMenu();

  // Retrieve menu views for testing.
  ExtensionMenuItemView* extension_A_item = menu_items()[0];
  ExtensionMenuItemView* extension_b_item = menu_items()[1];
  ASSERT_EQ(extension_A_item->primary_action_button_for_testing()
                ->label_text_for_testing(),
            u"Extension A");
  ASSERT_EQ(extension_b_item->primary_action_button_for_testing()
                ->label_text_for_testing(),
            u"Extension B");
  views::View* requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();

  // Verify site settings label has the current site, request access section
  // shows extension A request and extension items have the site access text
  // based on their access.
  ASSERT_EQ(main_page()->GetSiteSettingLabelForTesting(),
            u"Allow extensions on site.com");
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension_A->id()));
  EXPECT_EQ(extension_A_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));
  EXPECT_EQ(extension_b_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_NONE));

  // Navigate to a same-origin site.
  web_contents_tester()->NavigateAndCommit(GURL(("http://www.site.com/path")));
  LayoutMenuIfNecessary();

  // Verify site settings label has the new site, request access section still
  // shows the extension A request and extension items have the same site access
  // text.
  ASSERT_EQ(main_page()->GetSiteSettingLabelForTesting(),
            u"Allow extensions on site.com");
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension_A->id()));
  EXPECT_EQ(extension_A_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));
  EXPECT_EQ(extension_b_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_NONE));

  // Navigate to a cross-origin site.
  web_contents_tester()->NavigateAndCommit(GURL(("http://www.other.com")));
  LayoutMenuIfNecessary();

  // Verify site settings label has the new site, request access section is not
  // visible (requests are reset on cross-origin navigations) and extension
  // items updated their site access text based on their access.
  ASSERT_EQ(main_page()->GetSiteSettingLabelForTesting(),
            u"Allow extensions on other.com");
  EXPECT_FALSE(requests_access_container->GetVisible());
  EXPECT_EQ(extension_A_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK));
  EXPECT_EQ(extension_b_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_SITE));
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
  auto three_dot_icon = gfx::Image(
      gfx::CreateVectorIcon(kBrowserToolsChromeRefreshIcon,
                            color_provider->GetColor(kColorExtensionMenuIcon)));

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
// TODO(crbug.com/40879945): Verify context menu button shows the correct icon
// as pinned state is also preserved when a reload happens. Add this
// functionality when showing pin icon instead of context menu when extension is
// pinned is added.
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

// Tests that the site setting toggle, and extensions' site access toggle and
// site permission button are always hidden when site is restricted.
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

  // Verify site setting toggle is not visible, since no extension can customize
  // a restricted site.
  EXPECT_FALSE(main_page()->GetSiteSettingsToggleForTesting()->GetVisible());

  // Verify both extensions':
  //   - site access toggle is hidden, since site access cannot be changed
  //   - site permission button is hidden, since restricted sites have priority
  //   over enterprise extensions.
  EXPECT_FALSE(menu_items()[0]->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(menu_items()[1]->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(
      menu_items()[0]->site_permissions_button_for_testing()->GetVisible());
  EXPECT_FALSE(
      menu_items()[1]->site_permissions_button_for_testing()->GetVisible());
}

// Tests that the site setting toggle and extension's site access toggle is
// always hidden, and extensions' site permissions button is visible and
// disabled when site is blocked by policy.
TEST_F(ExtensionsMenuMainPageViewUnitTest, PolicyBlockedSite) {
  URLPattern default_policy_blocked_pattern =
      URLPattern(URLPattern::SCHEME_ALL, "*://*.policy-blocked.com/*");

  // Add a policy-blocked site.
  extensions::URLPatternSet default_allowed_hosts;
  extensions::URLPatternSet default_blocked_hosts;
  default_blocked_hosts.AddPattern(default_policy_blocked_pattern);
  extensions::PermissionsData::SetDefaultPolicyHostRestrictions(
      extensions::util::GetBrowserContextId(browser()->profile()),
      default_blocked_hosts, default_allowed_hosts);

  // Install extensions requesting host permissions or activeTab.
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  auto activeTab_extension =
      InstallExtensionWithPermissions("Extension: activeTab", {"activeTab"});
  auto enterprise_extension =
      InstallEnterpriseExtension("Extension: enterprise",
                                 /*host_permissions=*/{"<all_urls>"});

  // Allow enterprise extension access to policy-blocked site.
  extensions::URLPatternSet allowed_hosts;
  extensions::URLPatternSet blocked_hosts;
  allowed_hosts.AddPattern(default_policy_blocked_pattern);
  enterprise_extension->permissions_data()->SetPolicyHostRestrictions(
      blocked_hosts, allowed_hosts);

  // Navigate to the policy-blocked site.
  const GURL policy_blocked_url("https://www.policy-blocked.com");
  auto restricted_origin = url::Origin::Create(policy_blocked_url);
  web_contents_tester()->NavigateAndCommit(policy_blocked_url);

  ShowMenu();
  ASSERT_EQ(menu_items().size(), 3u);

  // Verify site setting toggle is not visible, since no extension can customize
  // a policy-blocked site.
  EXPECT_FALSE(main_page()->GetSiteSettingsToggleForTesting()->GetVisible());

  // Retrieve menu items.
  ExtensionMenuItemView* extension_item = menu_items()[0];
  ExtensionMenuItemView* activeTab_extension_item = menu_items()[1];
  ExtensionMenuItemView* enterprise_extension_item = menu_items()[2];
  ASSERT_EQ(extension_item->primary_action_button_for_testing()
                ->label_text_for_testing(),
            u"Extension");
  ASSERT_EQ(activeTab_extension_item->primary_action_button_for_testing()
                ->label_text_for_testing(),
            u"Extension: activeTab");
  ASSERT_EQ(enterprise_extension_item->primary_action_button_for_testing()
                ->label_text_for_testing(),
            u"Extension: enterprise");

  // Verify all extensions':
  //   - site access toggle is hidden, since site access cannot be changed
  //   - site permissions button is visible and disabled. We leave them visible
  //     because enterprise extensions can still have access to the site, but
  //     disabled because site access cannot be changed.
  EXPECT_FALSE(extension_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(
      activeTab_extension_item->site_access_toggle_for_testing()->GetVisible());
  EXPECT_FALSE(enterprise_extension_item->site_access_toggle_for_testing()
                   ->GetVisible());
  EXPECT_TRUE(
      extension_item->site_permissions_button_for_testing()->GetVisible());
  EXPECT_TRUE(activeTab_extension_item->site_permissions_button_for_testing()
                  ->GetVisible());
  EXPECT_TRUE(enterprise_extension_item->site_permissions_button_for_testing()
                  ->GetVisible());
  EXPECT_FALSE(
      extension_item->site_permissions_button_for_testing()->GetEnabled());
  EXPECT_FALSE(activeTab_extension_item->site_permissions_button_for_testing()
                   ->GetEnabled());
  EXPECT_FALSE(enterprise_extension_item->site_permissions_button_for_testing()
                   ->GetEnabled());

  // Verify site permission button text for:
  //   - extension and activeTab extension is "none", since site is blocked by
  //   policy
  //   - enterprise extension is "on all sites", since site is allowed to the
  //     extension by policy.
  EXPECT_EQ(extension_item->site_permissions_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_NONE));
  EXPECT_EQ(activeTab_extension_item->site_permissions_button_for_testing()
                ->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_NONE));
  EXPECT_EQ(
      enterprise_extension_item->site_permissions_button_for_testing()
          ->GetText(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_ALL_SITES));
}

// Tests that the message section only displays the text container when the
// site restricts access to all extensions.
TEST_F(ExtensionsMenuMainPageViewUnitTest, MessageSection_RestrictedAccess) {
  InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL restricted_url("chrome://extensions");
  web_contents_tester()->NavigateAndCommit(restricted_url);

  ShowMenu();
  views::View* text_container = main_page()->GetTextContainerForTesting();
  views::View* reload_container = main_page()->GetReloadContainerForTesting();
  views::View* requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();

  // Only the text container is displayed with restricted site message, when
  // site restricts access to all extensions.
  EXPECT_TRUE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());
  EXPECT_EQ(views::AsViewClass<views::Label>(text_container->children()[0])
                ->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MESSAGE_SECTION_RESTRICTED_ACCESS_TEXT));
}

// Tests that the message section only displays the text container when the
// site has policy-blocked access to all non-enterprise extensions.
TEST_F(ExtensionsMenuMainPageViewUnitTest, MessageSection_PolicyBlockedAccess) {
  // Add a policy blocked site.
  extensions::URLPatternSet default_blocked_hosts;
  extensions::URLPatternSet default_allowed_hosts;
  default_blocked_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "*://*.policy-blocked.com/*"));
  extensions::PermissionsData::SetDefaultPolicyHostRestrictions(
      extensions::util::GetBrowserContextId(browser()->profile()),
      default_blocked_hosts, default_allowed_hosts);

  InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  InstallEnterpriseExtension("Enterprise extension", {"<all_urls>"});

  // Navigate to the policy-blocked site.
  const GURL policy_blocked_url("https://www.policy-blocked.com");
  web_contents_tester()->NavigateAndCommit(policy_blocked_url);

  ShowMenu();
  views::View* text_container = main_page()->GetTextContainerForTesting();
  views::View* reload_container = main_page()->GetReloadContainerForTesting();
  views::View* requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();

  // Only the text container is displayed with policy blocked site message and
  // tooltip, when site access is blocked to all non-enterprise extensions.
  EXPECT_TRUE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());
  EXPECT_EQ(
      views::AsViewClass<views::Label>(text_container->children()[0])
          ->GetText(),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MESSAGE_SECTION_POLICY_BLOCKED_ACCESS_TEXT));
  EXPECT_TRUE(text_container->children()[1]->GetVisible());
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
  views::View* text_container = main_page()->GetTextContainerForTesting();
  views::View* reload_container = main_page()->GetReloadContainerForTesting();
  views::View* requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();

  // Only the text container is displayed when all the extensions are blocked on
  // this site.
  EXPECT_TRUE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());

  // Container has blocked site message and tooltip icon is hidden since there
  // are not enterprise extensions that still have access.
  EXPECT_EQ(views::AsViewClass<views::Label>(text_container->children()[0])
                ->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MESSAGE_SECTION_USER_BLOCKED_ACCESS_TEXT));
  EXPECT_FALSE(text_container->children()[1]->GetVisible());
}

// Tests that the message section only displays the text container (and the
// tooltip icon is visible since at least one extension is installed by
// enterprise) when the user has blocked all extensions on the site.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       MessageSection_UserBlockedAccess_Enterprise) {
  InstallEnterpriseExtension("Extension", {"<all_urls>"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  // Block all extensions on `url`.
  UpdateUserSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions, url);
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kBlockAllExtensions);

  ShowMenu();
  views::View* text_container = main_page()->GetTextContainerForTesting();
  views::View* reload_container = main_page()->GetReloadContainerForTesting();
  views::View* requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();

  // Only the text container is displayed when all the extensions are blocked on
  // this site.
  EXPECT_TRUE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());

  // Container has blocked site message and tooltip icon is visible since there
  // is an enterprise extensions that still has access.
  EXPECT_EQ(views::AsViewClass<views::Label>(text_container->children()[0])
                ->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_MESSAGE_SECTION_USER_BLOCKED_ACCESS_TEXT));
  EXPECT_TRUE(text_container->children()[1]->GetVisible());
}

// Test that the message section only displays the requests access container
// when the user can customize the extensions site access and at least 1+
// extensions added a site access request.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       MessageSection_UserCustomizedAccess_Extensions) {
  // Install two extension that requests host permissions.
  auto extension_A =
      InstallExtensionWithHostPermissions("Extension A", {"<all_urls>"});
  auto extension_B =
      InstallExtensionWithHostPermissions("Extension B", {"<all_urls>"});

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // By default, user can customize the site access of each extension and site
  // access will be granted
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  ShowMenu();
  views::View* text_container = main_page()->GetTextContainerForTesting();
  views::View* reload_container = main_page()->GetReloadContainerForTesting();
  views::View* requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();

  // Message section is hidden (all containers are not visible) when user can
  // customize site access but all extensions have granted access.
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());

  // Message section is hidden (all containers are not visible) when extensions
  // with withheld access hasn't added a site access request.
  WithholdHostPermissions(extension_A.get());
  WithholdHostPermissions(extension_B.get());
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());

  // Message section shows request access container with extension A when
  // extension A added a site access request.
  AddSiteAccessRequest(*extension_A, web_contents);
  LayoutMenuIfNecessary();
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension_A->id()));

  // Message section shows request access container with extension A and B when
  // both extensions added site access requests.
  AddSiteAccessRequest(*extension_B, web_contents);
  LayoutMenuIfNecessary();
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension_A->id(), extension_B->id()));

  // Message section shows request access container with extension B when
  // extension A removed its site access request.
  RemoveSiteAccessRequest(*extension_A, web_contents);
  LayoutMenuIfNecessary();
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension_B->id()));

  // Message section is hidden (all containers are not visible) when extension B
  // removed its site access request.
  RemoveSiteAccessRequest(*extension_B, web_contents);
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());
}

// Test that the message section only displays the requests access container
// when the user can customize the extensions site access and an extension added
// a site access request with a pattern filter that matches the current site.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       MessageSection_UserCustomizedAccess_RequestsWithPattern) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // By default, user can customize the site access of each extension and site
  // access will be granted
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  ShowMenu();
  views::View* text_container = main_page()->GetTextContainerForTesting();
  views::View* reload_container = main_page()->GetReloadContainerForTesting();
  views::View* requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();

  // Message section is hidden (all containers are not visible) when extension
  // with withheld access hasn't added a site access request.
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());

  // Message section is hidden (all containers are not visible) when extension
  // adds a site access request with filter that doesn't match the current web
  // contents.
  URLPattern filter(extensions::Extension::kValidHostPermissionSchemes,
                    "http://www.other.com/");
  AddSiteAccessRequest(*extension, web_contents, filter);
  LayoutMenuIfNecessary();
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());

  // Message section shows request access container with extension when it added
  // a site access request with filter that matches the current web contents.
  filter = URLPattern(extensions::Extension::kValidHostPermissionSchemes,
                      "http://www.example.com/");
  AddSiteAccessRequest(*extension, web_contents, filter);
  LayoutMenuIfNecessary();
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension->id()));

  // Message section is hidden (all containers are not visible) when extension
  // adds a site access request with filter that doesn't match the current web
  // contents (previous request was removed).
  filter = URLPattern(extensions::Extension::kValidHostPermissionSchemes,
                      "http://www.example.com/other");
  AddSiteAccessRequest(*extension, web_contents, filter);
  LayoutMenuIfNecessary();
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());
}

// Test when the user can customize the extensions site access and an extension
// added a site access request with a pattern filter that matches the current
// site after same-origin navigations.
TEST_F(
    ExtensionsMenuMainPageViewUnitTest,
    MessageSection_UserCustomizedAccess_RequestsWithPattern_NavigationBetweenPages) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);

  // By default, user can customize the site access of each extension and site
  // access will be granted
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  ShowMenu();
  views::View* text_container = main_page()->GetTextContainerForTesting();
  views::View* reload_container = main_page()->GetReloadContainerForTesting();
  views::View* requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();

  // Message section is hidden (all containers are not visible) when extension
  // adds a site access request for extension with a filter that doesn't match
  // the current web contents.
  URLPattern filter(extensions::Extension::kValidHostPermissionSchemes,
                    "*://*/path");
  AddSiteAccessRequest(
      *extension, browser()->tab_strip_model()->GetActiveWebContents(), filter);
  LayoutMenuIfNecessary();
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());

  // Navigate to a same-origin site that matches the filter.
  // Message section shows request access container with extension when it added
  // a site access request with filter that matches the current web contents.
  web_contents_tester()->NavigateAndCommit(GURL("http://www.example.com/path"));
  LayoutMenuIfNecessary();
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension->id()));

  // Navigate to a cross-origin site that matches the filters. Since it's a
  // cross-origin navigation, requests are reset.
  // Message section is hidden (all containers are not visible) when extension
  // has no requests for the current site.
  web_contents_tester()->NavigateAndCommit(GURL("http://www.other.com/"));
  LayoutMenuIfNecessary();
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_FALSE(requests_access_container->GetVisible());
}

// Tests that an extension's requests access container removes an extension's
// request when the extension is granted site access.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       MessageSection_UserCustomizedAccess_ExtensionGrantedSiteAccess) {
  // Install two extension that requests host permissions.
  auto extension_A = InstallExtensionWithHostPermissions(
      "Extension A", {"*://www.example.com/*"});
  auto extension_B =
      InstallExtensionWithHostPermissions("Extension B", {"<all_urls>"});
  WithholdHostPermissions(extension_A.get());
  WithholdHostPermissions(extension_B.get());

  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ShowMenu();
  views::View* requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();

  // Add site access requests for both extensions and verify they are both
  // visible on the request access container.
  AddSiteAccessRequest(*extension_A, web_contents);
  AddSiteAccessRequest(*extension_B, web_contents);
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension_A->id(), extension_B->id()));

  // Grant site access to extension B and verify request access container only
  // has extension A, since extension's B request was removed once the extension
  // gained access to the site.
  UpdateUserSiteAccess(*extension_B, web_contents,
                       PermissionsManager::UserSiteAccess::kOnSite);
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension_A->id()));
}

TEST_F(ExtensionsMenuMainPageViewUnitTest,
       MessageSection_UserCustomizedAccess_AllowExtension) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  // Navigate to a site and add a site access request for the extension.
  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);
  AddSiteAccessRequest(*extension,
                       browser()->tab_strip_model()->GetActiveWebContents());

  ShowMenu();
  views::View* requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();

  constexpr char kActivatedUserAction[] =
      "Extensions.Toolbar.ExtensionActivatedFromAllowingRequestAccessInMenu";
  base::UserActionTester user_action_tester;
  auto* permissions = PermissionsManager::Get(profile());

  // When extension added a site access request:
  //   - message section (menu) includes extension and is visible.
  //   - request access button (toolbar) includes extension.
  //   - action has not been run.
  //   - site access is "on click".
  EXPECT_TRUE(requests_access_container->GetVisible());
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
      static_cast<views::Button*>(extension_entry->children()[3]);
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
  EXPECT_FALSE(requests_access_container->GetVisible());
  EXPECT_TRUE(GetExtensionsInRequestAccessSection().empty());
  EXPECT_TRUE(GetExtensionsInRequestAccessButton().empty());
  EXPECT_EQ(user_action_tester.GetActionCount(kActivatedUserAction), 1);
  EXPECT_EQ(permissions->GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnClick);
}

TEST_F(ExtensionsMenuMainPageViewUnitTest,
       MessageSection_UserCustomizedAccess_DismissExtension) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  // Navigate to a site and add a site access request for the extension.
  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);
  AddSiteAccessRequest(*extension,
                       browser()->tab_strip_model()->GetActiveWebContents());

  ShowMenu();

  // When extension added a site access request:
  //   - message section (menu) includes extension and is visible.
  //   - request access button (toolbar) includes extension.
  EXPECT_TRUE(
      main_page()->GetRequestsAccessContainerForTesting()->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension->id()));
  EXPECT_THAT(GetExtensionsInRequestAccessButton(),
              testing::ElementsAre(extension->id()));

  // Click on the dismiss button for the extension.
  views::View* extension_entry =
      main_page()->GetExtensionRequestingAccessEntryForTesting(extension->id());
  ASSERT_TRUE(extension_entry);
  ASSERT_EQ(extension_entry->children().size(), 4u);
  views::Button* extension_dismiss_button =
      static_cast<views::Button*>(extension_entry->children()[2]);
  ClickButton(extension_dismiss_button);

  WaitForAnimation();
  LayoutContainerIfNecessary();
  LayoutMenuIfNecessary();

  // When the extension requests are dismissed:
  //   - message section (menu) does not include extension and is hidden
  //   - request access button (toolbar) does not include extension
  EXPECT_FALSE(
      main_page()->GetRequestsAccessContainerForTesting()->GetVisible());
  EXPECT_TRUE(GetExtensionsInRequestAccessSection().empty());
  EXPECT_TRUE(GetExtensionsInRequestAccessButton().empty());

  // Re navigate to the same page.
  // Note: refreshing the page doesn't revoke tab permissions, thus we
  // need to re navigate to the url.
  web_contents_tester()->NavigateAndCommit(GURL("http://other-url.com"));
  web_contents_tester()->NavigateAndCommit(url);

  // Navigating to the same url should not show again the request, since
  // requests are reset on cross-origin navigation.
  //   - message section (menu) does not include extension and is hidden
  //   - request access button (toolbar) does not include extension.
  EXPECT_FALSE(
      main_page()->GetRequestsAccessContainerForTesting()->GetVisible());
  EXPECT_TRUE(GetExtensionsInRequestAccessSection().empty());
  EXPECT_TRUE(GetExtensionsInRequestAccessButton().empty());
}

// Tests that the message section displays an extension with a site access
// request even if it is not allowed to show requests on the toolbar (extensions
// menu is not considered part of the toolbar for this).
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       MessageSection_UserCustomizedAccess_RequestNotAllowedOnToolbar) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  // Navigate to a site and add a site access request for the extension.
  const GURL url("http://www.example.com");
  web_contents_tester()->NavigateAndCommit(url);
  AddSiteAccessRequest(*extension,
                       browser()->tab_strip_model()->GetActiveWebContents());

  // By default, user can customize the site access of each extension and site
  // access will be granted
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  ShowMenu();
  views::View* text_container = main_page()->GetTextContainerForTesting();
  views::View* reload_container = main_page()->GetReloadContainerForTesting();
  views::View* requests_access_container =
      main_page()->GetRequestsAccessContainerForTesting();

  // Message section shows request access container with extension.
  LayoutMenuIfNecessary();
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension->id()));

  // Message section shows request access container with extension even if
  // requests are not allowed in the toolbar.
  SitePermissionsHelper(profile()).SetShowAccessRequestsInToolbar(
      extension->id(), false);
  EXPECT_FALSE(text_container->GetVisible());
  EXPECT_FALSE(reload_container->GetVisible());
  EXPECT_TRUE(requests_access_container->GetVisible());
  EXPECT_THAT(GetExtensionsInRequestAccessSection(),
              testing::ElementsAre(extension->id()));
}
