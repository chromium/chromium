// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view_platform_delegate_views.h"
#include "components/tabs/public/tab_interface.h"
#include "extensions/browser/extension_registry.h"

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

content::WebContents* ExtensionsMenuViewModel::GetActiveWebContents() {
  return TabListInterface::From(browser_)->GetActiveTab()->GetContents();
}
