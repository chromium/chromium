// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_

#include "base/macros.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom-forward.h"
#include "chrome/browser/ui/webui/app_management/app_management_shelf_delegate_chromeos.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

class AppManagementPageHandler : public app_management::mojom::PageHandler,
                                 public apps::AppRegistryCache::Observer,
                                 public apps::PreferredAppsList::Observer {
 public:
  AppManagementPageHandler(
      mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
      mojo::PendingRemote<app_management::mojom::Page> page,
      Profile* profile);
  ~AppManagementPageHandler() override;

  void OnPinnedChanged(const std::string& app_id, bool pinned);

  // app_management::mojom::PageHandler:
  void GetApps(GetAppsCallback callback) override;
  void GetExtensionAppPermissionMessages(
      const std::string& app_id,
      GetExtensionAppPermissionMessagesCallback callback) override;
  void SetPinned(const std::string& app_id,
                 apps::mojom::OptionalBool pinned) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void SetResizeLocked(const std::string& app_id, bool locked) override;
  void Uninstall(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void SetPreferredApp(const std::string& app_id,
                       bool is_preferred_app) override;

 private:
  app_management::mojom::AppPtr CreateUIAppPtr(const apps::AppUpdate& update);
  std::vector<std::string> GetSupportedLinksList(const std::string& app_id);

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // apps::PreferredAppsList::Observer overrides:
  void OnPreferredAppChanged(const std::string& app_id,
                             bool is_preferred_app) override;
  void OnPreferredAppsListWillBeDestroyed(
      apps::PreferredAppsList* list) override;

  mojo::Receiver<app_management::mojom::PageHandler> receiver_;

  mojo::Remote<app_management::mojom::Page> page_;

  Profile* profile_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  AppManagementShelfDelegate shelf_delegate_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  apps::PreferredAppsList& preferred_apps_list_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::ScopedObservation<apps::PreferredAppsList,
                          apps::PreferredAppsList::Observer>
      preferred_apps_list_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(AppManagementPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_
