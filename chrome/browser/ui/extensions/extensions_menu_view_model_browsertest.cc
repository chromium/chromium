// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"

#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_platform_delegate.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/permissions/permissions_updater.h"
#include "extensions/browser/permissions/scripting_permissions_modifier.h"
#include "extensions/browser/permissions/site_permissions_helper.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace {

using PermissionsManager = extensions::PermissionsManager;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

// A mock extensions menu platform delegate.
class TestPlatformDelegate : public ExtensionsMenuViewPlatformDelegate {
 public:
  TestPlatformDelegate() = default;
  ~TestPlatformDelegate() override = default;

  void AttachToModel(ExtensionsMenuViewModel* model) override {}
  void DetachFromModel() override {}
  void OnActiveWebContentsChanged(content::WebContents* web_contents) override {
  }
  void OnHostAccessRequestAddedOrUpdated(
      const extensions::ExtensionId& extension_id,
      content::WebContents* web_contents) override {}
  void OnHostAccessRequestRemoved(
      const extensions::ExtensionId& extension_id) override {}
  void OnHostAccessRequestsCleared() override {}
  void OnHostAccessRequestDismissedByUser(
      const extensions::ExtensionId& extension_id) override {}
  void OnShowHostAccessRequestsInToolbarChanged(
      const extensions::ExtensionId& extension_id,
      bool can_show_requests) override {}
  void OnPermissionsSettingsChanged() override {}
  void OnToolbarActionAdded(
      const ToolbarActionsModel::ActionId& action_id) override {}
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override {}
  void OnToolbarModelInitialized() override {}
  void OnToolbarActionUpdated() override {}
  void OnToolbarPinnedActionsChanged() override {}
};

}  // namespace

class ExtensionsMenuViewModelBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  ExtensionsMenuViewModelBrowserTest() = default;
  ~ExtensionsMenuViewModelBrowserTest() override = default;

  // Adds an extension with the given `host_permission`.
  scoped_refptr<const extensions::Extension> AddExtensionWithHostPermission(
      const std::string& name,
      const std::string& host_permission);

  // Adds an extension with `activeTab` permission.
  scoped_refptr<const extensions::Extension> AddActiveTabExtension(
      const std::string& name);

  // Adds an `extension` with the given `host_permissions`,
  // `permissions` and `location`.
  scoped_refptr<const extensions::Extension> AddExtension(
      const std::string& name,
      const std::vector<std::string>& permissions,
      const std::vector<std::string>& host_permissions);

  ExtensionsMenuViewModel* menu_model() { return menu_model_.get(); }
  SitePermissionsHelper* permissions_helper() {
    return permissions_helper_.get();
  }
  PermissionsManager* permissions_manager() { return permissions_manager_; }

  // ExtensionBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 private:
  std::unique_ptr<ExtensionsMenuViewModel> menu_model_;
  std::unique_ptr<SitePermissionsHelper> permissions_helper_;
  raw_ptr<PermissionsManager> permissions_manager_;
};

scoped_refptr<const extensions::Extension>
ExtensionsMenuViewModelBrowserTest::AddExtensionWithHostPermission(
    const std::string& name,
    const std::string& host_permission) {
  return AddExtension(name, /*permissions=*/{}, {host_permission});
}

scoped_refptr<const extensions::Extension>
ExtensionsMenuViewModelBrowserTest::AddActiveTabExtension(
    const std::string& name) {
  return AddExtension(name, /*permissions=*/{"activeTab"},
                      /*host_permissions=*/{});
}

scoped_refptr<const extensions::Extension>
ExtensionsMenuViewModelBrowserTest::AddExtension(
    const std::string& name,
    const std::vector<std::string>& permissions,
    const std::vector<std::string>& host_permissions) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .AddAPIPermissions(permissions)
          .AddHostPermissions(host_permissions)
          .SetID(crx_file::id_util::GenerateId(name))
          .Build();
  extension_registrar()->AddExtension(extension.get());
  return extension;
}

void ExtensionsMenuViewModelBrowserTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());

  menu_model_ = std::make_unique<ExtensionsMenuViewModel>(
      browser_window_interface(), std::make_unique<TestPlatformDelegate>());

  permissions_helper_ = std::make_unique<SitePermissionsHelper>(profile());
  permissions_manager_ = PermissionsManager::Get(profile());
}

void ExtensionsMenuViewModelBrowserTest::ExtensionsMenuViewModelBrowserTest::
    TearDownOnMainThread() {
  permissions_manager_ = nullptr;
  permissions_helper_.reset();
  menu_model_.reset();
  ExtensionBrowserTest::TearDownOnMainThread();
}

// Tests that the extensions menu view model correctly updates the site access
// for an extension.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest, UpdateSiteAccess) {
  // Add extension that requests host permissions.
  scoped_refptr<const extensions::Extension> extension =
      AddExtensionWithHostPermission("Extension", "<all_urls>");

  // Navigate to a site the extension has site access to.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));
  content::WebContents* web_contents = GetActiveWebContents();

  // Verify default initial site access is "on all sites".
  EXPECT_EQ(permissions_manager()->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            PermissionsManager::UserSiteAccess::kOnAllSites);

  // Update site access to "on site".
  menu_model()->UpdateSiteAccess(extension->id(),
                                 PermissionsManager::UserSiteAccess::kOnSite);
  EXPECT_EQ(permissions_helper()->GetSiteInteraction(*extension, web_contents),
            SitePermissionsHelper::SiteInteraction::kGranted);
  EXPECT_EQ(permissions_manager()->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            PermissionsManager::UserSiteAccess::kOnSite);

  // Update site access to "on click".
  menu_model()->UpdateSiteAccess(extension->id(),
                                 PermissionsManager::UserSiteAccess::kOnClick);
  EXPECT_EQ(permissions_helper()->GetSiteInteraction(*extension, web_contents),
            SitePermissionsHelper::SiteInteraction::kWithheld);
  EXPECT_EQ(permissions_manager()->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            PermissionsManager::UserSiteAccess::kOnClick);
}

// Tests that the extensions menu view model correctly grants site access to an
// extension that requests hosts permissions and access is currently withheld.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GrantSiteAccess_HostPermission) {
  // Add extension that requests host permissions, and withheld site access.
  scoped_refptr<const extensions::Extension> extension =
      AddExtensionWithHostPermission("Extension", "*://example.com/*");
  extensions::ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  // Navigate to a site the extension requested access to.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));
  content::WebContents* web_contents = GetActiveWebContents();

  // Verify site interaction is 'withheld' and site access is 'on click'
  EXPECT_EQ(permissions_helper()->GetSiteInteraction(*extension, web_contents),
            SitePermissionsHelper::SiteInteraction::kWithheld);
  EXPECT_EQ(permissions_manager()->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            PermissionsManager::UserSiteAccess::kOnClick);

  // Granting site access changes site interaction to 'granted' and site access
  // to 'on site'.
  menu_model()->GrantSiteAccess(extension->id());
  EXPECT_EQ(permissions_helper()->GetSiteInteraction(*extension, web_contents),
            SitePermissionsHelper::SiteInteraction::kGranted);
  EXPECT_EQ(permissions_manager()->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            PermissionsManager::UserSiteAccess::kOnSite);
}

// Tests that the extensions menu view model correctly grants site
// access for an extension with activeTab permission.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GrantSiteAccess_ActiveTab) {
  // Add extension with activeTab permission.
  scoped_refptr<const extensions::Extension> extension =
      AddActiveTabExtension("Extension");

  // Navigate to any (unrestricted) site.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));
  content::WebContents* web_contents = GetActiveWebContents();

  // Verify site interaction is 'activeTab' and site access is 'on click'
  EXPECT_EQ(permissions_manager()->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            PermissionsManager::UserSiteAccess::kOnClick);
  EXPECT_EQ(permissions_helper()->GetSiteInteraction(*extension, web_contents),
            SitePermissionsHelper::SiteInteraction::kActiveTab);

  // Granting site access changes site interaction to 'granted' but site access
  // remains 'on click', since it's a one-time grant.
  menu_model()->GrantSiteAccess(extension->id());
  EXPECT_EQ(permissions_helper()->GetSiteInteraction(*extension, web_contents),
            SitePermissionsHelper::SiteInteraction::kGranted);
  EXPECT_EQ(permissions_manager()->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            PermissionsManager::UserSiteAccess::kOnClick);
}

// Tests that the extensions menu view model correctly revokes site access to an
// extension that requests hosts permissions and access is currently granted.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       RevokeSiteAccess_HostPermission) {
  // Add extension that requests host permissions, which are granted by default.
  scoped_refptr<const extensions::Extension> extension =
      AddExtensionWithHostPermission("Extension", "*://example.com/*");

  // Navigate to a site the extension requested access to.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));
  content::WebContents* web_contents = GetActiveWebContents();

  // Verify site interaction is 'granted' and site access is 'on site'
  EXPECT_EQ(permissions_helper()->GetSiteInteraction(*extension, web_contents),
            SitePermissionsHelper::SiteInteraction::kGranted);
  EXPECT_EQ(permissions_manager()->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            PermissionsManager::UserSiteAccess::kOnSite);

  // Revoking site access changes site interaction to 'withheld' and site access
  // to 'on click'
  menu_model()->RevokeSiteAccess(extension->id());
  EXPECT_EQ(permissions_helper()->GetSiteInteraction(*extension, web_contents),
            SitePermissionsHelper::SiteInteraction::kWithheld);
  EXPECT_EQ(permissions_manager()->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            PermissionsManager::UserSiteAccess::kOnClick);
}

// Tests that the extensions menu view model correctly revokes site
// access for an extension with granted activeTab permission.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       RevokeSiteAccess_ActiveTab) {
  // Add extension with activeTab permission.
  scoped_refptr<const extensions::Extension> extension =
      AddActiveTabExtension("Extension");

  // Navigate to any (unrestricted) site.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));
  content::WebContents* web_contents = GetActiveWebContents();

  // Grant one-time site access to the extension.
  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(action_runner);
  action_runner->GrantTabPermissions({extension.get()});
  EXPECT_EQ(permissions_helper()->GetSiteInteraction(*extension, web_contents),
            SitePermissionsHelper::SiteInteraction::kGranted);
  EXPECT_EQ(permissions_manager()->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            PermissionsManager::UserSiteAccess::kOnClick);

  // Revoking site access changes site interaction to 'activeTab' and site
  // access remains 'on click'.
  menu_model()->RevokeSiteAccess(extension->id());
  EXPECT_EQ(permissions_helper()->GetSiteInteraction(*extension, web_contents),
            SitePermissionsHelper::SiteInteraction::kActiveTab);
  EXPECT_EQ(permissions_manager()->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            PermissionsManager::UserSiteAccess::kOnClick);
}

// Tests that the extensions menu view model correctly updates the site setting
// for an extension.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest, UpdateSiteSetting) {
  // Add extension that requests host permissions.
  scoped_refptr<const extensions::Extension> extension =
      AddExtensionWithHostPermission("Extension", "<all_urls>");

  // Navigate to a site the extension has site access to.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));
  auto origin = url::Origin::Create(url);

  // Verify default initial site setting is "customize by extension".
  EXPECT_EQ(permissions_manager()->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);

  // Update site setting to "block all extensions".
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  EXPECT_EQ(permissions_manager()->GetUserSiteSetting(origin),
            PermissionsManager::UserSiteSetting::kBlockAllExtensions);
}

// Tests that the extensions menu view model correctly dismisses a host access
// request for an extension.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       DismissHostAccessRequest) {
  // Add extension that requests host permissions, and withheld them.
  scoped_refptr<const extensions::Extension> extension =
      AddExtensionWithHostPermission("Extension", "*://example.com/*");
  extensions::ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  // Navigate to a site.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));
  content::WebContents* web_contents = GetActiveWebContents();
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);

  // Add a host access request.
  permissions_manager()->AddHostAccessRequest(web_contents, tab_id, *extension);
  EXPECT_TRUE(permissions_manager()->HasActiveHostAccessRequest(
      tab_id, extension->id()));

  // Dismiss the host access request.
  menu_model()->DismissHostAccessRequest(extension->id());

  // Verify the host access request was dismissed.
  EXPECT_FALSE(permissions_manager()->HasActiveHostAccessRequest(
      tab_id, extension->id()));
}

// Tests that the extensions menu view model correctly allows a host access
// request for an extension.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       AllowHostAccessRequest) {
  // Add extension that requests host permissions, and withheld them.
  scoped_refptr<const extensions::Extension> extension =
      AddExtensionWithHostPermission("Extension", "*://example.com/*");
  extensions::ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  // Navigate to a site.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));
  content::WebContents* web_contents = GetActiveWebContents();
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);

  // Add a host access request.
  permissions_manager()->AddHostAccessRequest(web_contents, tab_id, *extension);
  EXPECT_TRUE(permissions_manager()->HasActiveHostAccessRequest(
      tab_id, extension->id()));
  EXPECT_EQ(permissions_manager()->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            PermissionsManager::UserSiteAccess::kOnClick);

  // Allow the host access request.
  extensions::PermissionsManagerWaiter waiter(
      PermissionsManager::Get(profile()));
  menu_model()->AllowHostAccessRequest(extension->id());
  waiter.WaitForExtensionPermissionsUpdate();

  // Verify the host access request was allowed and site access is now 'on
  // site'.
  EXPECT_FALSE(permissions_manager()->HasActiveHostAccessRequest(
      tab_id, extension->id()));
  EXPECT_EQ(permissions_manager()->GetUserSiteAccess(
                *extension, web_contents->GetLastCommittedURL()),
            PermissionsManager::UserSiteAccess::kOnSite);
}

// Tests that the extensions menu view model correctly updates whether to show
// host access requests in the toolbar for an extension.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       ShowHostAccessRequestsInToolbar) {
  // Add extension that requests host permissions.
  scoped_refptr<const extensions::Extension> extension =
      AddExtensionWithHostPermission("Extension", "*://example.com/*");

  // By default, extensions can show host access requests in the toolbar.
  EXPECT_TRUE(
      permissions_helper()->ShowAccessRequestsInToolbar(extension->id()));

  // Set to not show host access requests in the toolbar.
  menu_model()->ShowHostAccessRequestsInToolbar(extension->id(), false);
  EXPECT_FALSE(
      permissions_helper()->ShowAccessRequestsInToolbar(extension->id()));

  // Set to show host access requests in the toolbar.
  menu_model()->ShowHostAccessRequestsInToolbar(extension->id(), true);
  EXPECT_TRUE(
      permissions_helper()->ShowAccessRequestsInToolbar(extension->id()));
}
