// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_action_delegate.h"
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
#include "extensions/common/manifest_constants.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace {

using PermissionsManager = extensions::PermissionsManager;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

// A fake implementation of ExtensionActionDelegate that does nothing.
class FakeExtensionActionDelegate : public ExtensionActionDelegate {
 public:
  void AttachToModel(ExtensionActionViewModel* model) override {}
  void DetachFromModel() override {}
  void RegisterCommand() override {}
  void UnregisterCommand() override {}
  bool IsShowingPopup() const override { return false; }
  void HidePopup() override {}
  gfx::NativeView GetPopupNativeView() override { return gfx::NativeView(); }
  void TriggerPopup(std::unique_ptr<extensions::ExtensionViewHost> host,
                    PopupShowAction show_action,
                    bool by_user,
                    ShowPopupCallback callback) override {}
  void ShowContextMenuAsFallback() override {}
  bool CloseOverflowMenuIfOpen() override { return false; }
};

// The test delegate that acts as the factory for Action ViewModels.
class TestExtensionsMenuDelegate : public ExtensionsMenuViewModel::Delegate {
 public:
  explicit TestExtensionsMenuDelegate(BrowserWindowInterface* browser)
      : browser_(browser) {}
  ~TestExtensionsMenuDelegate() override = default;

  // ExtensionsMenuViewModel::Delegate:
  std::unique_ptr<ExtensionActionViewModel> CreateActionViewModel(
      const extensions::ExtensionId& extension_id) override {
    return ExtensionActionViewModel::Create(
        extension_id, browser_,
        std::make_unique<FakeExtensionActionDelegate>());
  }

 private:
  raw_ptr<BrowserWindowInterface> browser_;
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
  std::unique_ptr<TestExtensionsMenuDelegate> menu_delegate_;
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

  menu_delegate_ =
      std::make_unique<TestExtensionsMenuDelegate>(browser_window_interface());
  menu_model_ = std::make_unique<ExtensionsMenuViewModel>(
      browser_window_interface(), menu_delegate_.get());

  permissions_helper_ = std::make_unique<SitePermissionsHelper>(profile());
  permissions_manager_ = PermissionsManager::Get(profile());
}

void ExtensionsMenuViewModelBrowserTest::ExtensionsMenuViewModelBrowserTest::
    TearDownOnMainThread() {
  permissions_manager_ = nullptr;
  permissions_helper_.reset();
  menu_model_.reset();
  menu_delegate_.reset();
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

// Tests that ExecuteAction triggers the extension action and records metrics.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest, ExecuteAction) {
  base::UserActionTester user_action_tester;
  constexpr char kActivatedUserAction[] =
      "Extensions.Toolbar.ExtensionActivatedFromMenu";

  auto extension = AddExtension("Test Extension");
  EXPECT_EQ(0, user_action_tester.GetActionCount(kActivatedUserAction));

  menu_model()->ExecuteAction(extension->id());
  EXPECT_EQ(1, user_action_tester.GetActionCount(kActivatedUserAction));

  // Note: Other tests verify whether the action was actually run (e.g. script
  // execution, popup creation). Here we only verify the action count because
  // that's the only logic handled specifically by this ViewModel.
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

// Tests that the extensions menu view model correctly determines whether the
// site permissions page can be shown for an extension.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       CanShowSitePermissionsPage) {
  auto enterprise_extension =
      AddEnterpriseExtension("Enterprise", {}, {"<all_urls>"});
  auto no_permissions_extension = AddExtension("No Permissions");
  auto extension_with_permissions =
      AddExtensionWithHostPermission("Extension", "<all_urls>");

  // Only extension with requested host permissions can show site permissions
  // page on a non-restricted site.
  NavigateTo("example.com");
  EXPECT_FALSE(
      menu_model()->CanShowSitePermissionsPage(enterprise_extension->id()));
  EXPECT_FALSE(
      menu_model()->CanShowSitePermissionsPage(no_permissions_extension->id()));
  EXPECT_TRUE(menu_model()->CanShowSitePermissionsPage(
      extension_with_permissions->id()));

  // No extension can show the site permission page on a restricted-site.
  const GURL restricted_url("chrome://extensions");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), restricted_url));
  EXPECT_FALSE(
      menu_model()->CanShowSitePermissionsPage(enterprise_extension->id()));
  EXPECT_FALSE(
      menu_model()->CanShowSitePermissionsPage(no_permissions_extension->id()));
  EXPECT_FALSE(menu_model()->CanShowSitePermissionsPage(
      extension_with_permissions->id()));
}

// Tests that GetActionButtonState returns the correct state when the extension
// has no action.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetActionButtonState_NoAction) {
  auto extension = AddExtension("Test Extension");

  NavigateTo("example.com");

  auto button_state =
      menu_model()->GetActionButtonState(extension->id(), gfx::Size());

  // Button is disabled when the extension has no action.
  EXPECT_EQ(button_state.text, u"Test Extension");
  EXPECT_EQ(button_state.tooltip_text, u"Test Extension");
  EXPECT_EQ(button_state.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_FALSE(button_state.icon.IsEmpty());
}

// Tests that GetActionButtonState returns the correct state when the extension
// has an action.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetActionButtonState_Action) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test Extension")
          .SetManifestKey(
              extensions::manifest_keys::kAction,
              base::DictValue().Set("default_title", "Custom Tooltip"))
          .Build();
  extension_registrar()->AddExtension(extension.get());

  NavigateTo("example.com");

  auto button_state =
      menu_model()->GetActionButtonState(extension->id(), gfx::Size());

  // Button is enabled when extension has an action.
  EXPECT_EQ(button_state.text, u"Test Extension");
  EXPECT_EQ(button_state.tooltip_text, u"Custom Tooltip");
  EXPECT_EQ(button_state.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_FALSE(button_state.icon.IsEmpty());
}

// Tests that GetContextMenuButtonState returns the correct state based on
// the extension's pinning status.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetContextMenuButtonState) {
  auto extension = AddExtension("Test Extension");

  // Verify the context menu button state when the extension is unpinned (by
  // default).
  ExtensionsMenuViewModel::ControlState state =
      menu_model()->GetContextMenuButtonState(extension->id());
  EXPECT_FALSE(state.is_on);
  EXPECT_EQ(state.accessible_name, u"See more options for Test Extension");

  // Verify the context menu button state when the extension is pinned.
  ToolbarActionsModel::Get(profile())->SetActionVisibility(extension->id(),
                                                           true);
  state = menu_model()->GetContextMenuButtonState(extension->id());
  EXPECT_TRUE(state.is_on);
  EXPECT_EQ(state.accessible_name,
            u"Test Extension is pinned. See more options.");
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
                       GetExtensionSitePermissionsState) {
  scoped_refptr<const extensions::Extension> extension_A =
      AddExtensionWithHostPermission("Extension A", "*://a.com/*");
  scoped_refptr<const extensions::Extension> extension_with_all_hosts =
      AddExtensionWithHostPermission("Extension with all hosts", "<all_urls>");
  scoped_refptr<const extensions::Extension> extension_activeTab =
      AddActiveTabExtension("Extension with activeTab");

  // Verify the site access state for an extension with all host permissions
  // granted.
  NavigateTo("example.com");
  auto site_permissions = menu_model()->GetExtensionSitePermissionsState(
      extension_with_all_hosts->id(), gfx::Size());
  EXPECT_EQ(site_permissions.on_click_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(site_permissions.on_site_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(site_permissions.on_all_sites_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_FALSE(site_permissions.on_click_option.is_on);
  EXPECT_FALSE(site_permissions.on_site_option.is_on);
  EXPECT_TRUE(site_permissions.on_all_sites_option.is_on);

  // Verify the site permissions for an extension with only access to the
  // current site. 'on site' is enabled because the user can choose that option,
  // whereas 'on all sites' is not.
  NavigateTo("a.com");
  site_permissions = menu_model()->GetExtensionSitePermissionsState(
      extension_A->id(), gfx::Size());
  EXPECT_EQ(site_permissions.on_click_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(site_permissions.on_site_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(site_permissions.on_all_sites_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_FALSE(site_permissions.on_click_option.is_on);
  EXPECT_TRUE(site_permissions.on_site_option.is_on);
  EXPECT_FALSE(site_permissions.on_all_sites_option.is_on);

  // Update site access to 'on click'.
  menu_model()->UpdateSiteAccess(extension_A->id(),
                                 PermissionsManager::UserSiteAccess::kOnClick);

  // Verify the site permissions for an extension with site access withheld when
  // it's requesting access only to the current site. 'on site' is enabled
  // because the user can still choose that option, whereas 'on all sites' is
  // not.
  site_permissions = menu_model()->GetExtensionSitePermissionsState(
      extension_A->id(), gfx::Size());
  EXPECT_EQ(site_permissions.on_click_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(site_permissions.on_site_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(site_permissions.on_all_sites_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_TRUE(site_permissions.on_click_option.is_on);
  EXPECT_FALSE(site_permissions.on_site_option.is_on);
  EXPECT_FALSE(site_permissions.on_all_sites_option.is_on);

  // Verify the site permissions for an extension with only on click access
  site_permissions = menu_model()->GetExtensionSitePermissionsState(
      extension_activeTab->id(), gfx::Size());
  EXPECT_EQ(site_permissions.on_click_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(site_permissions.on_site_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(site_permissions.on_all_sites_option.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_TRUE(site_permissions.on_click_option.is_on);
  EXPECT_FALSE(site_permissions.on_site_option.is_on);
  EXPECT_FALSE(site_permissions.on_all_sites_option.is_on);
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
                       GetMenuEntryState_RestrictedSite) {
  // Add an extension that requests site access.
  auto extension = AddExtensionWithHostPermission("Extension", "<all_urls>");

  // Navigate to a restricted site.
  const GURL restricted_url("chrome://extensions");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), restricted_url));

  // User cannot customize the extension's site access. Thus site access toggle
  // and site permissions button are always hidden.
  auto menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
}

// Tests the menu item state for an extension that did not request access to
// the current site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuEntryState_PolicyBlockedSite) {
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
  auto menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_entry_state.site_permissions_button.text, u"No access needed");
  EXPECT_EQ(menu_entry_state.site_permissions_button.accessible_name,
            u"No access needed");
  EXPECT_EQ(menu_entry_state.site_permissions_button.tooltip_text, u"");
}

// Tests the menu item state for an extension that did not request access to
// the current site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuEntryState_NoSiteAccess) {
  // Add an extension that doesn't request access to the current site.
  auto extension = AddExtension("Simple Extension");

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is hidden.
  //   - site permissions button is disabled.
  auto menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_entry_state.site_permissions_button.text, u"No access needed");
  EXPECT_EQ(menu_entry_state.site_permissions_button.accessible_name,
            u"No access needed");
  EXPECT_EQ(menu_entry_state.site_permissions_button.tooltip_text, u"");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden.
  //   - site permissions button is hidden
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
}

// Tests the menu item state for an extension that has withheld access to the
// current site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuEntryState_WithheldSiteAccess) {
  // Add an extension that requests access, and withhold its access.
  auto extension =
      AddExtensionWithHostPermission("Extension", "*://example.com/*");
  extensions::ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is enabled and off.
  //   - site permissions button is enabled.
  auto menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_FALSE(menu_entry_state.site_access_toggle.is_on);
  EXPECT_EQ(menu_entry_state.site_access_toggle.tooltip_text,
            u"Not allowed on this site");
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(menu_entry_state.site_permissions_button.text,
            u"Ask on every visit");
  EXPECT_EQ(menu_entry_state.site_permissions_button.accessible_name,
            u"Ask on every visit. Select to change site permissions");
  EXPECT_EQ(menu_entry_state.site_permissions_button.tooltip_text,
            u"Change site permissions");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden.
  //   - site permissions button is hidden
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
}

// Tests the menu item state for an extension that has granted access to the
// current site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuEntryState_GrantedSiteAccess) {
  // Add an extension that requests access to a specific site.
  auto extension =
      AddExtensionWithHostPermission("Extension", "*://example.com/*");

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is enabled and on.
  //   - site permissions button is enabled.
  auto menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_TRUE(menu_entry_state.site_access_toggle.is_on);
  EXPECT_EQ(menu_entry_state.site_access_toggle.tooltip_text,
            u"Allowed on this site");
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(menu_entry_state.site_permissions_button.text,
            u"Always on this site");
  EXPECT_EQ(menu_entry_state.site_permissions_button.accessible_name,
            u"Always on this site. Select to change site permissions");
  EXPECT_EQ(menu_entry_state.site_permissions_button.tooltip_text,
            u"Change site permissions");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden.
  //   - site permissions button is hidden
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
}

// Tests the menu item state for an extension that has grant access to all
// sites.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuEntryState_GrantedBroadSiteAccess) {
  // Add an extension that requests access to a specific site.
  auto extension = AddExtensionWithHostPermission("Extension", "<all_urls>");

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is enabled and on.
  //   - site permissions button is enabled.
  auto menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_TRUE(menu_entry_state.site_access_toggle.is_on);
  EXPECT_EQ(menu_entry_state.site_access_toggle.tooltip_text,
            u"Allowed on this site");
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  EXPECT_EQ(menu_entry_state.site_permissions_button.text,
            u"Always on all sites");
  EXPECT_EQ(menu_entry_state.site_permissions_button.accessible_name,
            u"Always on all sites. Select to change site permissions");
  EXPECT_EQ(menu_entry_state.site_permissions_button.tooltip_text,
            u"Change site permissions");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden.
  //   - site permissions button is hidden
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
}

// Tests the menu item state for an extension that did not requested access to
// the current site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuEntryState_NoSiteAccess_Enterprise) {
  // Add an extension that doesn't request access to the current site.
  auto extension = AddEnterpriseExtension("Extension", /*permissions=*/{},
                                          /*host_permissions=*/{});

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is hidden
  //   - site permissions button is disabled.
  auto menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_entry_state.site_permissions_button.text, u"No access needed");
  EXPECT_EQ(menu_entry_state.site_permissions_button.accessible_name,
            u"No access needed. Installed by your administrator");
  EXPECT_EQ(menu_entry_state.site_permissions_button.tooltip_text,
            u"Installed by your administrator");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden.
  //   - site permissions button is hidden
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
}

// Tests the menu item state for an enterprise extension that has withheld
// access to a specific site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuEntryState_WithheldSiteAccess_Enterprise) {
  // Add an extension that request access active tab access.
  auto extension = AddEnterpriseExtension(
      "Extension", /*permissions=*/{"activeTab"}, /*host_permissions=*/{});

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is hidden
  //   - site permissions button is disabled.
  auto menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_entry_state.site_permissions_button.text,
            u"Ask on every visit");
  EXPECT_EQ(menu_entry_state.site_permissions_button.accessible_name,
            u"Ask on every visit. Installed by your administrator");
  EXPECT_EQ(menu_entry_state.site_permissions_button.tooltip_text,
            u"Installed by your administrator");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden
  //   - site permissions button is disabled.
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_entry_state.site_permissions_button.text,
            u"Ask on every visit");
  EXPECT_EQ(menu_entry_state.site_permissions_button.accessible_name,
            u"Ask on every visit. Installed by your administrator");
  EXPECT_EQ(menu_entry_state.site_permissions_button.tooltip_text,
            u"Installed by your administrator");
}

// Tests the menu item state for an enterprise extension that requests access to
// a specific site.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuEntryState_GrantedSiteAccess_Enterprise) {
  // Add an extension that request access to all sites.
  auto extension =
      AddEnterpriseExtension("Extension", /*permissions=*/{},
                             /*host_permissions=*/{"*://example.com/*"});

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is hidden
  //   - site permissions button is disabled.
  auto menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_entry_state.site_permissions_button.text,
            u"Always on this site");
  EXPECT_EQ(menu_entry_state.site_permissions_button.accessible_name,
            u"Always on this site. Installed by your administrator");
  EXPECT_EQ(menu_entry_state.site_permissions_button.tooltip_text,
            u"Installed by your administrator");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden
  //   - site permissions button is disabled.
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_entry_state.site_permissions_button.text,
            u"Always on this site");
  EXPECT_EQ(menu_entry_state.site_permissions_button.accessible_name,
            u"Always on this site. Installed by your administrator");
  EXPECT_EQ(menu_entry_state.site_permissions_button.tooltip_text,
            u"Installed by your administrator");
}

// Tests the menu item state for an enterprise extension that requests access to
// all sites.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       GetMenuEntryState_GrantedBroadSiteAccess_Enterprise) {
  // Add an extension that request access to all sites.
  auto extension = AddEnterpriseExtension("Extension", /*permissions=*/{},
                                          /*host_permissions=*/{"<all_urls>"});

  NavigateTo("example.com");

  // When site setting is set to 'customize by extension' (default):
  //   - site access toggle is hidden
  //   - site permissions button is disabled.
  auto menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_entry_state.site_permissions_button.text,
            u"Always on all sites");
  EXPECT_EQ(menu_entry_state.site_permissions_button.accessible_name,
            u"Always on all sites. Installed by your administrator");
  EXPECT_EQ(menu_entry_state.site_permissions_button.tooltip_text,
            u"Installed by your administrator");

  // When site setting is set to 'block all extensions':
  //   - site access toggle is hidden
  //   - site permissions button is disabled.
  menu_model()->UpdateSiteSetting(
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);
  menu_entry_state =
      menu_model()->GetMenuEntryState(extension->id(), gfx::Size());
  EXPECT_EQ(menu_entry_state.site_access_toggle.status,
            ExtensionsMenuViewModel::ControlState::Status::kHidden);
  EXPECT_EQ(menu_entry_state.site_permissions_button.status,
            ExtensionsMenuViewModel::ControlState::Status::kDisabled);
  EXPECT_EQ(menu_entry_state.site_permissions_button.text,
            u"Always on all sites");
  EXPECT_EQ(menu_entry_state.site_permissions_button.accessible_name,
            u"Always on all sites. Installed by your administrator");
  EXPECT_EQ(menu_entry_state.site_permissions_button.tooltip_text,
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

// Tests that an action model is inserted to the menu model its corresponding
// extensions is installed, and that the order is maintained alphabetically
// (case-insensitive).
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       OnToolbarActionAdded) {
  // Add "Alpha". Should be the only item.
  {
    AddExtension("Alpha");
    const auto& actions = menu_model()->action_models();
    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0]->GetActionName(), u"Alpha");
  }

  // Add "Gamma". Should be after "Alpha".
  // Current order: Alpha, Gamma.
  {
    AddExtension("Gamma");
    const auto& actions = menu_model()->action_models();
    ASSERT_EQ(actions.size(), 2u);
    EXPECT_EQ(actions[0]->GetActionName(), u"Alpha");
    EXPECT_EQ(actions[1]->GetActionName(), u"Gamma");
  }

  // Add "beta". Should be inserted between "Alpha" and "Gamma" because
  // sorting is case-insensitive ("alpha" < "beta" < "gamma").
  // Current order: Alpha, beta, Gamma.
  {
    AddExtension("beta");
    const auto& actions = menu_model()->action_models();
    ASSERT_EQ(actions.size(), 3u);
    EXPECT_EQ(actions[0]->GetActionName(), u"Alpha");
    EXPECT_EQ(actions[1]->GetActionName(), u"beta");
    EXPECT_EQ(actions[2]->GetActionName(), u"Gamma");
  }
}

// Tests that the action model is removed from the menu model when the
// corresponding extension is uninstalled.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       OnToolbarActionRemoved) {
  auto extension1 = AddExtension("Alpha");
  auto extension2 = AddExtension("beta");

  // Verify initial state.
  ASSERT_EQ(menu_model()->action_models().size(), 2u);

  // Remove "Alpha". The model should update to remove the entry.
  extension_registrar()->RemoveExtension(
      extension1->id(), extensions::UnloadedExtensionReason::UNINSTALL);

  // Verify "Alpha" is gone and only "beta" remains.
  const auto& actions = menu_model()->action_models();
  ASSERT_EQ(actions.size(), 1u);
  EXPECT_EQ(actions[0]->GetActionName(), u"beta");
}

// Tests that the view model is correctly populated and sorted when initialized
// with existing extensions (e.g., on browser startup).
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       PopulateActionModels) {
  // Add extensions before creating the model we want to test.
  auto extension1 = AddExtension("Alpha");
  auto extension2 = AddExtension("Gamma");
  auto extension3 = AddExtension("beta");

  // Create a new model. The one in SetUpOnMainThread was created when no
  // extensions existed. We want to test the constructor's population logic.
  auto delegate =
      std::make_unique<TestExtensionsMenuDelegate>(browser_window_interface());
  auto model = std::make_unique<ExtensionsMenuViewModel>(
      browser_window_interface(), delegate.get());

  // Verify action models were added and sorted alphabetically
  // (case-insensitive). Expected order: Alpha, beta, Gamma.
  const auto& actions = model->action_models();
  ASSERT_EQ(actions.size(), 3u);
  EXPECT_EQ(actions[0]->GetActionName(), u"Alpha");
  EXPECT_EQ(actions[1]->GetActionName(), u"beta");
  EXPECT_EQ(actions[2]->GetActionName(), u"Gamma");
}

// Tests that host access requests are maintained in alphabetical order
// matching the action_models_ order, regardless of the order they are added.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuViewModelBrowserTest,
                       HostAccessRequests_SortedInsertionAndRemoval) {
  // Add 3 extensions (A, B, C) and withhold their permissions, using names that
  // ensure alphabetical order.
  auto extension_A = AddExtensionWithHostPermission("Alpha", "<all_urls>");
  auto extension_B = AddExtensionWithHostPermission("Beta", "<all_urls>");
  auto extension_C = AddExtensionWithHostPermission("Gamma", "<all_urls>");

  extensions::ScriptingPermissionsModifier(profile(), extension_A)
      .SetWithholdHostPermissions(true);
  extensions::ScriptingPermissionsModifier(profile(), extension_B)
      .SetWithholdHostPermissions(true);
  extensions::ScriptingPermissionsModifier(profile(), extension_C)
      .SetWithholdHostPermissions(true);

  NavigateTo("example.com");
  content::WebContents* web_contents = GetActiveWebContents();
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);

  // Verify initial state for action models are sorted.
  const auto& actions = menu_model()->action_models();
  ASSERT_EQ(3u, actions.size());
  EXPECT_EQ(extension_A->id(), actions[0]->GetId());
  EXPECT_EQ(extension_B->id(), actions[1]->GetId());
  EXPECT_EQ(extension_C->id(), actions[2]->GetId());
  EXPECT_TRUE(menu_model()->host_access_requests().empty());

  // Add request for "Beta".
  permissions_manager()->AddHostAccessRequest(web_contents, tab_id,
                                              *extension_B);
  std::vector<extensions::ExtensionId> requests =
      menu_model()->host_access_requests();
  ASSERT_EQ(1u, requests.size());
  EXPECT_EQ(extension_B->id(), requests[0]);

  // Add request for "Alpha".
  // Order should be [Alpha, Beta, Gamma].
  permissions_manager()->AddHostAccessRequest(web_contents, tab_id,
                                              *extension_A);
  requests = menu_model()->host_access_requests();
  ASSERT_EQ(2u, requests.size());
  EXPECT_EQ(extension_A->id(), requests[0]);
  EXPECT_EQ(extension_B->id(), requests[1]);

  // Add request for "Gamma".
  // Order should be [Alpha, Beta, Gamma].
  permissions_manager()->AddHostAccessRequest(web_contents, tab_id,
                                              *extension_C);
  requests = menu_model()->host_access_requests();
  ASSERT_EQ(3u, requests.size());
  EXPECT_EQ(extension_A->id(), requests[0]);
  EXPECT_EQ(extension_B->id(), requests[1]);
  EXPECT_EQ(extension_C->id(), requests[2]);

  // Remove request for "Beta".
  // Order should be [Alpha, Gamma].
  permissions_manager()->RemoveHostAccessRequest(tab_id, extension_B->id());
  requests = menu_model()->host_access_requests();
  ASSERT_EQ(2u, requests.size());
  EXPECT_EQ(extension_A->id(), requests[0]);
  EXPECT_EQ(extension_C->id(), requests[1]);

  // Dismiss request for "Alpha".
  // Only request left is [Gamma].
  permissions_manager()->UserDismissedHostAccessRequest(web_contents, tab_id,
                                                        extension_A->id());
  requests = menu_model()->host_access_requests();
  ASSERT_EQ(1u, requests.size());
  EXPECT_EQ(extension_C->id(), requests[0]);
}
