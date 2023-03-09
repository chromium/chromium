// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_site_permissions_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
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
  raw_ptr<BrowserView> browser_view_;
};

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

  void ClickPinButton(ExtensionMenuItemView* menu_item);
  void ClickSitePermissionsButton(ExtensionMenuItemView* menu_item);

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

void ExtensionsMenuMainPageViewUnitTest::ClickPinButton(
    ExtensionMenuItemView* menu_item) {
  ClickButton(menu_item->pin_button_for_testing());
  WaitForAnimation();
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
  return page ? page->GetMenuItemsForTesting()
              : std::vector<ExtensionMenuItemView*>();
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

TEST_F(ExtensionsMenuMainPageViewUnitTest, PinnedExtensionAppearsInToolbar) {
  constexpr char kName[] = "Extension";
  const std::string& extension_id = InstallExtension(kName)->id();

  ShowMenu();

  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_TRUE(menu_item);
  EXPECT_FALSE(extensions_container()->IsActionVisibleOnToolbar(extension_id));
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());

  // Pin.
  ClickPinButton(menu_item);
  EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(extension_id));
  EXPECT_EQ(GetPinnedExtensionNames(), std::vector<std::string>{kName});

  // Unpin.
  ClickPinButton(menu_item);
  EXPECT_FALSE(extensions_container()->IsActionVisibleOnToolbar(extension_id));
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());
}

TEST_F(ExtensionsMenuMainPageViewUnitTest,
       NewPinnedExtensionAppearsToTheRightOfPinnedExtensions) {
  constexpr char kExtensionA[] = "A Extension";
  InstallExtension(kExtensionA);
  constexpr char kExtensionB[] = "B Extension";
  InstallExtension(kExtensionB);
  constexpr char kExtensionC[] = "C Extension";
  InstallExtension(kExtensionC);

  ShowMenu();

  std::vector<ExtensionMenuItemView*> items = menu_items();

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

TEST_F(ExtensionsMenuMainPageViewUnitTest,
       PinnedExtensionAppearsInAnotherWindow) {
  const std::string& extension_id = InstallExtension("Extension")->id();

  ShowMenu();

  AdditionalBrowser browser2(
      CreateBrowser(browser()->profile(), browser()->type(),
                    /* hosted_app */ false, /* browser_window */ nullptr));

  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();
  ASSERT_TRUE(menu_item);
  ClickPinButton(menu_item);

  // Window that was already open gets the pinned extension.
  EXPECT_TRUE(
      browser2.extensions_container()->IsActionVisibleOnToolbar(extension_id));

  AdditionalBrowser browser3(
      CreateBrowser(browser()->profile(), browser()->type(),
                    /* hosted_app */ false, /* browser_window */ nullptr));

  // Brand-new window also gets the pinned extension.
  EXPECT_TRUE(
      browser3.extensions_container()->IsActionVisibleOnToolbar(extension_id));
}

// Verifies the extension site permissions button opens the site permissions
// page corresponding to the extension.
TEST_F(ExtensionsMenuMainPageViewUnitTest,
       SitePermissionsButtonOpensSubpageForCorrectExtension) {
  auto extensionA =
      InstallExtensionWithHostPermissions("Extension A", {"<all_urls>"});
  InstallExtensionWithHostPermissions("Extension B", {"<all_urls>"});

  ShowMenu();

  std::vector<ExtensionMenuItemView*> items = menu_items();
  ASSERT_EQ(items.size(), 2u);
  EXPECT_EQ(items[0]->view_controller()->GetId(), extensionA->id());

  ClickSitePermissionsButton(items[0]);

  ExtensionsMenuSitePermissionsPageView* page = site_permissions_page();
  ASSERT_TRUE(page);
  EXPECT_EQ(page->extension_id(), extensionA->id());
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

TEST_F(ExtensionsMenuMainPageViewUnitTest, DisableAndEnableExtension) {
  constexpr char kName[] = "Test Extension";
  auto extension_id = InstallExtension(kName)->id();

  ShowMenu();

  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();
  EXPECT_EQ(menu_items().size(), 1u);
  ClickPinButton(menu_item);

  DisableExtension(extension_id);
  LayoutMenuIfNecessary();
  WaitForAnimation();

  EXPECT_EQ(menu_items().size(), 0u);
  EXPECT_THAT(GetPinnedExtensionNames(), testing::IsEmpty());

  EnableExtension(extension_id);
  LayoutMenuIfNecessary();
  WaitForAnimation();

  EXPECT_EQ(menu_items().size(), 1u);
  EXPECT_EQ(GetPinnedExtensionNames(), std::vector<std::string>{kName});
}

// Tests that when an extension is reloaded it remains visible in the toolbar
// and extensions menu.
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

  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();
  EXPECT_EQ(menu_items().size(), 1u);

  ClickPinButton(menu_item);
  EXPECT_TRUE(
      extensions_container()->IsActionVisibleOnToolbar(extension->id()));

  // Reload the extension.
  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile()));
  ReloadExtension(extension->id());
  ASSERT_TRUE(registry_observer.WaitForExtensionLoaded());
  LayoutMenuIfNecessary();

  // Verify the extension is visible in the menu and on the toolbar.
  menu_item = GetOnlyMenuItem();
  EXPECT_EQ(menu_items().size(), 1u);
  EXPECT_TRUE(
      extensions_container()->IsActionVisibleOnToolbar(extension->id()));
}

// Tests that a when an extension is reloaded with manifest errors, and
// therefore fails to be loaded into Chrome, it's removed from the toolbar and
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

  ExtensionMenuItemView* menu_item = GetOnlyMenuItem();
  EXPECT_EQ(menu_items().size(), 1u);

  ClickPinButton(menu_item);
  EXPECT_TRUE(
      extensions_container()->IsActionVisibleOnToolbar(extension->id()));

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
  EXPECT_EQ(menu_items().size(), 0u);
  for (views::View* child : extensions_container()->children()) {
    EXPECT_FALSE(views::IsViewClass<ToolbarActionView>(child));
  }
}
