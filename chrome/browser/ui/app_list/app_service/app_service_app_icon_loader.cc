// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_service/app_service_app_icon_loader.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_shelf_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace {

const char kArcIntentHelperAppId[] = "lomchfnmkmhfhbibboadbgabihofenaa";

// Returns the app id from the app id or the shelf group id.
std::string GetAppId(Profile* profile, const std::string& id) {
  const arc::ArcAppShelfId arc_app_shelf_id =
      arc::ArcAppShelfId::FromString(id);
  if (!arc_app_shelf_id.valid() || !arc_app_shelf_id.has_shelf_group_id()) {
    return id;
  }

  return arc::GetAppFromAppOrGroupId(profile, id);
}

}  // namespace

// static
bool AppServiceAppIconLoader::CanLoadImage(Profile* profile,
                                           const std::string& id) {
  const std::string app_id = GetAppId(profile, id);

  // Skip the ARC intent helper, the system Android app that proxies links to
  // Chrome, which should be hidden.
  if (app_id == kArcIntentHelperAppId) {
    return false;
  }

  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }

  // Support icon loading for apps registered in AppService or Crostini apps
  // with the prefix "crostini:".
  if (apps::AppServiceProxyFactory::GetForProfile(profile)
              ->AppRegistryCache()
              .GetAppType(app_id) != apps::mojom::AppType::kUnknown ||
      crostini::IsUnmatchedCrostiniShelfAppId(app_id)) {
    return true;
  }

  return false;
}

AppServiceAppIconLoader::AppServiceAppIconLoader(
    Profile* profile,
    int resource_size_in_dip,
    AppIconLoaderDelegate* delegate)
    : AppIconLoader(profile, resource_size_in_dip, delegate) {
  Observe(&apps::AppServiceProxyFactory::GetForProfile(profile)
               ->AppRegistryCache());
}

AppServiceAppIconLoader::~AppServiceAppIconLoader() = default;

bool AppServiceAppIconLoader::CanLoadImageForApp(const std::string& id) {
  return AppServiceAppIconLoader::CanLoadImage(profile(), id);
}

void AppServiceAppIconLoader::FetchImage(const std::string& id) {
  const std::string app_id = GetAppId(profile(), id);

  AppIDToIconMap::const_iterator it = icon_map_.find(id);
  if (it != icon_map_.end()) {
    if (!it->second.isNull()) {
      delegate()->OnAppImageUpdated(id, it->second);
    }
    return;
  }

  icon_map_[id] = gfx::ImageSkia();
  shelf_app_id_map_[app_id].insert(id);
  constexpr bool allow_placeholder_icon = true;
  CallLoadIcon(app_id, allow_placeholder_icon);
}

void AppServiceAppIconLoader::ClearImage(const std::string& id) {
  const std::string app_id = GetAppId(profile(), id);
  if (base::Contains(shelf_app_id_map_, app_id)) {
    shelf_app_id_map_[app_id].erase(id);
    if (shelf_app_id_map_[app_id].empty()) {
      shelf_app_id_map_.erase(app_id);
    }
  }

  icon_map_.erase(id);
}

void AppServiceAppIconLoader::UpdateImage(const std::string& id) {
  AppIDToIconMap::const_iterator it = icon_map_.find(id);
  if (it == icon_map_.end() || it->second.isNull()) {
    return;
  }

  delegate()->OnAppImageUpdated(id, it->second);
}

void AppServiceAppIconLoader::OnAppUpdate(const apps::AppUpdate& update) {
  if (!update.IconKeyChanged()) {
    return;
  }

  if (!Exist(update.AppId())) {
    return;
  }

  constexpr bool allow_placeholder_icon = true;
  CallLoadIcon(update.AppId(), allow_placeholder_icon);
}

void AppServiceAppIconLoader::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void AppServiceAppIconLoader::CallLoadIcon(const std::string& app_id,
                                           bool allow_placeholder_icon) {
  apps::AppServiceProxyChromeOs* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());

  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;

  // When Crostini generates shelf id as the app_id, which couldn't match to an
  // app, the default penguin icon should be loaded.
  if (crostini::IsUnmatchedCrostiniShelfAppId(app_id)) {
    apps::mojom::IconKeyPtr icon_key = apps::mojom::IconKey::New();
    proxy->LoadIconFromIconKey(
        apps::mojom::AppType::kCrostini, app_id, std::move(icon_key), icon_type,
        icon_size_in_dip(), allow_placeholder_icon,
        base::BindOnce(&AppServiceAppIconLoader::OnLoadIcon,
                       weak_ptr_factory_.GetWeakPtr(), app_id));
    return;
  }

  apps::mojom::AppType app_type = proxy->AppRegistryCache().GetAppType(app_id);
  if (app_type == apps::mojom::AppType::kUnknown) {
    return;
  }

  proxy->LoadIcon(app_type, app_id, icon_type, icon_size_in_dip(),
                  allow_placeholder_icon,
                  base::BindOnce(&AppServiceAppIconLoader::OnLoadIcon,
                                 weak_ptr_factory_.GetWeakPtr(), app_id));
}

void AppServiceAppIconLoader::OnLoadIcon(const std::string& app_id,
                                         apps::mojom::IconValuePtr icon_value) {
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  if (icon_value->icon_type != icon_type) {
    return;
  }

  // Only load the icon that exists in icon_map_. The App could be removed from
  // icon_map_ after calling CallLoadIcon, so check it again.
  if (!Exist(app_id)) {
    return;
  }

  for (auto& id : shelf_app_id_map_[app_id]) {
    AppIDToIconMap::const_iterator it = icon_map_.find(id);
    if (it == icon_map_.end()) {
      continue;
    }
    gfx::ImageSkia image = icon_value->uncompressed;
    icon_map_[id] = image;
    delegate()->OnAppImageUpdated(id, image);
  }

  if (icon_value->is_placeholder_icon) {
    constexpr bool allow_placeholder_icon = false;
    CallLoadIcon(app_id, allow_placeholder_icon);
  }
}

bool AppServiceAppIconLoader::Exist(const std::string& app_id) {
  if (!base::Contains(shelf_app_id_map_, app_id)) {
    return false;
  }

  for (auto& id : shelf_app_id_map_[app_id]) {
    AppIDToIconMap::const_iterator it = icon_map_.find(id);
    if (it == icon_map_.end()) {
      continue;
    }
    return true;
  }
  return false;
}
