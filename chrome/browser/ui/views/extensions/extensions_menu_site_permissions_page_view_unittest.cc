// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include "base/feature_list.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_site_permissions_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

class ExtensionsSitePermissionsPageViewUnitTest
    : public ExtensionsToolbarUnitTest {
 public:
  ExtensionsSitePermissionsPageViewUnitTest();
  ~ExtensionsSitePermissionsPageViewUnitTest() override = default;
  ExtensionsSitePermissionsPageViewUnitTest(
      const ExtensionsSitePermissionsPageViewUnitTest&) = delete;
  ExtensionsSitePermissionsPageViewUnitTest& operator=(
      const ExtensionsSitePermissionsPageViewUnitTest&) = delete;

  // Opens menu and navigates to site permissions page for `extension_id`.
  void ShowSitePermissionsPage(extensions::ExtensionId extension_id);

  // Returns whether me menu has the main page opened.
  bool IsMainPageOpened();

  // Returns whether the menu has the `extension_id` site permissions page
  // opened.
  bool IsSitePermissionsPageOpened(extensions::ExtensionId extension_id);

  // Since this is a unittest, the extensions menu widget sometimes needs a
  // nudge to re-layout the views.
  void LayoutMenuIfNecessary();

  ExtensionsMenuMainPageView* main_page();
  ExtensionsMenuSitePermissionsPageView* site_permissions_page();

  // ExtensionsToolbarUnitTest:
  void SetUp() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<content::WebContentsTester> web_contents_tester_;
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
  auto extensionA = InstallExtension("A Extension");

  ShowSitePermissionsPage(extensionA->id());

  // Verify site permissions page is open for extension A.
  EXPECT_TRUE(IsSitePermissionsPageOpened(extensionA->id()));

  // Adding a new extension doesn't affect the opened site permissions page for
  // extension A.
  auto extensionB = InstallExtension("B Extension");
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
  auto extension = InstallExtension("Test Extension");

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
        "manifest_version": 3
      })";
  extension_directory.WriteManifest(kManifest);
  extensions::ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_directory.UnpackedPath());

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
