// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_controls.h"

#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/site_permissions_helper.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

class ExtensionsToolbarControlsUnitTest : public ExtensionsToolbarUnitTest {
 public:
  ExtensionsToolbarControlsUnitTest();
  ~ExtensionsToolbarControlsUnitTest() override = default;
  ExtensionsToolbarControlsUnitTest(const ExtensionsToolbarControlsUnitTest&) =
      delete;
  const ExtensionsToolbarControlsUnitTest& operator=(
      const ExtensionsToolbarControlsUnitTest&) = delete;

  ExtensionsRequestAccessButton* request_access_button();

  // Returns whether the request access button is visible or not.
  bool IsRequestAccessButtonVisible();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

ExtensionsToolbarControlsUnitTest::ExtensionsToolbarControlsUnitTest() {
  scoped_feature_list_.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);
}

ExtensionsRequestAccessButton*
ExtensionsToolbarControlsUnitTest::request_access_button() {
  return extensions_container()
      ->GetExtensionsToolbarControls()
      ->request_access_button_for_testing();
}

bool ExtensionsToolbarControlsUnitTest::IsRequestAccessButtonVisible() {
  return request_access_button()->GetVisible();
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

  // Changing the context menu may trigger the reload page bubble. Accept it so
  // permissions are updated.
  extensions::ExtensionActionRunner* runner =
      extensions::ExtensionActionRunner::GetForWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  runner->accept_bubble_for_testing(true);

  auto* manager = extensions::PermissionsManager::Get(profile());
  // Change the extension to run only on click using the context
  // menu. The extension should request access to the current site.
  {
    extensions::PermissionsManagerWaiter waiter(manager);
    context_menu.ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_CLICK, 0);
    waiter.WaitForExtensionPermissionsUpdate();
    EXPECT_TRUE(IsRequestAccessButtonVisible());
    EXPECT_EQ(
        request_access_button()->GetText(),
        l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 1));
  }

  // Change the extension to run only on site using the context
  // menu. The extension should not request access to the current site.
  {
    extensions::PermissionsManagerWaiter waiter(manager);
    context_menu.ExecuteCommand(
        extensions::ExtensionContextMenuModel::PAGE_ACCESS_RUN_ON_SITE, 0);
    waiter.WaitForExtensionPermissionsUpdate();
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

  // Remove the only extension that requests access to the current site.
  UninstallExtension(extension_all_urls->id());
  LayoutContainerIfNecessary();
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

// Tests that extensions with activeTab and requested url with withheld access
// are taken into account for the request access button visibility, but not the
// ones with just activeTab.
TEST_F(ExtensionsToolbarControlsUnitTest,
       RequestAccessButtonVisibility_ActiveTabExtensions) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();
  const GURL requested_url("http://www.requested-url.com");

  InstallExtensionWithPermissions("Extension A", {"activeTab"});
  constexpr char kExtensionName[] = "Extension B";
  auto extension = InstallExtensionWithHostPermissions(
      kExtensionName, {requested_url.spec(), "activeTab"});
  WithholdHostPermissions(extension.get());

  web_contents_tester->NavigateAndCommit(requested_url);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionsNamesForTesting(),
              testing::ElementsAre(kExtensionName));

  web_contents_tester->NavigateAndCommit(
      GURL("http://www.non-requested-url.com"));
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

// Test that request access button is visible based on the user site setting
// selected.
TEST_F(ExtensionsToolbarControlsUnitTest,
       RequestAccessButtonVisibility_UserSiteSetting) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();
  const GURL url("http://www.url.com");
  auto url_origin = url::Origin::Create(url);

  // Install an extension and withhold permissions so request access button can
  // be visible.
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  web_contents_tester->NavigateAndCommit(url);
  WaitForAnimation();

  // A site has "customize by extensions" site setting by default,
  ASSERT_EQ(
      GetUserSiteSetting(url),
      extensions::PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(IsRequestAccessButtonVisible());

  auto* manager = extensions::PermissionsManager::Get(profile());

  {
    // Request access button is not visible in restricted sites.
    extensions::PermissionsManagerWaiter manager_waiter(manager);
    manager->AddUserRestrictedSite(url_origin);
    manager_waiter.WaitForUserPermissionsSettingsChange();
    WaitForAnimation();
    EXPECT_FALSE(IsRequestAccessButtonVisible());
  }

  {
    // Request acesss button is visible if site is not restricted,
    // and at least one extension is requesting access.
    extensions::PermissionsManagerWaiter manager_waiter(manager);
    manager->RemoveUserRestrictedSite(url_origin);
    manager_waiter.WaitForUserPermissionsSettingsChange();
    WaitForAnimation();
    EXPECT_TRUE(IsRequestAccessButtonVisible());
  }
}

TEST_F(ExtensionsToolbarControlsUnitTest,
       RequestAccessButton_OnPressedExecuteAction) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();

  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  const GURL url("http://www.example.com");
  web_contents_tester->NavigateAndCommit(url);
  WaitForAnimation();
  LayoutContainerIfNecessary();

  constexpr char kActivatedUserAction[] =
      "Extensions.Toolbar.ExtensionsActivatedFromRequestAccessButton";
  base::UserActionTester user_action_tester;
  extensions::SitePermissionsHelper permissions(browser()->profile());

  // Request access button is visible because extension A is requesting
  // access.
  ASSERT_TRUE(request_access_button()->GetVisible());
  EXPECT_EQ(user_action_tester.GetActionCount(kActivatedUserAction), 0);
  EXPECT_EQ(permissions.GetSiteAccess(*extension, url),
            extensions::SitePermissionsHelper::SiteAccess::kOnClick);

  ClickButton(request_access_button());

  WaitForAnimation();
  LayoutContainerIfNecessary();

  // Verify request access button is hidden since extension executed its
  // action. Extension's site access should have not changed, since clicking the
  // button grants one time access.
  ASSERT_FALSE(request_access_button()->GetVisible());
  EXPECT_EQ(user_action_tester.GetActionCount(kActivatedUserAction), 1);
  EXPECT_EQ(permissions.GetSiteAccess(*extension, url),
            extensions::SitePermissionsHelper::SiteAccess::kOnClick);
}

class ExtensionsToolbarControlsWithPermittedSitesUnitTest
    : public ExtensionsToolbarControlsUnitTest {
 public:
  ExtensionsToolbarControlsWithPermittedSitesUnitTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        extensions_features::kExtensionsMenuAccessControl,
        extensions_features::kExtensionsMenuAccessControlWithPermittedSites};
    std::vector<base::test::FeatureRef> disabled_features;
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ExtensionsToolbarControlsWithPermittedSitesUnitTest(
      const ExtensionsToolbarControlsWithPermittedSitesUnitTest&) = delete;
  const ExtensionsToolbarControlsWithPermittedSitesUnitTest& operator=(
      const ExtensionsToolbarControlsWithPermittedSitesUnitTest&) = delete;
  ~ExtensionsToolbarControlsWithPermittedSitesUnitTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that request access button is visible based on the user site setting
// selected.
TEST_F(ExtensionsToolbarControlsWithPermittedSitesUnitTest,
       RequestAccessButtonVisibilityOnPermittedSites) {
  content::WebContentsTester* web_contents_tester =
      AddWebContentsAndGetTester();
  const GURL url("http://www.url.com");
  auto url_origin = url::Origin::Create(url);

  // Install an extension and withhold permissions so request access button can
  // be visible.
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  web_contents_tester->NavigateAndCommit(url);
  WaitForAnimation();

  // A site has "customize by extensions" site setting by default,
  ASSERT_EQ(
      GetUserSiteSetting(url),
      extensions::PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(IsRequestAccessButtonVisible());

  // Request access button is not visible in permitted sites.
  auto* manager = extensions::PermissionsManager::Get(profile());
  extensions::PermissionsManagerWaiter waiter(manager);
  manager->AddUserPermittedSite(url_origin);
  waiter.WaitForUserPermissionsSettingsChange();
  WaitForAnimation();

  // Request access button visibility is the same for other site settings, which
  // is already tested, regardless of whether permitted sites are supported or
  // not.
}
