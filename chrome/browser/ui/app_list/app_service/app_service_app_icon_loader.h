// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_APP_ICON_LOADER_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_APP_ICON_LOADER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "chrome/services/app_service/public/cpp/app_update.h"
#include "ui/gfx/image/image_skia.h"

class Profile;

// An AppIconLoader that loads icons for app service apps.
class AppServiceAppIconLoader : public AppIconLoader,
                                private apps::AppRegistryCache::Observer {
 public:
  AppServiceAppIconLoader(Profile* profile,
                          int resource_size_in_dip,
                          AppIconLoaderDelegate* delegate);
  ~AppServiceAppIconLoader() override;

  // AppIconLoader overrides:
  bool CanLoadImageForApp(const std::string& app_id) override;
  void FetchImage(const std::string& app_id) override;
  void ClearImage(const std::string& app_id) override;
  void UpdateImage(const std::string& app_id) override;

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  // Calls AppService LoadIcon to load icons.
  void CallLoadIcon(const std::string& app_id, bool allow_placeholder_icon);

  // Callback invoked when the icon is loaded.
  void OnLoadIcon(const std::string& app_id,
                  apps::mojom::IconValuePtr icon_value);

  using AppIDToIconMap = std::map<std::string, gfx::ImageSkia>;

  // Maps from app id to the icon to track the icons added via FetchImage.
  AppIDToIconMap icon_map_;

  base::WeakPtrFactory<AppServiceAppIconLoader> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppServiceAppIconLoader);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_APP_ICON_LOADER_H_
