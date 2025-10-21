// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/permissions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view_platform_delegate_views.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

// Returns the extension corresponding to `extension_id` on `profile`.
const extensions::Extension* GetExtension(
    Profile& profile,
    const extensions::ExtensionId& extension_id) {
  return extensions::ExtensionRegistry::Get(&profile)
      ->enabled_extensions()
      .GetByID(extension_id);
}

// Returns whether user can select the site access for `extension` on
// `web_contents`.
bool CanUserCustomizeExtensionSiteAccess(
    const extensions::Extension& extension,
    Profile& profile,
    const ToolbarActionsModel& toolbar_model,
    content::WebContents& web_contents) {
  const GURL& url = web_contents.GetLastCommittedURL();
  if (toolbar_model.IsRestrictedUrl(url)) {
    // We don't allow customization of restricted sites (e.g.
    // chrome://settings).
    return false;
  }

  if (extension.permissions_data()->IsPolicyBlockedHost(url)) {
    // Users can't customize the site access of policy-blocked sites.
    return false;
  }

  if (extensions::ExtensionSystem::Get(&profile)
          ->management_policy()
          ->HasEnterpriseForcedAccess(extension)) {
    // Users can't customize the site access of enterprise-installed extensions.
    return false;
  }

  // The extension wants site access if it at least wants "on click" access.
  auto* permissions_manager = PermissionsManager::Get(&profile);
  bool extension_wants_access = permissions_manager->CanUserSelectSiteAccess(
      extension, url, PermissionsManager::UserSiteAccess::kOnClick);
  if (!extension_wants_access) {
    // Users can't customize site access of extensions that don't want access to
    // begin with.
    return false;
  }

  // Users can only customize site access when they have allowed all extensions
  // to be customizable on the site.
  return permissions_manager->GetUserSiteSetting(
             web_contents.GetPrimaryMainFrame()->GetLastCommittedOrigin()) ==
         PermissionsManager::UserSiteSetting::kCustomizeByExtension;
}

void LogSiteAccessUpdate(PermissionsManager::UserSiteAccess site_access) {
  switch (site_access) {
    case PermissionsManager::UserSiteAccess::kOnClick:
      base::RecordAction(
          base::UserMetricsAction("Extensions.Menu.OnClickSelected"));
      break;
    case PermissionsManager::UserSiteAccess::kOnSite:
      base::RecordAction(
          base::UserMetricsAction("Extensions.Menu.OnSiteSelected"));
      break;
    case PermissionsManager::UserSiteAccess::kOnAllSites:
      base::RecordAction(
          base::UserMetricsAction("Extensions.Menu.OnAllSitesSelected"));
      break;
    default:
      NOTREACHED() << "Unknown site access";
  }
}

void LogSiteSettingsUpdate(PermissionsManager::UserSiteSetting site_setting) {
  switch (site_setting) {
    case PermissionsManager::UserSiteSetting::kCustomizeByExtension:
      base::RecordAction(
          base::UserMetricsAction("Extensions.Menu.AllowByExtensionSelected"));
      break;
    case PermissionsManager::UserSiteSetting::kBlockAllExtensions:
      base::RecordAction(
          base::UserMetricsAction("Extensions.Menu.ExtensionsBlockedSelected"));
      break;
    case PermissionsManager::UserSiteSetting::kGrantAllExtensions:
    default:
      NOTREACHED() << "Invalid site setting update";
  }
}

}  // namespace

ExtensionsMenuViewModel::ExtensionsMenuViewModel(
    BrowserWindowInterface* browser,
    std::unique_ptr<ExtensionsMenuViewPlatformDelegate> platform_delegate)
    : browser_(browser), platform_delegate_(std::move(platform_delegate)) {
  platform_delegate_->AttachToModel(this);

  permissions_manager_observation_.Observe(
      extensions::PermissionsManager::Get(browser_->GetProfile()));
}

ExtensionsMenuViewModel::~ExtensionsMenuViewModel() {
  platform_delegate_->DetachFromModel();
}

void ExtensionsMenuViewModel::OnHostAccessRequestAdded(
    const extensions::ExtensionId& extension_id,
    int tab_id) {
  // Ignore requests for other tabs.
  auto* web_contents = GetActiveWebContents();
  int current_tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  if (tab_id != current_tab_id) {
    return;
  }

  // Ignore requests that are not active.
  auto* permissions_manager =
      extensions::PermissionsManager::Get(browser_->GetProfile());
  if (!permissions_manager->HasActiveHostAccessRequest(tab_id, extension_id)) {
    return;
  }

  platform_delegate_->OnAccessRequestAdded(extension_id, web_contents);
}

void ExtensionsMenuViewModel::UpdateSiteAccess(
    const extensions::ExtensionId& extension_id,
    PermissionsManager::UserSiteAccess site_access) {
  LogSiteAccessUpdate(site_access);

  Profile* profile = browser_->GetProfile();
  SitePermissionsHelper permissions(profile);
  permissions.UpdateSiteAccess(*GetExtension(*profile, extension_id),
                               GetActiveWebContents(), site_access);
}

void ExtensionsMenuViewModel::GrantSiteAccess(
    const extensions::ExtensionId& extension_id) {
  auto* profile = browser_->GetProfile();
  const extensions::Extension* extension = GetExtension(*profile, extension_id);
  content::WebContents* web_contents = GetActiveWebContents();
  auto* toolbar_model = ToolbarActionsModel::Get(profile);
  auto url = web_contents->GetLastCommittedURL();
  auto* permissions_manager = PermissionsManager::Get(profile);

  // Can only grant site access when user can customize the extension's site
  // access and it's currently on click.
  auto current_site_access =
      permissions_manager->GetUserSiteAccess(*extension, url);
  CHECK(CanUserCustomizeExtensionSiteAccess(*extension, *profile,
                                            *toolbar_model, *web_contents));
  CHECK_EQ(current_site_access, PermissionsManager::UserSiteAccess::kOnClick);

  // Update site access when extension requested host permissions for the
  // current site (that is, site access was withheld).
  PermissionsManager::ExtensionSiteAccess extension_site_access =
      permissions_manager->GetSiteAccess(*extension, url);
  if (extension_site_access.withheld_site_access ||
      extension_site_access.withheld_all_sites_access) {
    // Restore to previous access by looking whether broad site access was
    // previously granted.
    PermissionsManager::UserSiteAccess new_site_access =
        permissions_manager->HasPreviousBroadSiteAccess(extension_id)
            ? PermissionsManager::UserSiteAccess::kOnAllSites
            : PermissionsManager::UserSiteAccess::kOnSite;
    SitePermissionsHelper permissions_helper(profile);
    permissions_helper.UpdateSiteAccess(*extension, web_contents,
                                        new_site_access);
    return;
  }

  // Otherwise, grant one-time access (e.g. extension with activeTab is
  // granted access).
  extensions::ExtensionActionRunner* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  if (action_runner) {
    action_runner->GrantTabPermissions({extension});
  }
}

void ExtensionsMenuViewModel::RevokeSiteAccess(
    const extensions::ExtensionId& extension_id) {
  auto* profile = browser_->GetProfile();
  const extensions::Extension* extension = GetExtension(*profile, extension_id);
  content::WebContents* web_contents = GetActiveWebContents();
  auto* toolbar_model = ToolbarActionsModel::Get(profile);

  // Can only revoke site access when user can customize the extension's site
  // access.
  CHECK(CanUserCustomizeExtensionSiteAccess(*extension, *profile,
                                            *toolbar_model, *web_contents));

  auto url = web_contents->GetLastCommittedURL();
  auto* permissions_manager = PermissionsManager::Get(profile);
  auto current_site_access =
      permissions_manager->GetUserSiteAccess(*extension, url);
  PermissionsManager::ExtensionSiteAccess extension_site_access =
      permissions_manager->GetSiteAccess(*extension, url);

  // Update site access to "on click" when extension requested, and was granted,
  // host permissions for the current site (that is, extension has site access).
  if (extension_site_access.has_site_access ||
      extension_site_access.has_all_sites_access) {
    CHECK_NE(current_site_access, PermissionsManager::UserSiteAccess::kOnClick);
    SitePermissionsHelper permissions_helper(profile);
    permissions_helper.UpdateSiteAccess(
        *extension, web_contents, PermissionsManager::UserSiteAccess::kOnClick);
    return;
  }

  // Otherwise, extension has one-time access and we need to clear tab
  // permissions (e.g extension with activeTab was granted one-time access).
  CHECK_EQ(current_site_access, PermissionsManager::UserSiteAccess::kOnClick);
  extensions::ActiveTabPermissionGranter::FromWebContents(web_contents)
      ->ClearActiveExtensionAndNotify(extension_id);

  auto* action_runner =
      extensions::ExtensionActionRunner::GetForWebContents(web_contents);
  if (action_runner) {
    action_runner->ShowReloadPageBubble({GetExtension(*profile, extension_id)});
  }
}

void ExtensionsMenuViewModel::UpdateSiteSetting(
    extensions::PermissionsManager::UserSiteSetting site_setting) {
  content::WebContents* web_contents = GetActiveWebContents();
  const url::Origin& origin =
      GetActiveWebContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  extensions::TabHelper::FromWebContents(web_contents)
      ->SetReloadRequired(site_setting);
  PermissionsManager::Get(browser_->GetProfile())
      ->UpdateUserSiteSetting(origin, site_setting);

  LogSiteSettingsUpdate(site_setting);
}

content::WebContents* ExtensionsMenuViewModel::GetActiveWebContents() {
  return TabListInterface::From(browser_)->GetActiveTab()->GetContents();
}
