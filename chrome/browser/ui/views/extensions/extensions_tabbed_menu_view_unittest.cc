// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_tabbed_menu_view.h"

#include "base/feature_list.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/views/view_utils.h"

namespace {

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

std::vector<std::string> GetNamesFromInstalledItems(
    std::vector<ExtensionsMenuItemView*> installed_items) {
  std::vector<std::string> names;
  names.resize(installed_items.size());
  std::transform(
      installed_items.begin(), installed_items.end(), names.begin(),
      [](ExtensionsMenuItemView* item) {
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
  std::vector<ExtensionsMenuItemView*> installed_items() {
    return ExtensionsTabbedMenuView::GetExtensionsTabbedMenuViewForTesting()
        ->GetInstalledItemsForTesting();
  }

  // Asserts there is exactly 1 menu item and then returns it.
  ExtensionsMenuItemView* GetOnlyInstalledMenuItem();

  void ShowTabbedMenu();

  void ClickSiteAccessButton();
  void ClickExtensionsButton();
  void ClickPinButton(ExtensionsMenuItemView* installed_item);
  void ClickContextMenuButton(ExtensionsMenuItemView* installed_item);

  void LayoutMenuIfNecessary() {
    extensions_tabbed_menu()->GetWidget()->LayoutRootViewIfNecessary();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

ExtensionsTabbedMenuViewUnitTest::ExtensionsTabbedMenuViewUnitTest() {
  scoped_feature_list_.InitAndEnableFeature(
      features::kExtensionsMenuAccessControl);
}

ExtensionsMenuItemView*
ExtensionsTabbedMenuViewUnitTest::GetOnlyInstalledMenuItem() {
  std::vector<ExtensionsMenuItemView*> items = installed_items();
  if (items.size() != 1u) {
    ADD_FAILURE() << "Not exactly one item; size is: " << items.size();
    return nullptr;
  }
  return *items.begin();
}

void ExtensionsTabbedMenuViewUnitTest::ShowTabbedMenu() {
  ExtensionsTabbedMenuView::ShowBubble(
      extensions_button(), browser(), extensions_container(),
      ExtensionsToolbarButton::ButtonType::kExtensions, true);
}

void ExtensionsTabbedMenuViewUnitTest::ClickSiteAccessButton() {
  ClickButton(site_access_button());
  LayoutContainerIfNecessary();
}

void ExtensionsTabbedMenuViewUnitTest::ClickExtensionsButton() {
  ClickButton(extensions_button());
  LayoutContainerIfNecessary();
}

void ExtensionsTabbedMenuViewUnitTest::ClickPinButton(
    ExtensionsMenuItemView* installed_item) {
  ClickButton(installed_item->pin_button_for_testing());
  WaitForAnimation();
}

void ExtensionsTabbedMenuViewUnitTest::ClickContextMenuButton(
    ExtensionsMenuItemView* installed_item) {
  ClickButton(installed_item->context_menu_button_for_testing());
}

TEST_F(ExtensionsTabbedMenuViewUnitTest, ButtonOpensAndClosesCorrespondingTab) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();

  // Load an extension with all urls permissions so the site access button is
  // visible.
  InstallExtensionWithHostPermissions("all_urls", {"<all_urls>"});

  // Navigate to an url where the extension should have access to.
  const GURL url("http://www.a.com");
  web_contents_tester->NavigateAndCommit(url);
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
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();

  // Load an extension with all urls permissions so the site access button is
  // visible.
  InstallExtensionWithHostPermissions("all_urls", {"<all_urls>"});

  // Navigate to an url where the extension should have access to.
  const GURL url("http://www.a.com");
  web_contents_tester->NavigateAndCommit(url);
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

  ShowTabbedMenu();

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

  ShowTabbedMenu();

  std::vector<ExtensionsMenuItemView*> items = installed_items();
  ASSERT_EQ(items.size(), 4u);

  // Basic std::sort would do A,C,Z,b however we want A,b,C,Z
  std::vector<std::string> expected_items{kExtensionAName, kExtensionBName,
                                          kExtensionCName, kExtensionZName};
  EXPECT_EQ(GetNamesFromInstalledItems(items), expected_items);
}

TEST_F(ExtensionsTabbedMenuViewUnitTest,
       InstalledTab_PinnedExtensionAppearsInToolbar) {
  constexpr char kName[] = "Test Name";
  InstallExtension(kName);

  ShowTabbedMenu();

  ExtensionsMenuItemView* installed_item = GetOnlyInstalledMenuItem();
  ASSERT_TRUE(installed_item);
  ToolbarActionViewController* controller = installed_item->view_controller();
  EXPECT_FALSE(extensions_container()->IsActionVisibleOnToolbar(controller));
  EXPECT_EQ(GetPinnedExtensionNames(), std::vector<std::string>{});

  ClickPinButton(installed_item);

  EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(controller));
  EXPECT_EQ(GetPinnedExtensionNames(), std::vector<std::string>{kName});

  ClickPinButton(installed_item);  // Unpin.

  EXPECT_FALSE(extensions_container()->IsActionVisibleOnToolbar(
      installed_item->view_controller()));
  EXPECT_EQ(GetPinnedExtensionNames(), std::vector<std::string>{});
}

TEST_F(ExtensionsTabbedMenuViewUnitTest,
       InstalledTab_NewPinnedExtensionAppearsToTheRightOfPinnedExtensions) {
  constexpr char kExtensionA[] = "A Extension";
  InstallExtension(kExtensionA);
  constexpr char kExtensionB[] = "B Extension";
  InstallExtension(kExtensionB);
  constexpr char kExtensionC[] = "C Extension";
  InstallExtension(kExtensionC);

  ShowTabbedMenu();
  std::vector<ExtensionsMenuItemView*> items = installed_items();

  // Verify the order of the extensions is A,B,C.
  {
    EXPECT_EQ(items.size(), 3u);
    std::vector<std::string> expected_items{kExtensionA, kExtensionB,
                                            kExtensionC};
    EXPECT_EQ(GetNamesFromInstalledItems(items), expected_items);
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

  ShowTabbedMenu();

  AdditionalBrowser browser2(
      CreateBrowser(browser()->profile(), browser()->type(),
                    /* hosted_app */ false, /* browser_window */ nullptr));

  ExtensionsMenuItemView* installed_item = GetOnlyInstalledMenuItem();
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

TEST_F(ExtensionsTabbedMenuViewUnitTest,
       InstalledTab_AddAndRemoveExtensionWhenMenuIsOpen) {
  constexpr char kExtensionA[] = "A Extension";
  constexpr char kExtensionC[] = "C Extension";
  InstallExtension(kExtensionA);
  InstallExtension(kExtensionC);

  ShowTabbedMenu();

  // Verify the order of the extensions is A,C.
  {
    std::vector<ExtensionsMenuItemView*> items = installed_items();
    ASSERT_EQ(items.size(), 2u);
    std::vector<std::string> expected_names{kExtensionA, kExtensionC};
    EXPECT_EQ(GetNamesFromInstalledItems(items), expected_names);
  }

  // Add a new extension while the menu is open.
  constexpr char kExtensionB[] = "B Extension";
  auto extensionB = InstallExtension(kExtensionB);
  LayoutMenuIfNecessary();

  // Extension should be added in the correct place.
  // Verify the new order is A,B,C.
  {
    std::vector<ExtensionsMenuItemView*> items = installed_items();
    ASSERT_EQ(items.size(), 3u);
    std::vector<std::string> expected_names{kExtensionA, kExtensionB,
                                            kExtensionC};
    EXPECT_EQ(GetNamesFromInstalledItems(items), expected_names);
  }

  // Remove a extension while the menu is open
  UninstallExtension(extensionB->id());
  LayoutMenuIfNecessary();

  // Verify the new order is A,C.
  {
    std::vector<ExtensionsMenuItemView*> items = installed_items();
    ASSERT_EQ(items.size(), 2u);
    std::vector<std::string> expected_names{kExtensionA, kExtensionC};
    EXPECT_EQ(GetNamesFromInstalledItems(items), expected_names);
  }
}

TEST_F(ExtensionsTabbedMenuViewUnitTest,
       InstalledTab_DisableAndEnableExtension) {
  constexpr char kName[] = "Test Extension";
  auto extension_id = InstallExtension(kName)->id();

  ShowTabbedMenu();

  ExtensionsMenuItemView* menu_item = GetOnlyInstalledMenuItem();
  EXPECT_EQ(installed_items().size(), 1u);
  ClickPinButton(menu_item);

  DisableExtension(extension_id);
  LayoutMenuIfNecessary();
  WaitForAnimation();

  EXPECT_EQ(installed_items().size(), 0u);
  EXPECT_EQ(GetPinnedExtensionNames(), std::vector<std::string>{});

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

  ShowTabbedMenu();

  ExtensionsMenuItemView* installed_item = GetOnlyInstalledMenuItem();
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

  ShowTabbedMenu();

  ExtensionsMenuItemView* installed_item = GetOnlyInstalledMenuItem();
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

TEST_F(ExtensionsTabbedMenuViewUnitTest, WindowTitle) {
  InstallExtension("Test Extension");

  ShowTabbedMenu();

  ExtensionsTabbedMenuView* menu = extensions_tabbed_menu();
  EXPECT_FALSE(menu->GetWindowTitle().empty());
  EXPECT_TRUE(menu->GetAccessibleWindowTitle().empty());
}
