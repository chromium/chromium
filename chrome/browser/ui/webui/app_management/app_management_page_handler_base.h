// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_BASE_H_
#define CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_BASE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

class Profile;

class AppManagementPageHandlerBase : public app_management::mojom::PageHandler,
                                     public apps::AppRegistryCache::Observer {
 public:
  //  Handles platform specific tasks.
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    virtual gfx::NativeWindow GetUninstallAnchorWindow() const = 0;
  };

  AppManagementPageHandlerBase(const AppManagementPageHandlerBase&) = delete;
  AppManagementPageHandlerBase& operator=(const AppManagementPageHandlerBase&) =
      delete;

  ~AppManagementPageHandlerBase() override;

  // app_management::mojom::PageHandler:
  void GetApps(GetAppsCallback callback) override;
  void GetApp(const std::string& app_id, GetAppCallback callback) override;
  void SetPermission(const std::string& app_id,
                     apps::PermissionPtr permission) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void SetFileHandlingEnabled(const std::string& app_id, bool enabled) override;

 protected:
  AppManagementPageHandlerBase(
      mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
      mojo::PendingRemote<app_management::mojom::Page> page,
      Profile* profile);

  // Creates an AppPtr for the given `app_id`. Can be overridden to add
  // additional platform-specific data to the App. Returns nullptr if the given
  // app is not installed or should not be shown in App Management.
  virtual app_management::mojom::AppPtr CreateApp(const std::string& app_id);

  // Notify the WebUI frontend that the app with a given `app_id` has changed on
  // the backend. Will generate a new AppPtr and send it to the frontend.
  void NotifyAppChanged(const std::string& app_id);

  Profile* profile() { return profile_; }

 private:
  app_management::mojom::AppPtr CreateAppFromAppUpdate(
      const apps::AppUpdate& update);

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  mojo::Receiver<app_management::mojom::PageHandler> receiver_;

  mojo::Remote<app_management::mojom::Page> page_;

  raw_ptr<Profile> profile_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::WeakPtrFactory<AppManagementPageHandlerBase> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_BASE_H_
