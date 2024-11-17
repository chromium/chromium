// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/people/parental_controls_handler.h"

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/child_accounts/child_user_service.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_ui.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_constants.h"
#include "url/gurl.h"

namespace ash::settings {

const char kFamilyLinkSiteURL[] = "https://families.google.com/families";

ParentalControlsHandler::ParentalControlsHandler(Profile* profile)
    : profile_(profile) {}

ParentalControlsHandler::~ParentalControlsHandler() = default;

void ParentalControlsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "showAddSupervisionDialog",
      base::BindRepeating(
          &ParentalControlsHandler::HandleShowAddSupervisionDialog,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "launchFamilyLinkSettings",
      base::BindRepeating(
          &ParentalControlsHandler::HandleLaunchFamilyLinkSettings,
          base::Unretained(this)));
}

void ParentalControlsHandler::OnJavascriptAllowed() {}
void ParentalControlsHandler::OnJavascriptDisallowed() {}

void ParentalControlsHandler::HandleShowAddSupervisionDialog(
    const base::Value::List& args) {
  DCHECK(args.empty());
  AddSupervisionDialog::Show();
}

void ParentalControlsHandler::HandleLaunchFamilyLinkSettings(
    const base::Value::List& args) {
  DCHECK(args.empty());

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);

  apps::AppRegistryCache& registry = proxy->AppRegistryCache();
  const std::string app_id = arc::ArcPackageNameToAppId(
      ChildUserService::kFamilyLinkHelperAppPackageName, profile_);
  if (registry.GetAppType(app_id) != apps::AppType::kUnknown) {
    // Launch FLH app since it is available.
    proxy->Launch(
        app_id, ui::EF_NONE, apps::LaunchSource::kFromParentalControls,
        std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId));
    return;
  }

  // No FLH app installed, so try to launch Play Store to FLH app install page.
  if (registry.GetAppType(arc::kPlayStoreAppId) != apps::AppType::kUnknown) {
    proxy->LaunchAppWithUrl(
        arc::kPlayStoreAppId, ui::EF_NONE,
        GURL(ChildUserService::kFamilyLinkHelperAppPlayStoreURL),
        apps::LaunchSource::kFromChromeInternal);
    return;
  }

  // As a last resort, launch browser to the family link site.
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kFamilyLinkSiteURL),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewWindow);
}

}  // namespace ash::settings
