// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
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
}

ExtensionsMenuViewModel::~ExtensionsMenuViewModel() {
  platform_delegate_->DetachFromModel();
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
