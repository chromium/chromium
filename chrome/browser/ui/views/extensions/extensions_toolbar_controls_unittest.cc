// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_controls.h"

#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/notification_types.h"
#include "ui/views/view_utils.h"

class ExtensionsToolbarControlsUnitTest : public ExtensionsToolbarUnitTest {
 public:
  ExtensionsToolbarControlsUnitTest();
  ~ExtensionsToolbarControlsUnitTest() override = default;
  ExtensionsToolbarControlsUnitTest(const ExtensionsToolbarControlsUnitTest&) =
      delete;
  const ExtensionsToolbarControlsUnitTest& operator=(
      const ExtensionsToolbarControlsUnitTest&) = delete;

  ExtensionsRequestAccessButton* request_access_button();
  ExtensionsToolbarButton* site_access_button();

  // Returns whether the request access button is visible or not.
  bool IsRequestAccessButtonVisible();

  // Returns whether the site access button is visible or not.
  bool IsSiteAccessButtonVisible();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

ExtensionsToolbarControlsUnitTest::ExtensionsToolbarControlsUnitTest() {
  scoped_feature_list_.InitAndEnableFeature(
      features::kExtensionsMenuAccessControl);
}

ExtensionsRequestAccessButton*
ExtensionsToolbarControlsUnitTest::request_access_button() {
  return extensions_container()
      ->GetExtensionsToolbarControls()
      ->request_access_button_for_testing();
}

ExtensionsToolbarButton*
ExtensionsToolbarControlsUnitTest::site_access_button() {
  return extensions_container()
      ->GetExtensionsToolbarControls()
      ->site_access_button_for_testing();
}

bool ExtensionsToolbarControlsUnitTest::IsRequestAccessButtonVisible() {
  return request_access_button()->GetVisible();
}

bool ExtensionsToolbarControlsUnitTest::IsSiteAccessButtonVisible() {
  return site_access_button()->GetVisible();
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
      nullptr, true,
      extensions::ExtensionContextMenuModel::ContextMenuSource::kToolbarAction);
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

  // TODO(crbug.com/1304959): Remove the only extension that requests access to
  // the current site to verify no extension has access to the current
  // site. Uninstall extension in unit tests is flaky.
}

TEST_F(ExtensionsToolbarControlsUnitTest,
       RequestAccessButtonVisibility_NavigationBetweenPages) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();
  const GURL url_a("http://www.a.com");
  const GURL url_b("http://www.b.com");

  // Add an extension that only requests access to a specific url, and withhold
  // site access.
  auto extension_a =
      InstallExtensionWithHostPermissions("Extension A", {url_a.spec()});
  WithholdHostPermissions(extension_a.get());
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Navigate to an url the extension requests access to.
  web_contents_tester->NavigateAndCommit(url_a);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 1));

  // Navigate to an url the extension does not request access to.
  web_contents_tester->NavigateAndCommit(url_b);
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

TEST_F(ExtensionsToolbarControlsUnitTest,
       RequestAccessButtonVisibility_ContextMenuChangesHostPermissions) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();
  const GURL url_a("http://www.a.com");
  const GURL url_b("http://www.b.com");

  // Add an extension with all urls host permissions. Since we haven't navigated
  // to an url yet, the extension should not request access.
  auto extension =
      InstallExtensionWithHostPermissions("Extension AllUrls", {"<all_urls>"});
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Navigate to an url the extension should have access to as part of
  // <all_urls>, since permissions are granted by default.
  web_contents_tester->NavigateAndCommit(url_a);
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  extensions::ExtensionContextMenuModel context_menu(
      extension.get(), browser(), extensions::ExtensionContextMenuModel::PINNED,
      nullptr, true,
      extensions::ExtensionContextMenuModel::ContextMenuSource::kToolbarAction);

  // Change the extension to run only on click using the context
  // menu. The extension should request access to the current site.
  {
    content::WindowedNotificationObserver permissions_observer(
        extensions::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
        content::NotificationService::AllSources());
    context_menu.ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_CLICK, 0);
    permissions_observer.Wait();
    EXPECT_TRUE(IsRequestAccessButtonVisible());
    EXPECT_EQ(
        request_access_button()->GetText(),
        l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 1));
  }

  // Change the extension to run only on site using the context
  // menu. The extension should not request access to the current site.
  {
    content::WindowedNotificationObserver permissions_observer(
        extensions::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
        content::NotificationService::AllSources());
    context_menu.ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE, 0);
    permissions_observer.Wait();
    EXPECT_FALSE(IsRequestAccessButtonVisible());
  }
}

TEST_F(ExtensionsToolbarControlsUnitTest,
       RequestAccessButtonVisibility_MultipleExtensions) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();
  const GURL url_a("http://www.a.com");
  const GURL url_b("http://www.b.com");

  // Navigate to a.com and since there are no extensions installed yet, no
  // extension is requesting access to the current site.
  web_contents_tester->NavigateAndCommit(url_a);
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Add an extension that doesn't request host permissions.
  InstallExtension("no_permissions");
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Add an extension that only requests access to a.com, and
  // withhold host permissions.
  auto extension_a =
      InstallExtensionWithHostPermissions("Extension A", {url_a.spec()});
  WithholdHostPermissions(extension_a.get());
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 1));

  // Add an extension with all urls host permissions, and withhold host
  // permissions.
  auto extension_all_urls =
      InstallExtensionWithHostPermissions("Extension AllUrls", {"<all_urls>"});
  WithholdHostPermissions(extension_all_urls.get());
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 2));

  // Navigate to a different url. Only "all_urls" should request access.
  web_contents_tester->NavigateAndCommit(url_b);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 1));

  // TODO(crbug.com/1304959): Remove the only extension that requests access to
  // the current site to verify no extension should have access to the current
  // site. Uninstall extension in unit tests is flaky.
}
