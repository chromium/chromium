// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_controls.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/site_permissions_helper.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/permissions_manager.h"
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

  // Navigates to `url`.
  void NavigateAndCommit(const GURL& URL);

  ExtensionsRequestAccessButton* request_access_button();
  ExtensionsToolbarButton* extensions_button();

  // Returns whether the request access button is visible or not.
  bool IsRequestAccessButtonVisible();

  // ExtensionsToolbarUnitTest:
  void SetUp() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<content::WebContentsTester, DanglingUntriaged> web_contents_tester_;
};

ExtensionsToolbarControlsUnitTest::ExtensionsToolbarControlsUnitTest() {
  scoped_feature_list_.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);
}

void ExtensionsToolbarControlsUnitTest::NavigateAndCommit(const GURL& url) {
  web_contents_tester_->NavigateAndCommit(url);
  WaitForAnimation();
}

ExtensionsRequestAccessButton*
ExtensionsToolbarControlsUnitTest::request_access_button() {
  return extensions_container()
      ->GetExtensionsToolbarControls()
      ->request_access_button_for_testing();
}

ExtensionsToolbarButton*
ExtensionsToolbarControlsUnitTest::extensions_button() {
  return extensions_container()->GetExtensionsButton();
}

bool ExtensionsToolbarControlsUnitTest::IsRequestAccessButtonVisible() {
  return request_access_button()->GetVisible();
}

void ExtensionsToolbarControlsUnitTest::SetUp() {
  ExtensionsToolbarUnitTest::SetUp();
  web_contents_tester_ = AddWebContentsAndGetTester();
}

TEST_F(ExtensionsToolbarControlsUnitTest,
       ExtensionsButton_SitePermissionsUpdates) {
  // Install an extension that requests host permissions.
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL url("http://www.url.com");
  auto url_origin = url::Origin::Create(url);
  NavigateAndCommit(url);

  auto* manager = extensions::PermissionsManager::Get(profile());
  {
    // Extensions button has "all extensions blocked" icon type when it's
    // an user restricted site.
    extensions::PermissionsManagerWaiter manager_waiter(manager);
    manager->AddUserRestrictedSite(url_origin);
    manager_waiter.WaitForUserPermissionsSettingsChange();
    WaitForAnimation();
    EXPECT_EQ(extensions_button()->GetStateForTesting(),
              ExtensionsToolbarButton::State::kAllExtensionsBlocked);
  }

  {
    // Extensions button has "any extension has access" icon type when it's not
    // an user restricted site and 1+ extensions have
    // site access granted. Note that by default extensions have granted access.
    extensions::PermissionsManagerWaiter manager_waiter(manager);
    manager->RemoveUserRestrictedSite(url_origin);
    manager_waiter.WaitForUserPermissionsSettingsChange();
    WaitForAnimation();
    EXPECT_EQ(extensions_button()->GetStateForTesting(),
              ExtensionsToolbarButton::State::kAnyExtensionHasAccess);
  }

  {
    // Extension button has "default" icon type when it's not an user restricted
    // site and no extensions have site access granted.
    // To achieve this, we withhold host permissions in the only extension
    // installed.
    WithholdHostPermissions(extension.get());
    WaitForAnimation();
    EXPECT_EQ(extensions_button()->GetStateForTesting(),
              ExtensionsToolbarButton::State::kDefault);
  }
}

TEST_F(ExtensionsToolbarControlsUnitTest,
       ExtensionsButton_ChromeRestrictedSite) {
  InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL restricted_url("chrome://extensions");
  NavigateAndCommit(restricted_url);

  // Extensions button has "all extensions blocked" icon type for chrome
  // restricted sites.
  EXPECT_EQ(extensions_button()->GetStateForTesting(),
            ExtensionsToolbarButton::State::kAllExtensionsBlocked);
}

TEST_F(ExtensionsToolbarControlsUnitTest,
       RequestAccessButtonVisibility_NavigationBetweenPages) {
  const GURL url_a("http://www.a.com");
  const GURL url_b("http://www.b.com");

  // Add an extension that only requests access to a specific url, and withhold
  // site access.
  auto extension_a =
      InstallExtensionWithHostPermissions("Extension A", {url_a.spec()});
  WithholdHostPermissions(extension_a.get());
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Navigate to an url the extension requests access to.
  NavigateAndCommit(url_a);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 1));

  // Navigate to an url the extension does not request access to.
  NavigateAndCommit(url_b);
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

TEST_F(ExtensionsToolbarControlsUnitTest,
       RequestAccessButtonVisibility_ContextMenuChangesHostPermissions) {
  const GURL url_a("http://www.a.com");
  const GURL url_b("http://www.b.com");

  // Add an extension with all urls host permissions. Since we haven't navigated
  // to an url yet, the extension should not request access.
  auto extension =
      InstallExtensionWithHostPermissions("Extension AllUrls", {"<all_urls>"});
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Navigate to an url the extension should have access to as part of
  // <all_urls>, since permissions are granted by default.
  NavigateAndCommit(url_a);
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  extensions::ExtensionContextMenuModel context_menu(
      extension.get(), browser(), /*is_pinned=*/true, /*delegate=*/nullptr,
      /*can_show_icon_in_toolbar=*/true,
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
  const GURL url_a("http://www.a.com");
  const GURL url_b("http://www.b.com");

  // Navigate to a.com and since there are no extensions installed yet, no
  // extension is requesting access to the current site.
  NavigateAndCommit(url_a);
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Add an extension that doesn't request host permissions.
  InstallExtension("no_permissions");
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Add an extension that only requests access to a.com, and
  // withhold host permissions.
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {url_a.spec()});
  WithholdHostPermissions(extension.get());
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 1));
  std::u16string tooltip = base::UTF8ToUTF16(
      base::StrCat({"Click to allow on a.com:\n", extension->name()}));
  EXPECT_EQ(request_access_button()->GetTooltipText(gfx::Point()), tooltip);

  // Add an extension with all urls host permissions, and withhold host
  // permissions.
  auto extension_all_urls =
      InstallExtensionWithHostPermissions("Extension AllUrls", {"<all_urls>"});
  WithholdHostPermissions(extension_all_urls.get());
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 2));
  tooltip = base::UTF8ToUTF16(
      base::StrCat({"Click to allow on a.com:\n", extension->name(), "\n",
                    extension_all_urls->name()}));
  EXPECT_EQ(request_access_button()->GetTooltipText(gfx::Point()), tooltip);

  // Navigate to a different url. Only "all_urls" should request access.
  NavigateAndCommit(url_b);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 1));
  tooltip = base::UTF8ToUTF16(
      base::StrCat({"Click to allow on b.com:\n", extension_all_urls->name()}));
  EXPECT_EQ(request_access_button()->GetTooltipText(gfx::Point()), tooltip);

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
  const GURL requested_url("http://www.requested-url.com");

  InstallExtensionWithPermissions("Extension A", {"activeTab"});
  auto extension = InstallExtensionWithHostPermissions(
      "Extension B", {requested_url.spec(), "activeTab"});
  WithholdHostPermissions(extension.get());

  NavigateAndCommit(requested_url);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension->id()));

  NavigateAndCommit(GURL("http://www.non-requested-url.com"));
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

// Test that request access button is visible based on the user site setting
// selected.
TEST_F(ExtensionsToolbarControlsUnitTest,
       RequestAccessButtonVisibility_UserSiteSetting) {
  const GURL url("http://www.url.com");
  auto url_origin = url::Origin::Create(url);

  // Install an extension and withhold permissions so request access button can
  // be visible.
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  NavigateAndCommit(url);

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

// Tests that an extension requesting site access but not allowed in
// the button is not shown in the request access button.
TEST_F(ExtensionsToolbarControlsUnitTest,
       RequestAccessButtonVisibility_ExtensionsNotAllowedInButton) {
  // Add two extensions that request access to all urls, and withhold their
  // site access.
  auto extension_a =
      InstallExtensionWithHostPermissions("Extension A", {"<all_urls>"});
  auto extension_b =
      InstallExtensionWithHostPermissions("Extension B", {"<all_urls>"});
  WithholdHostPermissions(extension_a.get());
  WithholdHostPermissions(extension_b.get());

  // By default, both extensions should be allowed in the request
  // access button. However, request access button is not visible because we
  // haven't navigated to a site yet.
  extensions::SitePermissionsHelper permissions_helper(browser()->profile());
  EXPECT_TRUE(
      permissions_helper.ShowAccessRequestsInToolbar(extension_a->id()));
  EXPECT_TRUE(
      permissions_helper.ShowAccessRequestsInToolbar(extension_b->id()));
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Navigate to an url that both extensions requests access to.
  const GURL url("http://www.example.com");
  NavigateAndCommit(url);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 2));

  // Disallow extension A in the request access button. Verify only extension A
  // is visible in the button.
  permissions_helper.SetShowAccessRequestsInToolbar(extension_a->id(), false);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 1));

  // Disallow extension B in the request access button. Verify button is not
  // visible anymore.
  permissions_helper.SetShowAccessRequestsInToolbar(extension_b->id(), false);
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

TEST_F(ExtensionsToolbarControlsUnitTest,
       RequestAccessButtonVisibility_ExtensionDismissedRequests) {
  // Add two extensions that request access to all urls, and withhold their
  // site access.
  auto extension_a =
      InstallExtensionWithHostPermissions("Extension A", {"<all_urls>"});
  auto extension_b =
      InstallExtensionWithHostPermissions("Extension B", {"<all_urls>"});
  WithholdHostPermissions(extension_a.get());
  WithholdHostPermissions(extension_b.get());

  // By default, both extensions should be allowed in the request
  // access button. However, request access button is not visible because we
  // haven't navigated to a site yet.
  extensions::SitePermissionsHelper permissions_helper(browser()->profile());
  EXPECT_TRUE(
      permissions_helper.ShowAccessRequestsInToolbar(extension_a->id()));
  EXPECT_TRUE(
      permissions_helper.ShowAccessRequestsInToolbar(extension_b->id()));
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Navigate to an url that both extensions requests access to.
  const GURL url("http://www.example.com");
  NavigateAndCommit(url);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 2));

  // Dismiss extension A's requests. Verify only extension B is visible in the
  // button.
  extensions::TabHelper* tab_helper = extensions::TabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  tab_helper->DismissExtensionRequests(extension_a->id());
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 1));

  // Dismiss extension B's requests. Verify button is not visible anymore.
  tab_helper->DismissExtensionRequests(extension_b->id());
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

TEST_F(ExtensionsToolbarControlsUnitTest,
       RequestAccessButton_OnPressedExecuteAction) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  const GURL url("http://www.example.com");
  NavigateAndCommit(url);
  LayoutContainerIfNecessary();

  constexpr char kActivatedUserAction[] =
      "Extensions.Toolbar.ExtensionsActivatedFromRequestAccessButton";
  base::UserActionTester user_action_tester;
  auto* permissions = extensions::PermissionsManager::Get(profile());

  // Request access button is visible because extension A is requesting
  // access.
  ASSERT_TRUE(request_access_button()->GetVisible());
  EXPECT_EQ(user_action_tester.GetActionCount(kActivatedUserAction), 0);
  EXPECT_EQ(permissions->GetUserSiteAccess(*extension, url),
            extensions::PermissionsManager::UserSiteAccess::kOnClick);

  // Extension menu button has default state since extensions are not blocked,
  // and there is no extension with access to the site.
  EXPECT_EQ(extensions_button()->GetStateForTesting(),
            ExtensionsToolbarButton::State::kDefault);

  ClickButton(request_access_button());
  WaitForAnimation();
  LayoutContainerIfNecessary();

  // Verify request access button is hidden since extension executed its
  // action. Extension's site access should have not changed, since clicking the
  // button grants one time access.
  ASSERT_FALSE(request_access_button()->GetVisible());
  EXPECT_EQ(user_action_tester.GetActionCount(kActivatedUserAction), 1);
  EXPECT_EQ(permissions->GetUserSiteAccess(*extension, url),
            extensions::PermissionsManager::UserSiteAccess::kOnClick);

  // Verify extensions menu button has "any extension  has access" state, since
  // the extension executed its action.
  EXPECT_EQ(extensions_button()->GetStateForTesting(),
            ExtensionsToolbarButton::State::kAnyExtensionHasAccess);
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
  const GURL url("http://www.url.com");
  auto url_origin = url::Origin::Create(url);

  // Install an extension and withhold permissions so request access button can
  // be visible.
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  NavigateAndCommit(url);

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
