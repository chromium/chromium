// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_service/app_service_app_icon_loader.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "chrome/services/app_service/public/cpp/app_update.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

AppServiceAppIconLoader::AppServiceAppIconLoader(
    Profile* profile,
    int resource_size_in_dip,
    AppIconLoaderDelegate* delegate)
    : AppIconLoader(profile, resource_size_in_dip, delegate) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  if (proxy) {
    Observe(&proxy->AppRegistryCache());
  }
}

AppServiceAppIconLoader::~AppServiceAppIconLoader() = default;

bool AppServiceAppIconLoader::CanLoadImageForApp(const std::string& app_id) {
  return true;
}

void AppServiceAppIconLoader::FetchImage(const std::string& app_id) {
  AppIDToIconMap::const_iterator it = icon_map_.find(app_id);
  if (it != icon_map_.end()) {
    delegate()->OnAppImageUpdated(app_id, it->second);
    return;
  }

  icon_map_[app_id] = gfx::ImageSkia();

  constexpr bool allow_placeholder_icon = true;
  CallLoadIcon(app_id, allow_placeholder_icon);
}

void AppServiceAppIconLoader::ClearImage(const std::string& app_id) {
  icon_map_.erase(app_id);
}

void AppServiceAppIconLoader::UpdateImage(const std::string& app_id) {
  AppIDToIconMap::const_iterator it = icon_map_.find(app_id);
  if (it == icon_map_.end()) {
    return;
  }

  delegate()->OnAppImageUpdated(app_id, it->second);
}

void AppServiceAppIconLoader::OnAppUpdate(const apps::AppUpdate& update) {
  if (!update.IconKeyChanged()) {
    return;
  }

  // Only load the icon that has been added to icon_map_.
  const std::string& app_id = update.AppId();
  AppIDToIconMap::const_iterator it = icon_map_.find(app_id);
  if (it == icon_map_.end()) {
    return;
  }

  constexpr bool allow_placeholder_icon = true;
  CallLoadIcon(app_id, allow_placeholder_icon);
}

void AppServiceAppIconLoader::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void AppServiceAppIconLoader::CallLoadIcon(const std::string& app_id,
                                           bool allow_placeholder_icon) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  if (!proxy) {
    return;
  }

  // When Crostini generates shelf id as the app_id, which couldn't match to an
  // app, the default penguin icon should be loaded.
  if (base::StartsWith(app_id, crostini::kCrostiniAppIdPrefix,
                       base::CompareCase::SENSITIVE)) {
    apps::mojom::IconKeyPtr icon_key = apps::mojom::IconKey::New();
    proxy->LoadIconFromIconKey(
        apps::mojom::AppType::kCrostini, std::string(), std::move(icon_key),
        apps::mojom::IconCompression::kUncompressed, icon_size_in_dip(),
        allow_placeholder_icon,
        base::BindOnce(&AppServiceAppIconLoader::OnLoadIcon,
                       weak_ptr_factory_.GetWeakPtr(), app_id));
    return;
  }

  apps::mojom::AppType app_type = proxy->AppRegistryCache().GetAppType(app_id);
  if (app_type == apps::mojom::AppType::kUnknown) {
    return;
  }

  proxy->LoadIcon(app_type, app_id, apps::mojom::IconCompression::kUncompressed,
                  icon_size_in_dip(), allow_placeholder_icon,
                  base::BindOnce(&AppServiceAppIconLoader::OnLoadIcon,
                                 weak_ptr_factory_.GetWeakPtr(), app_id));
}

void AppServiceAppIconLoader::OnLoadIcon(const std::string& app_id,
                                         apps::mojom::IconValuePtr icon_value) {
  if (icon_value->icon_compression !=
      apps::mojom::IconCompression::kUncompressed) {
    return;
  }

  // Only load the icon that exists in icon_map_. The App could be removed from
  // icon_map_ after calling CallLoadIcon, so check it again.
  AppIDToIconMap::const_iterator it = icon_map_.find(app_id);
  if (it == icon_map_.end()) {
    return;
  }

  gfx::ImageSkia image = icon_value->uncompressed;
  icon_map_[app_id] = image;
  delegate()->OnAppImageUpdated(app_id, image);

  if (icon_value->is_placeholder_icon) {
    constexpr bool allow_placeholder_icon = false;
    CallLoadIcon(app_id, allow_placeholder_icon);
  }
}
