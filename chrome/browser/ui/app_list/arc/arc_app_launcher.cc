// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_launcher.h"

#include <memory>
#include <utility>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/events/event_constants.h"

ArcAppLauncher::ArcAppLauncher(content::BrowserContext* context,
                               const std::string& app_id,
                               const base::Optional<std::string>& launch_intent,
                               bool deferred_launch_allowed,
                               int64_t display_id,
                               apps::mojom::LaunchSource launch_source)
    : context_(context),
      app_id_(app_id),
      launch_intent_(std::move(launch_intent)),
      deferred_launch_allowed_(deferred_launch_allowed),
      display_id_(display_id),
      launch_source_(launch_source) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id_);
  if (!app_info ||
      !MaybeLaunchApp(app_id, *app_info, apps::mojom::Readiness::kUnknown))
    prefs->AddObserver(this);

  auto* profile = Profile::FromBrowserContext(context_);
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  Observe(&apps::AppServiceProxyFactory::GetForProfile(profile)
               ->AppRegistryCache());
}

ArcAppLauncher::~ArcAppLauncher() {
  if (!app_launched_) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
    if (prefs)
      prefs->RemoveObserver(this);
    VLOG(2) << "App " << app_id_ << "was not launched.";
  }
}

void ArcAppLauncher::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  MaybeLaunchApp(app_id, app_info, apps::mojom::Readiness::kUnknown);
}

void ArcAppLauncher::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  MaybeLaunchApp(app_id, app_info, apps::mojom::Readiness::kUnknown);
}

void ArcAppLauncher::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() != app_id_ ||
      update.Readiness() != apps::mojom::Readiness::kReady) {
    return;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs);

  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id_);
  if (!app_info)
    return;

  MaybeLaunchApp(app_id_, *app_info, apps::mojom::Readiness::kReady);
}

void ArcAppLauncher::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

bool ArcAppLauncher::MaybeLaunchApp(const std::string& app_id,
                                    const ArcAppListPrefs::AppInfo& app_info,
                                    apps::mojom::Readiness readiness) {
  if (app_launched_)
    return true;

  if (app_id != app_id_ || (!app_info.ready && !deferred_launch_allowed_) ||
      app_info.suspended) {
    return false;
  }

  auto* profile = Profile::FromBrowserContext(context_);
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  if (readiness == apps::mojom::Readiness::kUnknown) {
    if (proxy->AppRegistryCache().GetAppType(app_id) ==
        apps::mojom::AppType::kUnknown) {
      return false;
    }

    apps::mojom::Readiness readiness = apps::mojom::Readiness::kUnknown;
    proxy->AppRegistryCache().ForOneApp(
        app_id, [&readiness](const apps::AppUpdate& update) {
          readiness = update.Readiness();
        });

    if (readiness != apps::mojom::Readiness::kReady)
      return false;
  } else if (readiness != apps::mojom::Readiness::kReady) {
    return false;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs && prefs->GetApp(app_id_));
  prefs->RemoveObserver(this);
  Observe(nullptr);

  if (launch_intent_) {
    if (!arc::LaunchAppWithIntent(context_, app_id_, launch_intent_,
                                  ui::EF_NONE,
                                  arc::UserInteractionType::NOT_USER_INITIATED,
                                  arc::MakeWindowInfo(display_id_))) {
      VLOG(2) << "Failed to launch app: " + app_id_ + ".";
    }
  } else {
    proxy->Launch(app_id_, ui::EF_NONE, launch_source_,
                  apps::MakeWindowInfo(display_id_));
  }

  app_launched_ = true;
  return true;
}
