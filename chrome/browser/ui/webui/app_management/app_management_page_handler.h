// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/app_management/app_management_shelf_delegate_chromeos.h"
#endif

class Profile;

class AppManagementPageHandler : public app_management::mojom::PageHandler,
                                 public apps::AppRegistryCache::Observer,
                                 public apps::PreferredAppsListHandle::Observer,
                                 public web_app::WebAppRegistrarObserver {
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

  AppManagementPageHandler(
      mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
      mojo::PendingRemote<app_management::mojom::Page> page,
      Profile* profile,
      Delegate& delegate);

  AppManagementPageHandler(const AppManagementPageHandler&) = delete;
  AppManagementPageHandler& operator=(const AppManagementPageHandler&) = delete;

  ~AppManagementPageHandler() override;

  void OnPinnedChanged(const std::string& app_id, bool pinned);

  // app_management::mojom::PageHandler:
  void GetApps(GetAppsCallback callback) override;
  void GetApp(const std::string& app_id, GetAppCallback callback) override;
  void GetSubAppToParentMap(GetSubAppToParentMapCallback callback) override;
  void GetExtensionAppPermissionMessages(
      const std::string& app_id,
      GetExtensionAppPermissionMessagesCallback callback) override;
  void SetPinned(const std::string& app_id,
                 app_management::mojom::OptionalBool pinned) override;
  void SetPermission(const std::string& app_id,
                     apps::PermissionPtr permission) override;
  void SetResizeLocked(const std::string& app_id, bool locked) override;
  void Uninstall(const std::string& app_id) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void SetPreferredApp(const std::string& app_id,
                       bool is_preferred_app) override;
  void GetOverlappingPreferredApps(
      const std::string& app_id,
      GetOverlappingPreferredAppsCallback callback) override;
  void UpdateAppSize(const std::string& app_id) override;
  void SetWindowMode(const std::string& app_id,
                     apps::WindowMode window_mode) override;
  void SetRunOnOsLoginMode(
      const std::string& app_id,
      apps::RunOnOsLoginMode run_on_os_login_mode) override;
  void SetFileHandlingEnabled(const std::string& app_id, bool enabled) override;
  void ShowDefaultAppAssociationsUi() override;
  void OpenStorePage(const std::string& app_id) override;

  // web_app::WebAppRegistrarObserver:
  void OnWebAppFileHandlerApprovalStateChanged(
      const webapps::AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;

  // The following observers are used for user link capturing on W/M/L platforms
  // to observe user link capturing preferences being changed
  // in the registrar, so as to propagate the changes to the app-settings/ page
  // to change the UI dynamically.
#if !BUILDFLAG(IS_CHROMEOS)
  void OnWebAppUserLinkCapturingPreferencesChanged(const webapps::AppId& app_id,
                                                   bool is_preferred) override;
#endif  // !BUILDFLAG(IS_CHROMEOS)

 private:
  app_management::mojom::AppPtr CreateUIAppPtr(const apps::AppUpdate& update);

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // apps::PreferredAppsListHandle::Observer overrides:
  void OnPreferredAppChanged(const std::string& app_id,
                             bool is_preferred_app) override;
  void OnPreferredAppsListWillBeDestroyed(
      apps::PreferredAppsListHandle* handle) override;

  mojo::Receiver<app_management::mojom::PageHandler> receiver_;

  mojo::Remote<app_management::mojom::Page> page_;

  raw_ptr<Profile> profile_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  AppManagementShelfDelegate shelf_delegate_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  const raw_ref<Delegate> delegate_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::ScopedObservation<apps::PreferredAppsListHandle,
                          apps::PreferredAppsListHandle::Observer>
      preferred_apps_list_handle_observer_{this};

  base::ScopedObservation<web_app::WebAppRegistrar,
                          web_app::WebAppRegistrarObserver>
      registrar_observation_{this};

  base::WeakPtrFactory<AppManagementPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_H_
