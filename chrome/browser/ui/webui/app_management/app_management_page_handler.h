// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "chrome/browser/ui/webui/app_management/app_management_shelf_delegate_chromeos.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#endif

class Profile;

class AppManagementPageHandler : public app_management::mojom::PageHandler,
#if defined(OS_CHROMEOS)
                                 public ArcAppListPrefs::Observer,
#endif  // OS_CHROMEOS
                                 public apps::AppRegistryCache::Observer {
 public:
  AppManagementPageHandler(
      mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
      mojo::PendingRemote<app_management::mojom::Page> page,
      Profile* profile);
  ~AppManagementPageHandler() override;

#if defined(OS_CHROMEOS)
  static bool IsCurrentArcVersionSupported(Profile* profile);
#endif  // OS_CHROMEOS

  void OnPinnedChanged(const std::string& app_id, bool pinned);
#if defined(OS_CHROMEOS)
  void OnArcVersionChanged(int androidVersion);
#endif  // OS_CHROMEOS

  // app_management::mojom::PageHandler:
  void GetApps(GetAppsCallback callback) override;
  void GetExtensionAppPermissionMessages(
      const std::string& app_id,
      GetExtensionAppPermissionMessagesCallback callback) override;
  void SetPinned(const std::string& app_id,
                 apps::mojom::OptionalBool pinned) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void Uninstall(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;

 private:
  app_management::mojom::AppPtr CreateUIAppPtr(const apps::AppUpdate& update);

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

#if defined(OS_CHROMEOS)
  // ArcAppListPrefs::Observer:
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageModified(
      const arc::mojom::ArcPackageInfo& package_info) override;
#endif  // OS_CHROMEOS

  mojo::Receiver<app_management::mojom::PageHandler> receiver_;

  mojo::Remote<app_management::mojom::Page> page_;

  Profile* profile_;

#if defined(OS_CHROMEOS)
  ScopedObserver<ArcAppListPrefs, ArcAppListPrefs::Observer>
      arc_app_list_prefs_observer_{this};
  AppManagementShelfDelegate shelf_delegate_{this};
#endif  // OS_CHROMEOS

  DISALLOW_COPY_AND_ASSIGN(AppManagementPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_
