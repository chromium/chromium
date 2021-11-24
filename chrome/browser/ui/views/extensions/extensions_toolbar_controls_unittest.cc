// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_controls.h"

#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "ui/views/view_utils.h"

class ExtensionsToolbarControlsUnitTest : public ExtensionsToolbarUnitTest {
 public:
  ExtensionsToolbarControlsUnitTest();
  ~ExtensionsToolbarControlsUnitTest() override = default;
  ExtensionsToolbarControlsUnitTest(const ExtensionsToolbarControlsUnitTest&) =
      delete;
  const ExtensionsToolbarControlsUnitTest& operator=(
      const ExtensionsToolbarControlsUnitTest&) = delete;

  // Returns whether the site access button is visible or not.
  bool IsSiteAccessButtonVisible();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

ExtensionsToolbarControlsUnitTest::ExtensionsToolbarControlsUnitTest() {
  scoped_feature_list_.InitAndEnableFeature(
      features::kExtensionsMenuAccessControl);
}

bool ExtensionsToolbarControlsUnitTest::IsSiteAccessButtonVisible() {
  for (auto* view : extensions_container()->children()) {
    if (views::IsViewClass<ExtensionsToolbarControls>(view))
      return static_cast<ExtensionsToolbarControls*>(view)
          ->site_access_button_for_testing()
          ->GetVisible();
  }
  return false;
}

TEST_F(ExtensionsToolbarControlsUnitTest,
       SiteAccessButtonVisibility_NavigationBetweenPages) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();
  const GURL url_a("http://www.a.com");
  const GURL url_b("http://www.b.com");

  // Add an extension that only requests access to a specific url.
  InstallExtensionWithHostPermissions("specific_url", {url_a.spec()});
  EXPECT_FALSE(IsSiteAccessButtonVisible());

  // Navigate to an url the extension should have access to.
  web_contents_tester->NavigateAndCommit(url_a);
  EXPECT_TRUE(IsSiteAccessButtonVisible());

  // Navigate to an url the extension should not have access to.
  web_contents_tester->NavigateAndCommit(url_b);
  EXPECT_FALSE(IsSiteAccessButtonVisible());
}

TEST_F(ExtensionsToolbarControlsUnitTest,
       SiteAccessButtonVisibility_ContextMenuChangesHostPermissions) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();
  const GURL url_a("http://www.a.com");
  const GURL url_b("http://www.b.com");

  // Add an extension with all urls host permissions. Since we haven't navigated
  // to an url yet, the extension should not have access.
  auto extension =
      InstallExtensionWithHostPermissions("all_urls", {"<all_urls>"});
  EXPECT_FALSE(IsSiteAccessButtonVisible());

  // Navigate to an url the extension should have access to as part of
  // <all_urls>.
  web_contents_tester->NavigateAndCommit(url_a);
  EXPECT_TRUE(IsSiteAccessButtonVisible());

  // Change the extension to run only on the current site using the context
  // menu. The extension should still have access to the current site.
  extensions::ExtensionContextMenuModel context_menu(
      extension.get(), browser(), extensions::ExtensionContextMenuModel::PINNED,
      nullptr, true);
  context_menu.ExecuteCommand(
      extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE, 0);
  EXPECT_TRUE(IsSiteAccessButtonVisible());

  // Navigate to a different url. The extension should not have access.
  web_contents_tester->NavigateAndCommit(url_b);
  EXPECT_FALSE(IsSiteAccessButtonVisible());

  // Go back to the original url. The extension should have access.
  web_contents_tester->NavigateAndCommit(url_a);
  EXPECT_TRUE(IsSiteAccessButtonVisible());
}

TEST_F(ExtensionsToolbarControlsUnitTest,
       SiteAccessButtonVisibility_MultipleExtensions) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();
  const GURL url_a("http://www.a.com");
  const GURL url_b("http://www.b.com");

  // There are no extensions installed yet, so no extension has access to the
  // current site.
  EXPECT_FALSE(IsSiteAccessButtonVisible());

  // Add an extension that doesn't request host permissions. Extension should
  // not have access to the current site.
  InstallExtension("no_permissions");
  EXPECT_FALSE(IsSiteAccessButtonVisible());

  // Add an extension that only requests access to url_a. Extension should not
  // have access to the current site.
  InstallExtensionWithHostPermissions("specific_url", {url_a.spec()});
  EXPECT_FALSE(IsSiteAccessButtonVisible());

  // Add an extension with all urls host permissions. Extension should not have
  // access because there isn't a real url yet.
  auto extension_all_urls =
      InstallExtensionWithHostPermissions("all_urls", {"<all_urls>"});
  EXPECT_FALSE(IsSiteAccessButtonVisible());

  // Navigate to the url that "specific_url" extension has access to. Both
  // "all_urls" and "specific_urls" should have accessn to the current site.
  web_contents_tester->NavigateAndCommit(url_a);
  EXPECT_TRUE(IsSiteAccessButtonVisible());

  // Navigate to a different url. Only "all_urls" should have access.
  web_contents_tester->NavigateAndCommit(url_b);
  EXPECT_TRUE(IsSiteAccessButtonVisible());

  // Remove the only extension that has access to the current site. No extension
  // should have access to the current site.
  UninstallExtension(extension_all_urls->id());
  EXPECT_FALSE(IsSiteAccessButtonVisible());
}
