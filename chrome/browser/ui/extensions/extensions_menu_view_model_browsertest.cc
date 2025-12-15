// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_platform_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/permissions/permissions_updater.h"
#include "extensions/browser/permissions/scripting_permissions_modifier.h"
#include "extensions/browser/permissions/site_permissions_helper.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/l10n/l10n_util.h"

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
  void OnUserPermissionsSettingsChanged() override {}
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

  // Adds a basic extension.
  scoped_refptr<const extensions::Extension> AddExtension(
      const std::string& name);

  // Adds an extension with the given `host_permission`.
  scoped_refptr<const extensions::Extension> AddExtensionWithHostPermission(
      const std::string& name,
      const std::string& host_permission);

  // Adds an extension with `activeTab` permission.
  scoped_refptr<const extensions::Extension> AddActiveTabExtension(
      const std::string& name);

  // Adds an enterprise-installed extension.
  scoped_refptr<const extensions::Extension> AddEnterpriseExtension(
      const std::string& name,
      const std::vector<std::string>& permissions,
      const std::vector<std::string>& host_permissions);

  // Adds an `extension` with the given `host_permissions`,
  // `permissions` and `location`.
  scoped_refptr<const extensions::Extension> AddExtension(
      const std::string& name,
      const std::vector<std::string>& permissions,
      const std::vector<std::string>& host_permissions,
      extensions::mojom::ManifestLocation location =
          extensions::mojom::ManifestLocation::kUnpacked);

  // Adds a policy restriction blocking access to sites matching `pattern`.
  void AddPolicyBlockedSite(std::string_view pattern);

  // Navigates the active web contents to a URL on `host_name`.
  void NavigateTo(std::string_view host_name);

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
ExtensionsMenuViewModelBrowserTest::AddExtension(const std::string& name) {
  return AddExtension(name, /*permissions=*/{}, /*host_permissions=*/{});
}

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
ExtensionsMenuViewModelBrowserTest::AddEnterpriseExtension(
    const std::string& name,
    const std::vector<std::string>& permissions,
    const std::vector<std::string>& host_permissions) {
  return AddExtension(name, permissions, host_permissions,
                      extensions::mojom::ManifestLocation::kExternalPolicy);
}

scoped_refptr<const extensions::Extension>
ExtensionsMenuViewModelBrowserTest::AddExtension(
    const std::string& name,
    const std::vector<std::string>& permissions,
    const std::vector<std::string>& host_permissions,
    extensions::mojom::ManifestLocation location) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .AddAPIPermissions(permissions)
          .AddHostPermissions(host_permissions)
          .SetLocation(location)
          .SetID(crx_file::id_util::GenerateId(name))
          .Build();
  extension_registrar()->AddExtension(extension.get());
  return extension;
}

void ExtensionsMenuViewModelBrowserTest::AddPolicyBlockedSite(
    std::string_view pattern) {
  URLPattern default_policy_blocked_pattern =
      URLPattern(URLPattern::SCHEME_ALL, pattern);
  extensions::URLPatternSet default_allowed_hosts;
  extensions::URLPatternSet default_blocked_hosts;
  default_blocked_hosts.AddPattern(default_policy_blocked_pattern);
  extensions::PermissionsData::SetDefaultPolicyHostRestrictions(
      extensions::util::GetBrowserContextId(profile()), default_blocked_hosts,
      default_allowed_hosts);
}

void ExtensionsMenuViewModelBrowserTest::NavigateTo(
    std::string_view host_name) {
  const GURL url = embedded_test_server()->GetURL(host_name, "/simple.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));
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

// Tests that the extensions menu view model correctly gets the site settings
// for the current site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetSiteSettingsState) {
  // Add an extension that requests host permissions.
  AddExtensionWithHostPermission("Extension", "<all_urls>");

  // Navigate to a site that the extension requests access to.
  NavigateTo("example.com");

  // Verify the site settings when the user can customize the site's access.
  ExtensionsMenuViewModel::SiteSettingsState site_settings_state =
      menu_model()->GetSiteSettingsState();
  EXPECT_FALSE(site_settings_state.has_tooltip);
  EXPECT_EQ(site_settings_state.toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_TRUE(site_settings_state.toggle.is_on);
  EXPECT_EQ(site_settings_state.toggle.tooltip_text,
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_SITE_SETTINGS_TOGGLE_ON_TOOLTIP));

  // Update the user site setting to block all extensions on the current site.
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);

  // Verify the site settings when the user has blocked access to the current
  // site.
  site_settings_state = menu_model()->GetSiteSettingsState();
  EXPECT_FALSE(site_settings_state.has_tooltip);
  EXPECT_EQ(site_settings_state.toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_FALSE(site_settings_state.toggle.is_on);
  EXPECT_EQ(site_settings_state.toggle.tooltip_text,
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_MENU_SITE_SETTINGS_TOGGLE_OFF_TOOLTIP));

  // Navigate to restricted site.
  std::u16string restricted_site = u"chrome://extensions";
  const GURL restricted_url(restricted_site);
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), restricted_url));

  // Verify the site setting when the site is restricted
  site_settings_state = menu_model()->GetSiteSettingsState();
  EXPECT_FALSE(site_settings_state.has_tooltip);
  EXPECT_EQ(site_settings_state.toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_FALSE(site_settings_state.toggle.is_on);
  EXPECT_EQ(site_settings_state.toggle.tooltip_text, std::u16string());

  // Navigate to a policy blocked site.
  AddPolicyBlockedSite("*://*.policy-blocked.com/*");
  NavigateTo("policy-blocked.com");

  // Verify the site setting when the site is policy blocked.
  site_settings_state = menu_model()->GetSiteSettingsState();
  EXPECT_FALSE(site_settings_state.has_tooltip);
  EXPECT_EQ(site_settings_state.toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_FALSE(site_settings_state.toggle.is_on);
  EXPECT_EQ(site_settings_state.toggle.tooltip_text, std::u16string());

  // Verify site settings has a tooltip when the site is policy blocked but
  // there is an enterprise-installed extension with site access.
  AddEnterpriseExtension("Enterprise extension", /*permissions=*/{},
                         {"all_urls"});
  site_settings_state = menu_model()->GetSiteSettingsState();
  EXPECT_TRUE(site_settings_state.has_tooltip);
}

// Tests that the extensions menu view model correctly returns the extension's
// site access options state.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetExtensionSiteAccessOptionsState) {
  scoped_refptr<const extensions::Extension> extension_A =
      AddExtensionWithHostPermission("Extension A", "*://a.com/*");
  scoped_refptr<const extensions::Extension> extension_with_all_hosts =
      AddExtensionWithHostPermission("Extension with all hosts", "<all_urls>");
  scoped_refptr<const extensions::Extension> extension_activeTab =
      AddActiveTabExtension("Extension with activeTab");

  // Verify the site access state for an extension with all host permissions
  // granted.
  NavigateTo("example.com");
  auto site_access_states = menu_model()->GetExtensionSiteAccessOptionsState(
      extension_with_all_hosts->id());
  EXPECT_EQ(site_access_states.on_click_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(site_access_states.on_site_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(site_access_states.on_all_sites_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_FALSE(site_access_states.on_click_option.is_on);
  EXPECT_FALSE(site_access_states.on_site_option.is_on);
  EXPECT_TRUE(site_access_states.on_all_sites_option.is_on);

  // Verify the site permissions for an extension with only access to the
  // current site. 'on site' is enabled because the user can choose that option,
  // whereas 'on all sites' is not.
  NavigateTo("a.com");
  site_access_states =
      menu_model()->GetExtensionSiteAccessOptionsState(extension_A->id());
  EXPECT_EQ(site_access_states.on_click_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(site_access_states.on_site_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(site_access_states.on_all_sites_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_FALSE(site_access_states.on_click_option.is_on);
  EXPECT_TRUE(site_access_states.on_site_option.is_on);
  EXPECT_FALSE(site_access_states.on_all_sites_option.is_on);

  // Update site access to 'on click'.
  menu_model()->UpdateSiteAccess(extension_A->id(),
                                 PermissionsManager::UserSiteAccess::kOnClick);

  // Verify the site permissions for an extension with site access withheld when
  // it's requesting access only to the current site. 'on site' is enabled
  // because the user can still choose that option, whereas 'on all sites' is
  // not.
  site_access_states =
      menu_model()->GetExtensionSiteAccessOptionsState(extension_A->id());
  EXPECT_EQ(site_access_states.on_click_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(site_access_states.on_site_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(site_access_states.on_all_sites_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_TRUE(site_access_states.on_click_option.is_on);
  EXPECT_FALSE(site_access_states.on_site_option.is_on);
  EXPECT_FALSE(site_access_states.on_all_sites_option.is_on);

  // Verify the site permissions for an extension with only on click access
  site_access_states = menu_model()->GetExtensionSiteAccessOptionsState(
      extension_activeTab->id());
  EXPECT_EQ(site_access_states.on_click_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(site_access_states.on_site_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(site_access_states.on_all_sites_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_TRUE(site_access_states.on_click_option.is_on);
  EXPECT_FALSE(site_access_states.on_site_option.is_on);
  EXPECT_FALSE(site_access_states.on_all_sites_option.is_on);
}

// Tests that the extensions menu view model correctly returns the extension's
// show host access requests toggle.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetExtensionShowRequestsToggleState) {
  scoped_refptr<const extensions::Extension> extension =
      AddExtensionWithHostPermission("Extension", "<all_urls>");
  NavigateTo("example.com");

  // Verify the toggle state is by default enabled and on.
  auto toggle_state =
      menu_model()->GetExtensionShowRequestsToggleState(extension->id());
  EXPECT_EQ(toggle_state.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_TRUE(toggle_state.is_on);
  EXPECT_EQ(
      toggle_state.accessible_name,
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SHOW_REQUESTS_TOGGLE_ON));

  SitePermissionsHelper(profile()).SetShowAccessRequestsInToolbar(
      extension->id(), /*show_access_requests_in_toolbar=*/false);

  // Verify the toggle state is enabled and off.
  toggle_state =
      menu_model()->GetExtensionShowRequestsToggleState(extension->id());
  EXPECT_EQ(toggle_state.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_FALSE(toggle_state.is_on);
  EXPECT_EQ(
      toggle_state.accessible_name,
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SHOW_REQUESTS_TOGGLE_OFF));
}

// Tests the menu item state for an extension that did not request access to
// the current site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuItemState_RestrictedSite) {
  // Add an extension that requests site access.
  auto extension = AddExtensionWithHostPermission("Extension", "<all_urls>");

  // Navigate to a restricted site.
  const GURL restricted_url("chrome://extensions");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), restricted_url));

  // User cannot customize the extension's site access. Thus site access toggle
  // and site permissions button are always hidden.
  auto menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
}

// Tests the menu item state for an extension that did not request access to
// the current site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuItemState_PolicyBlockedSite) {
  // Add an extension that requests site access.
  auto extension = AddExtensionWithHostPermission("Extension", "<all_urls>");

  // Navigate to a policy-blocked site.
  AddPolicyBlockedSite("*://*.policy-blocked.com/*");
  NavigateTo("policy-blocked.com");

  // User cannot customize the extension's site access. THus:
  //   - site access toggle is always hidden.
  //   - site permissions button is disabled. We leave them visible because
  //   enterprise extensions can still have access to the site, but disabled
  //   because site access cannot be changed.
  auto menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_item_state.site_permissions_button.text, u"No access needed");
  EXPECT_EQ(menu_item_state.site_permissions_button.accessible_name,
            u"No access needed");
  EXPECT_EQ(menu_item_state.site_permissions_button.tooltip_text, u"");
}

// Tests the menu item state for an extension that did not request access to
// the current site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuItemState_NoSiteAccess) {
  // Add an extension that doesn't request access to the current site.
  auto extension = AddExtension("Simple Extension");

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is hidden.
  //   - site permissions button is disabled.
  auto menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_item_state.site_permissions_button.text, u"No access needed");
  EXPECT_EQ(menu_item_state.site_permissions_button.accessible_name,
            u"No access needed");
  EXPECT_EQ(menu_item_state.site_permissions_button.tooltip_text, u"");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden.
  //   - site permissions button is hidden
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
}

// Tests the menu item state for an extension that has withheld access to the
// current site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuItemState_WithheldSiteAccess) {
  // Add an extension that requests access, and withhold its access.
  auto extension =
      AddExtensionWithHostPermission("Extension", "*://example.com/*");
  extensions::ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is enabled and off.
  //   - site permissions button is enabled.
  auto menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_FALSE(menu_item_state.site_access_toggle.is_on);
  EXPECT_EQ(menu_item_state.site_access_toggle.tooltip_text,
            u"Not allowed on this site");
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(menu_item_state.site_permissions_button.text,
            u"Ask on every visit");
  EXPECT_EQ(menu_item_state.site_permissions_button.accessible_name,
            u"Ask on every visit. Select to change site permissions");
  EXPECT_EQ(menu_item_state.site_permissions_button.tooltip_text,
            u"Change site permissions");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden.
  //   - site permissions button is hidden
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
}

// Tests the menu item state for an extension that has granted access to the
// current site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuItemState_GrantedSiteAccess) {
  // Add an extension that requests access to a specific site.
  auto extension =
      AddExtensionWithHostPermission("Extension", "*://example.com/*");

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is enabled and on.
  //   - site permissions button is enabled.
  auto menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_TRUE(menu_item_state.site_access_toggle.is_on);
  EXPECT_EQ(menu_item_state.site_access_toggle.tooltip_text,
            u"Allowed on this site");
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(menu_item_state.site_permissions_button.text,
            u"Always on this site");
  EXPECT_EQ(menu_item_state.site_permissions_button.accessible_name,
            u"Always on this site. Select to change site permissions");
  EXPECT_EQ(menu_item_state.site_permissions_button.tooltip_text,
            u"Change site permissions");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden.
  //   - site permissions button is hidden
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
}

// Tests the menu item state for an extension that has grant access to all
// sites.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuItemState_GrantedBroadSiteAccess) {
  // Add an extension that requests access to a specific site.
  auto extension = AddExtensionWithHostPermission("Extension", "<all_urls>");

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is enabled and on.
  //   - site permissions button is enabled.
  auto menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_TRUE(menu_item_state.site_access_toggle.is_on);
  EXPECT_EQ(menu_item_state.site_access_toggle.tooltip_text,
            u"Allowed on this site");
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(menu_item_state.site_permissions_button.text,
            u"Always on all sites");
  EXPECT_EQ(menu_item_state.site_permissions_button.accessible_name,
            u"Always on all sites. Select to change site permissions");
  EXPECT_EQ(menu_item_state.site_permissions_button.tooltip_text,
            u"Change site permissions");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden.
  //   - site permissions button is hidden
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
}

// Tests the menu item state for an extension that did not requested access to
// the current site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuItemState_NoSiteAccess_Enterprise) {
  // Add an extension that doesn't request access to the current site.
  auto extension = AddEnterpriseExtension("Extension", /*permissions=*/{},
                                          /*host_permissions=*/{});

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is hidden
  //   - site permissions button is disabled.
  auto menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_item_state.site_permissions_button.text, u"No access needed");
  EXPECT_EQ(menu_item_state.site_permissions_button.accessible_name,
            u"No access needed. Installed by your administrator");
  EXPECT_EQ(menu_item_state.site_permissions_button.tooltip_text,
            u"Installed by your administrator");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden.
  //   - site permissions button is hidden
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
}

// Tests the menu item state for an enterprise extension that has withheld
// access to a specific site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuItemState_WithheldSiteAccess_Enterprise) {
  // Add an extension that request access active tab access.
  auto extension = AddEnterpriseExtension(
      "Extension", /*permissions=*/{"activeTab"}, /*host_permissions=*/{});

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is hidden
  //   - site permissions button is disabled.
  auto menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_item_state.site_permissions_button.text,
            u"Ask on every visit");
  EXPECT_EQ(menu_item_state.site_permissions_button.accessible_name,
            u"Ask on every visit. Installed by your administrator");
  EXPECT_EQ(menu_item_state.site_permissions_button.tooltip_text,
            u"Installed by your administrator");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden
  //   - site permissions button is disabled.
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_item_state.site_permissions_button.text,
            u"Ask on every visit");
  EXPECT_EQ(menu_item_state.site_permissions_button.accessible_name,
            u"Ask on every visit. Installed by your administrator");
  EXPECT_EQ(menu_item_state.site_permissions_button.tooltip_text,
            u"Installed by your administrator");
}

// Tests the menu item state for an enterprise extension that requests access to
// a specific site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuItemState_GrantedSiteAccess_Enterprise) {
  // Add an extension that request access to all sites.
  auto extension =
      AddEnterpriseExtension("Extension", /*permissions=*/{},
                             /*host_permissions=*/{"*://example.com/*"});

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is hidden
  //   - site permissions button is disabled.
  auto menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_item_state.site_permissions_button.text,
            u"Always on this site");
  EXPECT_EQ(menu_item_state.site_permissions_button.accessible_name,
            u"Always on this site. Installed by your administrator");
  EXPECT_EQ(menu_item_state.site_permissions_button.tooltip_text,
            u"Installed by your administrator");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden
  //   - site permissions button is disabled.
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_item_state.site_permissions_button.text,
            u"Always on this site");
  EXPECT_EQ(menu_item_state.site_permissions_button.accessible_name,
            u"Always on this site. Installed by your administrator");
  EXPECT_EQ(menu_item_state.site_permissions_button.tooltip_text,
            u"Installed by your administrator");
}

// Tests the menu item state for an enterprise extension that requests access to
// all sites.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuItemState_GrantedBroadSiteAccess_Enterprise) {
  // Add an extension that request access to all sites.
  auto extension = AddEnterpriseExtension("Extension", /*permissions=*/{},
                                          /*host_permissions=*/{"<all_urls>"});

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is hidden
  //   - site permissions button is disabled.
  auto menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_item_state.site_permissions_button.text,
            u"Always on all sites");
  EXPECT_EQ(menu_item_state.site_permissions_button.accessible_name,
            u"Always on all sites. Installed by your administrator");
  EXPECT_EQ(menu_item_state.site_permissions_button.tooltip_text,
            u"Installed by your administrator");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden
  //   - site permissions button is disabled.
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_item_state = menu_model()->GetMenuItemState(extension->id());
  EXPECT_EQ(menu_item_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_item_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_item_state.site_permissions_button.text,
            u"Always on all sites");
  EXPECT_EQ(menu_item_state.site_permissions_button.accessible_name,
            u"Always on all sites. Installed by your administrator");
  EXPECT_EQ(menu_item_state.site_permissions_button.tooltip_text,
            u"Installed by your administrator");
}

// Tests that the extensions menu view model correctly returns the optional
// section.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest, GetOptionalSection) {
  AddExtensionWithHostPermission("Extension", "<all_urls>");
  NavigateTo("example.com");

  // Verify the optional section is 'host access requests' when the user can
  // customize the site's access. Platform delegate may hide this section if
  // there are no active requests.
  EXPECT_EQ(menu_model()->GetOptionalSection(),
            ExtensionsMenuViewModel::OptionalSection::kHostAccessRequests);

  // Update the user site setting to block all extensions on the current site.
  // This action implies a state change that requires a page reload.
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);

  // Verify the optional section is 'reload page' when the page needs to be
  // reloaded for changes to take place.
  EXPECT_EQ(menu_model()->GetOptionalSection(),
            ExtensionsMenuViewModel::OptionalSection::kReloadPage);

  // Reload the page to clear the "reload required" state.
  {
    content::TestNavigationObserver observer(GetActiveWebContents());
    menu_model()->ReloadWebContents();
    observer.Wait();
  }

  // Verify the optional section is 'none' when the user has blocked access to
  // the current site and there is no pending reload.
  EXPECT_EQ(menu_model()->GetOptionalSection(),
            ExtensionsMenuViewModel::OptionalSection::kNone);

  // Update the user site setting to allow extensions on the current site again.
  // This again triggers the need for a reload.
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_EQ(menu_model()->GetOptionalSection(),
            ExtensionsMenuViewModel::OptionalSection::kReloadPage);

  // Reload the page.
  {
    content::TestNavigationObserver observer(GetActiveWebContents());
    menu_model()->ReloadWebContents();
    observer.Wait();
  }

  // Verify we are back to 'host access requests' optional section.
  EXPECT_EQ(menu_model()->GetOptionalSection(),
            ExtensionsMenuViewModel::OptionalSection::kHostAccessRequests);

  // Navigate to a restricted site.
  const GURL restricted_url("chrome://extensions");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), restricted_url));

  // Restricted sites should never show an optional section.
  EXPECT_EQ(menu_model()->GetOptionalSection(),
            ExtensionsMenuViewModel::OptionalSection::kNone);

  // Navigate to a policy blocked site.
  AddPolicyBlockedSite("*://*.policy-blocked.com/*");
  NavigateTo("policy-blocked.com");

  // Policy blocked sites should never show an optional section.
  EXPECT_EQ(menu_model()->GetOptionalSection(),
            ExtensionsMenuViewModel::OptionalSection::kNone);
}
