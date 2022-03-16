// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_tabbed_menu_view.h"

#include <string>

#include "base/feature_list.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/site_permissions_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_urls.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/view_utils.h"
#include "url/origin.h"

namespace {

// Combobox option's indexes for site access menu items.
constexpr int kOnClickComboboxIndex = 0;
constexpr int kOnSiteComboboxIndex = 1;
constexpr int kOnAllSitesComboboxIndex = 2;
// Button's indexes for site access settings
constexpr int kGrantAllExtensionsIndex = 0;
constexpr int kBlockAllExtensionsIndex = 1;
constexpr int kCustomizeByExtensionIndex = 2;

// A scoper that manages a Browser instance created by BrowserWithTestWindowTest
// beyond the default instance it creates in SetUp.
class AdditionalBrowser {
 public:
  explicit AdditionalBrowser(std::unique_ptr<Browser> browser)
      : browser_(std::move(browser)),
        browser_view_(BrowserView::GetBrowserViewForBrowser(browser_.get())) {}

  ~AdditionalBrowser() {
    // Tear down `browser_`, similar to TestWithBrowserView::TearDown.
    browser_.release();
    browser_view_->GetWidget()->CloseNow();
  }

  ExtensionsToolbarContainer* extensions_container() {
    return browser_view_->toolbar()->extensions_container();
  }

 private:
  std::unique_ptr<Browser> browser_;
  BrowserView* browser_view_;
};

std::vector<std::string> GetNamesFromMenuItems(
    std::vector<InstalledExtensionMenuItemView*> item_views) {
  std::vector<std::string> names;
  names.resize(item_views.size());
  std::transform(
      item_views.begin(), item_views.end(), names.begin(),
      [](InstalledExtensionMenuItemView* item) {
        return base::UTF16ToUTF8(item->primary_action_button_for_testing()
                                     ->label_text_for_testing());
      });
  return names;
}

std::vector<std::string> GetNamesFromSiteAccessMenuItems(
    std::vector<SiteAccessMenuItemView*> item_views) {
  std::vector<std::string> names;
  names.resize(item_views.size());
  std::transform(
      item_views.begin(), item_views.end(), names.begin(),
      [](SiteAccessMenuItemView* item) {
        return base::UTF16ToUTF8(item->primary_action_button_for_testing()
                                     ->label_text_for_testing());
      });
  return names;
}

}  // namespace

class ExtensionsTabbedMenuViewUnitTest : public ExtensionsToolbarUnitTest {
 public:
  ExtensionsTabbedMenuViewUnitTest();
  ~ExtensionsTabbedMenuViewUnitTest() override = default;
  ExtensionsTabbedMenuViewUnitTest(const ExtensionsTabbedMenuViewUnitTest&) =
      delete;
  ExtensionsTabbedMenuViewUnitTest& operator=(
      const ExtensionsTabbedMenuViewUnitTest&) = delete;

  content::WebContentsTester* web_contents_tester() {
    return web_contents_tester_;
  }

  ExtensionsToolbarButton* extensions_button() {
    return extensions_container()
        ->GetExtensionsToolbarControls()
        ->extensions_button();
  }
  ExtensionsToolbarButton* site_access_button() {
    return extensions_container()
        ->GetExtensionsToolbarControls()
        ->site_access_button_for_testing();
  }
  ExtensionsTabbedMenuView* extensions_tabbed_menu() {
    return ExtensionsTabbedMenuView::GetExtensionsTabbedMenuViewForTesting();
  }
  std::vector<InstalledExtensionMenuItemView*> installed_items() {
    return ExtensionsTabbedMenuView::GetExtensionsTabbedMenuViewForTesting()
        ->GetInstalledItemsForTesting();
  }
  std::vector<SiteAccessMenuItemView*> has_access_items() {
    return ExtensionsTabbedMenuView::GetExtensionsTabbedMenuViewForTesting()
        ->GetVisibleHasAccessItemsForTesting();
  }
  std::vector<SiteAccessMenuItemView*> requests_access_items() {
    return ExtensionsTabbedMenuView::GetExtensionsTabbedMenuViewForTesting()
        ->GetVisibleRequestsAccessItemsForTesting();
  }
  views::Label* site_access_message() {
    return ExtensionsTabbedMenuView::GetExtensionsTabbedMenuViewForTesting()
        ->GetSiteAccessMessageForTesting();
  }

  // Asserts there is exactly one installed menu item and then returns it.
  InstalledExtensionMenuItemView* GetOnlyInstalledMenuItem();
  // Asserts there is exactly one has access menu item and then returns it.
  SiteAccessMenuItemView* GetOnlyHasAccessMenuItem();
  // Asserts there is exactly one requests access menu item and then returns it.
  SiteAccessMenuItemView* GetOnlyRequestsAccessMenuItem();

  // Returns whether the has access section is displayed on the site access tab.
  bool IsHasAccessSectionDisplayed();
  // Returns whether the requests access section is displayed on the site access
  // tab.
  bool IsRequestsAccessSectionDisplayed();

  // Opens the tabbed menu in the installed tab.
  void ShowInstalledTabInMenu();
  // Opens the tabbed menu in the site access tab.
  void ShowSiteAccessTabInMenu();

  void ClickSiteAccessButton();
  void ClickExtensionsButton();

  void ClickPrimaryActionButton(InstalledExtensionMenuItemView* item);
  void ClickPrimaryActionButton(SiteAccessMenuItemView* item);
  void ClickPinButton(InstalledExtensionMenuItemView* installed_item);
  void ClickContextMenuButton(InstalledExtensionMenuItemView* installed_item);
  void SelectSiteAccessInCombobox(SiteAccessMenuItemView* site_access_item,
                                  int index);
  void SelectSiteSetting(int index);

  void LayoutMenuIfNecessary() {
    extensions_tabbed_menu()->GetWidget()->LayoutRootViewIfNecessary();
  }

  // ExtensionsToolbarUnitTest:
  void SetUp() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::WebContentsTester* web_contents_tester_;
};

ExtensionsTabbedMenuViewUnitTest::ExtensionsTabbedMenuViewUnitTest() {
  scoped_feature_list_.InitAndEnableFeature(
      features::kExtensionsMenuAccessControl);
}

void ExtensionsTabbedMenuViewUnitTest::SetUp() {
  ExtensionsToolbarUnitTest::SetUp();
  // Menu needs web contents at construction, so we need to add them to every
  // test.
  web_contents_tester_ = AddWebContentsAndGetTester();
}

InstalledExtensionMenuItemView*
ExtensionsTabbedMenuViewUnitTest::GetOnlyInstalledMenuItem() {
  std::vector<InstalledExtensionMenuItemView*> items = installed_items();
  if (items.size() != 1u) {
    ADD_FAILURE() << "Not exactly one item; size is: " << items.size();
    return nullptr;
  }
  return *items.begin();
}

SiteAccessMenuItemView*
ExtensionsTabbedMenuViewUnitTest::GetOnlyHasAccessMenuItem() {
  std::vector<SiteAccessMenuItemView*> items = has_access_items();
  if (items.size() != 1u) {
    ADD_FAILURE() << "Not exactly one item; size is: " << items.size();
    return nullptr;
  }
  return *items.begin();
}

SiteAccessMenuItemView*
ExtensionsTabbedMenuViewUnitTest::GetOnlyRequestsAccessMenuItem() {
  std::vector<SiteAccessMenuItemView*> items = requests_access_items();
  if (items.size() != 1u) {
    ADD_FAILURE() << "Not exactly one item; size is: " << items.size();
    return nullptr;
  }
  return *items.begin();
}

bool ExtensionsTabbedMenuViewUnitTest::IsHasAccessSectionDisplayed() {
  return has_access_items().size() != 0;
}

bool ExtensionsTabbedMenuViewUnitTest::IsRequestsAccessSectionDisplayed() {
  return requests_access_items().size() != 0;
}

void ExtensionsTabbedMenuViewUnitTest::ShowInstalledTabInMenu() {
  ExtensionsTabbedMenuView::ShowBubble(
      extensions_button(), browser(), extensions_container(),
      ExtensionsToolbarButton::ButtonType::kExtensions, true);
}

void ExtensionsTabbedMenuViewUnitTest::ShowSiteAccessTabInMenu() {
  ExtensionsTabbedMenuView::ShowBubble(
      extensions_button(), browser(), extensions_container(),
      ExtensionsToolbarButton::ButtonType::kSiteAccess, true);
}

void ExtensionsTabbedMenuViewUnitTest::ClickSiteAccessButton() {
  ClickButton(site_access_button());
  LayoutContainerIfNecessary();
}

void ExtensionsTabbedMenuViewUnitTest::ClickExtensionsButton() {
  ClickButton(extensions_button());
  LayoutContainerIfNecessary();
}

void ExtensionsTabbedMenuViewUnitTest::ClickPrimaryActionButton(
    InstalledExtensionMenuItemView* item) {
  ClickButton(item->primary_action_button_for_testing());
  WaitForAnimation();
}

void ExtensionsTabbedMenuViewUnitTest::ClickPrimaryActionButton(
    SiteAccessMenuItemView* item) {
  ClickButton(item->primary_action_button_for_testing());
  WaitForAnimation();
}

void ExtensionsTabbedMenuViewUnitTest::ClickPinButton(
    InstalledExtensionMenuItemView* installed_item) {
  ClickButton(installed_item->pin_button_for_testing());
  WaitForAnimation();
}

void ExtensionsTabbedMenuViewUnitTest::ClickContextMenuButton(
    InstalledExtensionMenuItemView* installed_item) {
  ClickButton(installed_item->context_menu_button_for_testing());
}

void ExtensionsTabbedMenuViewUnitTest::SelectSiteAccessInCombobox(
    SiteAccessMenuItemView* site_access_item,
    int index) {
  content::WindowedNotificationObserver permissions_observer(
      extensions::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
      content::NotificationService::AllSources());
  site_access_item->site_access_combobox_for_testing()->SetSelectedRow(index);
  permissions_observer.Wait();
  LayoutMenuIfNecessary();
}

void ExtensionsTabbedMenuViewUnitTest::SelectSiteSetting(int index) {
  auto* site_setting = static_cast<views::RadioButton*>(
      extensions_tabbed_menu()->GetSiteSettingsForTesting()->children().at(
          index));

  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, gfx::PointF(),
                               gfx::PointF(), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  site_setting->NotifyClick(release_event);
}

TEST_F(ExtensionsTabbedMenuViewUnitTest, ButtonOpensAndClosesCorrespondingTab) {
  // Load an extension with all urls permissions so the site access button is
  // visible.
  InstallExtensionWithHostPermissions("all_urls", {"<all_urls>"});

  // Navigate to an url where the extension should have access to.
  const GURL url("http://www.a.com");
  web_contents_tester()->NavigateAndCommit(url);
  WaitForAnimation();
  EXPECT_TRUE(site_access_button()->GetVisible());
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the extensions button when the menu is closed. Extensions menu
  // should open in the installed extensions tab.
  ClickExtensionsButton();
  EXPECT_TRUE(ExtensionsTabbedMenuView::IsShowing());
  EXPECT_EQ(extensions_tabbed_menu()->GetSelectedTabIndex(), 1u);

  // Click on the extensions button when the menu is open. Extensions menu
  // should be closed.
  ClickExtensionsButton();
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the site access button when the menu is closed. Extensions menu
  // should open in the site access tab.
  ClickSiteAccessButton();
  EXPECT_TRUE(ExtensionsTabbedMenuView::IsShowing());
  EXPECT_EQ(extensions_tabbed_menu()->GetSelectedTabIndex(), 0u);

  // Click on the site access button when the menu is open. Extensions menu
  // should close.
  ClickSiteAccessButton();
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());
}

TEST_F(ExtensionsTabbedMenuViewUnitTest, TogglingButtonsClosesMenu) {
  // Load an extension with all urls permissions so the site access button is
  // visible.
  InstallExtensionWithHostPermissions("all_urls", {"<all_urls>"});

  // Navigate to an url where the extension should have access to.
  const GURL url("http://www.a.com");
  web_contents_tester()->NavigateAndCommit(url);
  WaitForAnimation();
  EXPECT_TRUE(site_access_button()->GetVisible());
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the extensions button when the menu is closed. Extensions menu
  // should open.
  ClickExtensionsButton();
  EXPECT_TRUE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the site access button when the menu is open. Extensions menu
  // should close since the button click is treated as a click outside the menu,
  // and therefore closing the menu, instead of triggering the button's click
  // action.
  // TODO(crbug.com/1263311): Toggle to the corresponding tab when clicking on
  // the other control when the menu is open.
  ClickSiteAccessButton();
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the site access button when the menu is closed. Extensions menu
  // should open.
  ClickSiteAccessButton();
  EXPECT_TRUE(ExtensionsTabbedMenuView::IsShowing());

  // Click on the extensions button when the menu is open. Extensions menu
  // should close, as explained previously.
  ClickExtensionsButton();
  EXPECT_FALSE(ExtensionsTabbedMenuView::IsShowing());
}

TEST_F(ExtensionsTabbedMenuViewUnitTest,
       InstalledTab_InstalledExtensionsAreShownInInstalledTab) {
  // To start, there should be no extensions in the menu.
  EXPECT_EQ(installed_items().size(), 0u);

  // Add an extension, and verify that it's added to the menu.
  constexpr char kExtensionName[] = "Test 1";
  InstallExtension(kExtensionName);

  ShowInstalledTabInMenu();

  ASSERT_EQ(installed_items().size(), 1u);
  EXPECT_EQ(base::UTF16ToUTF8((*installed_items().begin())
                                  ->primary_action_button_for_testing()
                                  ->label_text_for_testing()),
            kExtensionName);
}

TEST_F(ExtensionsTabbedMenuViewUnitTest,
       InstalledTab_InstalledExtensionsAreSorted) {
  constexpr char kExtensionZName[] = "Z Extension";
  InstallExtension(kExtensionZName);
  constexpr char kExtensionAName[] = "A Extension";
  InstallExtension(kExtensionAName);
  constexpr char kExtensionBName[] = "b Extension";
  InstallExtension(kExtensionBName);
  constexpr char kExtensionCName[] = "C Extension";
  InstallExtension(kExtensionCName);

  ShowInstalledTabInMenu();

  std::vector<InstalledExtensionMenuItemView*> items = installed_items();
  ASSERT_EQ(items.size(), 4u);

  // Basic std::sort would do A,C,Z,b however we want A,b,C,Z
  std::vector<std::string> expected_items{kExtensionAName, kExtensionBName,
                                          kExtensionCName, kExtensionZName};
  EXPECT_EQ(GetNamesFromMenuItems(items), expected_items);
}

TEST_F(ExtensionsTabbedMenuViewUnitTest,
       InstalledTab_PinnedExtensionAppearsInToolbar) {
  constexpr char kName[] = "Test Name";
  InstallExtension(kName);

  ShowInstalledTabInMenu();

  InstalledExtensionMenuItemView* installed_item = GetOnlyInstalledMenuItem();
  ASSERT_TRUE(installed_item);
  ToolbarActionViewController* controller = installed_item->view_controller();
  EXPECT_FALSE(extensions_container()->IsActionVisibleOnToolbar(controller));
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());

  ClickPinButton(installed_item);

  EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(controller));
  EXPECT_EQ(GetPinnedExtensionNames(), std::vector<std::string>{kName});

  ClickPinButton(installed_item);  // Unpin.

  EXPECT_FALSE(extensions_container()->IsActionVisibleOnToolbar(
      installed_item->view_controller()));
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());
}

TEST_F(ExtensionsTabbedMenuViewUnitTest,
       InstalledTab_NewPinnedExtensionAppearsToTheRightOfPinnedExtensions) {
  constexpr char kExtensionA[] = "A Extension";
  InstallExtension(kExtensionA);
  constexpr char kExtensionB[] = "B Extension";
  InstallExtension(kExtensionB);
  constexpr char kExtensionC[] = "C Extension";
  InstallExtension(kExtensionC);

  ShowInstalledTabInMenu();

  std::vector<InstalledExtensionMenuItemView*> items = installed_items();

  // Verify the order of the extensions is A,B,C.
  {
    EXPECT_EQ(items.size(), 3u);
    std::vector<std::string> expected_items{kExtensionA, kExtensionB,
                                            kExtensionC};
    EXPECT_EQ(GetNamesFromMenuItems(items), expected_items);
  }

  // Pinning an extension should add it to the toolbar.
  {
    ClickPinButton(items.at(0));
    std::vector<std::string> expected_names{kExtensionA};
    EXPECT_EQ(GetPinnedExtensionNames(), expected_names);
  }

  // Pinning a second extension should add it to the right of the current pinned
  // extensions.

  {
    ClickPinButton(items.at(1));
    std::vector<std::string> expected_names{kExtensionA, kExtensionB};
    EXPECT_EQ(GetPinnedExtensionNames(), expected_names);
  }

  // Pinning a third extension should add it to the right of the current pinned
  // extensions.
  {
    ClickPinButton(items.at(2));
    std::vector<std::string> expected_names{kExtensionA, kExtensionB,
                                            kExtensionC};
    EXPECT_EQ(GetPinnedExtensionNames(), expected_names);
  }

  // Unpinning the middle extension should remove it from the toolbar without
  // affecting the order of the other pinned extensions.
  {
    ClickPinButton(items.at(1));
    std::vector<std::string> expected_names{kExtensionA, kExtensionC};
    EXPECT_EQ(GetPinnedExtensionNames(), expected_names);
  }

  // Pinning an extension should add it to the right of the current pinned
  // extensions, even if it was pinned and unpinned previously.
  {
    ClickPinButton(items.at(1));
    std::vector<std::string> expected_names{kExtensionA, kExtensionC,
                                            kExtensionB};
    EXPECT_EQ(GetPinnedExtensionNames(), expected_names);
  }
}

TEST_F(ExtensionsTabbedMenuViewUnitTest,
       InstalledTab_PinnedExtensionAppearsInAnotherWindow) {
  InstallExtension("Test Name");

  ShowInstalledTabInMenu();

  AdditionalBrowser browser2(
      CreateBrowser(browser()->profile(), browser()->type(),
                    /* hosted_app */ false, /* browser_window */ nullptr));

  InstalledExtensionMenuItemView* installed_item = GetOnlyInstalledMenuItem();
  ASSERT_TRUE(installed_item);
  ClickPinButton(installed_item);

  // Window that was already open gets the pinned extension.
  EXPECT_TRUE(browser2.extensions_container()->IsActionVisibleOnToolbar(
      installed_item->view_controller()));

  AdditionalBrowser browser3(
      CreateBrowser(browser()->profile(), browser()->type(),
                    /* hosted_app */ false, /* browser_window */ nullptr));

  // Brand-new window also gets the pinned extension.
  EXPECT_TRUE(browser3.extensions_container()->IsActionVisibleOnToolbar(
      installed_item->view_controller()));
}

// TODO(crbug.com/1304959): Test is flaky.
TEST_F(ExtensionsTabbedMenuViewUnitTest,
       InstalledTab_AddAndRemoveExtensionWhenMenuIsOpen) {
  constexpr char kExtensionA[] = "A Extension";
  constexpr char kExtensionC[] = "C Extension";
  InstallExtension(kExtensionA);
  InstallExtension(kExtensionC);

  ShowInstalledTabInMenu();

  // Verify the order of the extensions is A,C.
  {
    std::vector<InstalledExtensionMenuItemView*> items = installed_items();
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
    std::vector<InstalledExtensionMenuItemView*> items = installed_items();
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
    std::vector<InstalledExtensionMenuItemView*> items = installed_items();
    ASSERT_EQ(items.size(), 2u);
    std::vector<std::string> expected_names{kExtensionA, kExtensionC};
    EXPECT_EQ(GetNamesFromMenuItems(items), expected_names);
  }
}

TEST_F(ExtensionsTabbedMenuViewUnitTest,
       InstalledTab_DisableAndEnableExtension) {
  constexpr char kName[] = "Test Extension";
  auto extension_id = InstallExtension(kName)->id();

  ShowInstalledTabInMenu();

  InstalledExtensionMenuItemView* menu_item = GetOnlyInstalledMenuItem();
  EXPECT_EQ(installed_items().size(), 1u);
  ClickPinButton(menu_item);

  DisableExtension(extension_id);
  LayoutMenuIfNecessary();
  WaitForAnimation();

  EXPECT_EQ(installed_items().size(), 0u);
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());

  EnableExtension(extension_id);
  LayoutMenuIfNecessary();
  WaitForAnimation();

  EXPECT_EQ(installed_items().size(), 1u);
  EXPECT_EQ(GetPinnedExtensionNames(), std::vector<std::string>{kName});
}

// Tests that when an extension is reloaded it remains visible in the toolbar
// and extensions menu.
TEST_F(ExtensionsTabbedMenuViewUnitTest, InstalledTab_ReloadExtension) {
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

  ShowInstalledTabInMenu();

  InstalledExtensionMenuItemView* installed_item = GetOnlyInstalledMenuItem();
  EXPECT_EQ(installed_items().size(), 1u);

  ClickPinButton(installed_item);
  EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(
      installed_item->view_controller()));

  // Reload the extension.
  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile()));
  ReloadExtension(extension->id());
  ASSERT_TRUE(registry_observer.WaitForExtensionLoaded());
  LayoutMenuIfNecessary();

  // Verify the extension is visible in the menu and on the toolbar.
  installed_item = GetOnlyInstalledMenuItem();
  EXPECT_EQ(installed_items().size(), 1u);
  EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(
      installed_item->view_controller()));
}

// Tests that a when an extension is reloaded with manifest errors, and
// therefore fails to be loaded into Chrome, it's removed from the toolbar and
// extensions menu.
TEST_F(ExtensionsTabbedMenuViewUnitTest, InstalledTab_ReloadExtensionFailed) {
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

  ShowInstalledTabInMenu();

  InstalledExtensionMenuItemView* installed_item = GetOnlyInstalledMenuItem();
  EXPECT_EQ(installed_items().size(), 1u);

  ClickPinButton(installed_item);
  EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(
      installed_item->view_controller()));

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

  // Verify the extension is no longer visible in the menu or on the toolbar
  // since it was removed.
  EXPECT_EQ(installed_items().size(), 0u);
  for (views::View* child : extensions_container()->children())
    EXPECT_FALSE(views::IsViewClass<ToolbarActionView>(child));
}

TEST_F(ExtensionsTabbedMenuViewUnitTest,
       InstalledTab_DiscoverMoreButtonOpenWebstorePage) {
  InstallExtension("Test Extension");

  ShowInstalledTabInMenu();
  EXPECT_TRUE(ExtensionsTabbedMenuView::IsShowing());

  ClickButton(extensions_tabbed_menu()->GetDiscoverMoreButtonForTesting());

  EXPECT_EQ(
      extension_urls::GetWebstoreLaunchURL(),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

TEST_F(ExtensionsTabbedMenuViewUnitTest, SiteAccessTab_NoExtensionsHaveAccess) {
  InstallExtension("Test Extension A");
  InstallExtension("Test Extension B");

  const GURL url("http://www.url.com");
  web_contents_tester()->NavigateAndCommit(url);
  ShowSiteAccessTabInMenu();

  auto no_extensions_have_access_text = l10n_util::GetStringFUTF16(
      IDS_EXTENSIONS_MENU_SITE_ACCESS_TAB_NO_EXTENSIONS_HAVE_ACCESS_TEXT,
      base::UTF8ToUTF16(url.host()));

  // Verify only the correct message is displayed when no extensions have access
  // to the current site.
  EXPECT_TRUE(site_access_message()->GetVisible());
  EXPECT_EQ(site_access_message()->GetText(), no_extensions_have_access_text);
  EXPECT_FALSE(IsHasAccessSectionDisplayed());
  EXPECT_FALSE(IsRequestsAccessSectionDisplayed());
}

TEST_F(ExtensionsTabbedMenuViewUnitTest,
       SiteAccessTab_ExtensionsInCorrectSiteAccessSection) {
  constexpr char kHasAccessName[] = "Has Access Extension";
  InstallExtensionWithHostPermissions(kHasAccessName, {"<all_urls>"});
  constexpr char kNoAccessName[] = "No Access Extension";
  InstallExtension(kNoAccessName);

  const GURL url_a("http://www.a.com");
  web_contents_tester()->NavigateAndCommit(url_a);
  ShowSiteAccessTabInMenu();

  // Site access message should not be displayed since there is at least one
  // extension with host permissions.
  EXPECT_FALSE(site_access_message()->GetVisible());

  // Extension with <all_urls> permission has site access by default (except for
  // forbidden websites such as chrome:-scheme), and it should be in the has
  // access section.
  ASSERT_EQ(has_access_items().size(), 1u);
  EXPECT_EQ(base::UTF16ToUTF8(GetOnlyHasAccessMenuItem()
                                  ->primary_action_button_for_testing()
                                  ->label_text_for_testing()),
            kHasAccessName);
  // Extension with no host permissions does not have site access, and it should
  // not be in any site access section.
  EXPECT_EQ(requests_access_items().size(), 0u);
}

// TODO(crbug.com/1304951): Test is flaky.
TEST_F(
    ExtensionsTabbedMenuViewUnitTest,
    SiteAccessTab_ExtensionInCorrectSiteAccessSectionAfterChangingSiteAccessUsingCombobox) {
  constexpr char kExtensionName[] = "Has Access Extension";
  InstallExtensionWithHostPermissions(kExtensionName, {"<all_urls>"});

  const GURL url_a("http://www.a.com");
  web_contents_tester()->NavigateAndCommit(url_a);
  ShowSiteAccessTabInMenu();

  // Verify extension is in the "has access" section with "on all sites" access.
  ASSERT_EQ(has_access_items().size(), 1u);
  ASSERT_EQ(requests_access_items().size(), 0u);
  EXPECT_EQ(GetOnlyHasAccessMenuItem()
                ->site_access_combobox_for_testing()
                ->GetSelectedIndex(),
            kOnAllSitesComboboxIndex);

  // Change extension's site access to run "on site" using the combobox.
  SelectSiteAccessInCombobox(GetOnlyHasAccessMenuItem(), kOnSiteComboboxIndex);

  // Verify extension is in the "has access" section with "on site" access.
  ASSERT_EQ(has_access_items().size(), 1u);
  ASSERT_EQ(requests_access_items().size(), 0u);
  EXPECT_EQ(GetOnlyHasAccessMenuItem()
                ->site_access_combobox_for_testing()
                ->GetSelectedIndex(),
            kOnSiteComboboxIndex);

  // Change extension's site access to run "on click" using the combobox.
  SelectSiteAccessInCombobox(GetOnlyHasAccessMenuItem(), kOnClickComboboxIndex);

  // Verify extension is in the "requests access" section with "on click"
  // access.
  ASSERT_EQ(has_access_items().size(), 0u);
  ASSERT_EQ(requests_access_items().size(), 1u);
  EXPECT_EQ(GetOnlyRequestsAccessMenuItem()
                ->site_access_combobox_for_testing()
                ->GetSelectedIndex(),
            kOnClickComboboxIndex);
}

// TODO(crbug.com/1304951): Test is flaky.
TEST_F(
    ExtensionsTabbedMenuViewUnitTest,
    SiteAccessTab_ExtensionInCorrectSiteAccessSectionAfterChangingSiteAccessUsingContextMenu) {
  constexpr char kExtensionName[] = "Has Access Extension";
  auto extension =
      InstallExtensionWithHostPermissions(kExtensionName, {"<all_urls>"});

  const GURL url_a("http://www.a.com");
  web_contents_tester()->NavigateAndCommit(url_a);
  ShowSiteAccessTabInMenu();

  extensions::ExtensionContextMenuModel menu(
      extension.get(), browser(), extensions::ExtensionContextMenuModel::PINNED,
      nullptr, true,
      extensions::ExtensionContextMenuModel::ContextMenuSource::kToolbarAction);

  // Verify extension is in the "has access" section with "on all sites" access.
  ASSERT_EQ(has_access_items().size(), 1u);
  ASSERT_EQ(requests_access_items().size(), 0u);
  EXPECT_EQ(GetOnlyHasAccessMenuItem()
                ->site_access_combobox_for_testing()
                ->GetSelectedIndex(),
            kOnAllSitesComboboxIndex);

  // Change extension's site access to run "on site" using the context menu.
  {
    content::WindowedNotificationObserver permissions_observer(
        extensions::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
        content::NotificationService::AllSources());
    menu.ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE, 0);
    permissions_observer.Wait();
    LayoutMenuIfNecessary();
  }

  // Verify extension is in the "has access" section with "on site" access.
  ASSERT_EQ(has_access_items().size(), 1u);
  ASSERT_EQ(requests_access_items().size(), 0u);
  EXPECT_EQ(GetOnlyHasAccessMenuItem()
                ->site_access_combobox_for_testing()
                ->GetSelectedIndex(),
            kOnSiteComboboxIndex);

  // Change extension's site access to run "on click" using the context menu.
  {
    content::WindowedNotificationObserver permissions_observer(
        extensions::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
        content::NotificationService::AllSources());
    menu.ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_CLICK, 0);
    permissions_observer.Wait();
    LayoutMenuIfNecessary();
  }

  // Verify extension is in the "requests access" section with "on click"
  // access.
  ASSERT_EQ(has_access_items().size(), 0u);
  ASSERT_EQ(requests_access_items().size(), 1u);
  EXPECT_EQ(GetOnlyRequestsAccessMenuItem()
                ->site_access_combobox_for_testing()
                ->GetSelectedIndex(),
            kOnClickComboboxIndex);
}

// TODO(crbug.com/1304951): Test is flaky.
TEST_F(
    ExtensionsTabbedMenuViewUnitTest,
    SiteAccessTab_ExtensionsInCorrectSiteAccessSectionAfterClickingOnAction) {
  constexpr char kExtensionName[] = "Has Access Extension";
  InstallExtensionWithHostPermissions(kExtensionName, {"<all_urls>"});

  const GURL url_a("http://www.a.com");
  web_contents_tester()->NavigateAndCommit(url_a);
  ShowSiteAccessTabInMenu();

  // Change extension's site access to run "on click" using the combobox. By
  // default, extension has site access.
  SelectSiteAccessInCombobox(GetOnlyHasAccessMenuItem(), kOnClickComboboxIndex);

  // Verify extension is in the "requests access" section with "on click"
  // access.
  ASSERT_EQ(has_access_items().size(), 0u);
  ASSERT_EQ(requests_access_items().size(), 1u);
  EXPECT_EQ(GetOnlyRequestsAccessMenuItem()
                ->site_access_combobox_for_testing()
                ->GetSelectedIndex(),
            kOnClickComboboxIndex);

  // Run extensions action by clicking on it.
  content::WindowedNotificationObserver permissions_observer(
      extensions::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
      content::NotificationService::AllSources());
  ClickPrimaryActionButton(GetOnlyRequestsAccessMenuItem());
  permissions_observer.Wait();
  LayoutMenuIfNecessary();

  // Verify extension is in the "has access" section with "on click" access.
  ASSERT_EQ(has_access_items().size(), 1u);
  ASSERT_EQ(requests_access_items().size(), 0u);
  EXPECT_EQ(GetOnlyHasAccessMenuItem()
                ->site_access_combobox_for_testing()
                ->GetSelectedIndex(),
            kOnClickComboboxIndex);
}

// TODO(crbug.com/1304959): Test is flaky.
TEST_F(ExtensionsTabbedMenuViewUnitTest,
       SiteAccessTab_AddAndRemoveExtensionWhenMenuIsOpen) {
  constexpr char kExtensionA[] = "A Extension";
  constexpr char kExtensionC[] = "C Extension";
  InstallExtensionWithHostPermissions(kExtensionA, {"<all_urls>"});
  InstallExtensionWithHostPermissions(kExtensionC, {"<all_urls>"});

  const GURL url_a("http://www.a.com");
  web_contents_tester()->NavigateAndCommit(url_a);
  ShowSiteAccessTabInMenu();

  // Verify the order of the extensions is A,C under the has access section.
  // Note that extensions installed with all urls permissions have access by
  // default.
  {
    std::vector<SiteAccessMenuItemView*> has_acess_items = has_access_items();
    ASSERT_EQ(has_acess_items.size(), 2u);
    std::vector<std::string> expected_names{kExtensionA, kExtensionC};
    EXPECT_EQ(GetNamesFromSiteAccessMenuItems(has_acess_items), expected_names);
  }

  // Add a new extension while the menu is open.
  constexpr char kExtensionB[] = "B Extension";
  auto extensionB =
      InstallExtensionWithHostPermissions(kExtensionB, {"<all_urls>"});
  LayoutMenuIfNecessary();

  // Verify the new order is A,B,C under the has access section
  {
    std::vector<SiteAccessMenuItemView*> has_acess_items = has_access_items();
    ASSERT_EQ(has_acess_items.size(), 3u);
    std::vector<std::string> expected_names{kExtensionA, kExtensionB,
                                            kExtensionC};
    EXPECT_EQ(GetNamesFromSiteAccessMenuItems(has_acess_items), expected_names);
  }

  // Remove a extension while the menu is open
  UninstallExtension(extensionB->id());
  LayoutMenuIfNecessary();

  // Verify the new order is A,C.
  {
    std::vector<SiteAccessMenuItemView*> has_acess_items = has_access_items();
    ASSERT_EQ(has_acess_items.size(), 2u);
    std::vector<std::string> expected_names{kExtensionA, kExtensionC};
    EXPECT_EQ(GetNamesFromSiteAccessMenuItems(has_acess_items), expected_names);
  }
}

TEST_F(ExtensionsTabbedMenuViewUnitTest,
       SiteAccessTab_TabChangesWithExtensionMenuOpen) {
  constexpr char kExtension[] = "Test Extension";
  const GURL url_a("http://www.a.com");
  const GURL url_b("http://www.b.com");
  const GURL url_c("http://www.c.com");

  InstallExtensionWithHostPermissions(
      kExtension, {url_a.spec(), url_b.spec(), url_c.spec()});
  ShowSiteAccessTabInMenu();

  // Navigate to a url where the extension does not want access.
  const GURL url_no_access("http://www.noaccess.com");
  web_contents_tester()->NavigateAndCommit(url_no_access);
  LayoutMenuIfNecessary();
  ASSERT_TRUE(ExtensionsTabbedMenuView::IsShowing());

  // Verify site access sections are empty.
  {
    EXPECT_THAT(GetNamesFromSiteAccessMenuItems(has_access_items()),
                testing::IsEmpty());
    EXPECT_THAT(GetNamesFromSiteAccessMenuItems(requests_access_items()),
                testing::IsEmpty());
  }

  // Navigate to a url where the extension wants access.
  web_contents_tester()->NavigateAndCommit(url_a);
  LayoutMenuIfNecessary();
  ASSERT_TRUE(ExtensionsTabbedMenuView::IsShowing());

  // Verify the extension is in the "has access" section with "on site"
  // access.
  {
    EXPECT_THAT(GetNamesFromSiteAccessMenuItems(has_access_items()),
                testing::ElementsAre(kExtension));
    EXPECT_THAT(GetNamesFromSiteAccessMenuItems(requests_access_items()),
                testing::IsEmpty());
    EXPECT_THAT(GetOnlyHasAccessMenuItem()
                    ->site_access_combobox_for_testing()
                    ->GetSelectedIndex(),
                kOnSiteComboboxIndex);
  }

  // Navigate to a url where the extension keeps its current access.
  web_contents_tester()->NavigateAndCommit(url_b);
  LayoutMenuIfNecessary();
  ASSERT_TRUE(ExtensionsTabbedMenuView::IsShowing());

  // Verify the extension is still in "has access" section with "on site"
  // access.
  {
    EXPECT_THAT(GetNamesFromSiteAccessMenuItems(has_access_items()),
                testing::ElementsAre(kExtension));
    EXPECT_THAT(GetNamesFromSiteAccessMenuItems(requests_access_items()),
                testing::IsEmpty());
    EXPECT_EQ(GetOnlyHasAccessMenuItem()
                  ->site_access_combobox_for_testing()
                  ->GetSelectedIndex(),
              kOnSiteComboboxIndex);
  }

  // Navigate to a url where the extension can't access.
  const GURL chrome_url("chrome://extensions");
  web_contents_tester()->NavigateAndCommit(chrome_url);
  LayoutMenuIfNecessary();
  ASSERT_TRUE(ExtensionsTabbedMenuView::IsShowing());

  // Verify site access sections are empty.
  {
    EXPECT_THAT(GetNamesFromSiteAccessMenuItems(has_access_items()),
                testing::IsEmpty());
    EXPECT_THAT(GetNamesFromSiteAccessMenuItems(requests_access_items()),
                testing::IsEmpty());
  }
}

TEST_F(ExtensionsTabbedMenuViewUnitTest,
       SiteAccessTab_SiteSettingsButtonOpensSiteSettingsView) {
  // Load an extension with all urls permissions so the site access settings can
  // be accessed.
  InstallExtensionWithHostPermissions("all_urls", {"<all_urls>"});

  // Navigate to a url where the extension should have access to, and open the
  // site access tab.
  const GURL url("http://www.a.com");
  web_contents_tester()->NavigateAndCommit(url);
  WaitForAnimation();
  ShowSiteAccessTabInMenu();

  // Verify the site settings are hidden by default
  auto* site_settings = extensions_tabbed_menu()->GetSiteSettingsForTesting();
  EXPECT_FALSE(site_settings->GetVisible());

  // Verify clicking the site settings button displays the site settings.
  ClickButton(extensions_tabbed_menu()->GetSiteSettingsButtonForTesting());
  EXPECT_TRUE(site_settings->GetVisible());

  // Verify clicking again the site settings button hides the site settings.
  ClickButton(extensions_tabbed_menu()->GetSiteSettingsButtonForTesting());
  EXPECT_FALSE(site_settings->GetVisible());
}

TEST_F(ExtensionsTabbedMenuViewUnitTest, SiteAccessTab_SelectSiteSetting) {
  auto extensionA =
      InstallExtensionWithHostPermissions("Extension A", {"<all_urls>"});
  auto extensionB =
      InstallExtensionWithHostPermissions("Extension B", {"<all_urls>"});

  // Navigate to a url where the extension should have access to.
  const GURL url("http://www.a.com");
  web_contents_tester()->NavigateAndCommit(url);
  WaitForAnimation();
  ShowSiteAccessTabInMenu();

  auto* manager = extensions::PermissionsManager::Get(profile());
  auto origin = url::Origin::Create(url);

  // Verify site has "customize by extensions" site setting by default.
  EXPECT_EQ(
      manager->GetUserSiteSetting(origin),
      extensions::PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  // TODO(crbug.com/1263310): After adding a permissions manager observer in the
  // menu, test the site access tab contents view is the appropriate instead of
  // checking the user site setting directly.

  SelectSiteSetting(kGrantAllExtensionsIndex);
  EXPECT_EQ(
      manager->GetUserSiteSetting(origin),
      extensions::PermissionsManager::UserSiteSetting::kGrantAllExtensions);

  SelectSiteSetting(kBlockAllExtensionsIndex);
  EXPECT_EQ(
      manager->GetUserSiteSetting(origin),
      extensions::PermissionsManager::UserSiteSetting::kBlockAllExtensions);

  SelectSiteSetting(kCustomizeByExtensionIndex);
  EXPECT_EQ(
      manager->GetUserSiteSetting(origin),
      extensions::PermissionsManager::UserSiteSetting::kCustomizeByExtension);
}

TEST_F(ExtensionsTabbedMenuViewUnitTest, WindowTitle) {
  InstallExtension("Test Extension");

  ShowInstalledTabInMenu();

  ExtensionsTabbedMenuView* menu = extensions_tabbed_menu();
  EXPECT_FALSE(menu->GetWindowTitle().empty());
  EXPECT_TRUE(menu->GetAccessibleWindowTitle().empty());
}
