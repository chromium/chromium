// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_CHROMEOS_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler_base.h"
#include "chrome/browser/ui/webui/app_management/app_management_shelf_delegate_chromeos.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

class Profile;

// PageHandler for the ChromeOS App Management page.
class AppManagementPageHandlerChromeOs
    : public AppManagementPageHandlerBase,
      public apps::PreferredAppsListHandle::Observer {
 public:
  AppManagementPageHandlerChromeOs(
      mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
      mojo::PendingRemote<app_management::mojom::Page> page,
      Profile* profile,
      AppManagementPageHandlerBase::Delegate& delegate);

  AppManagementPageHandlerChromeOs(const AppManagementPageHandlerChromeOs&) =
      delete;
  AppManagementPageHandlerChromeOs& operator=(
      const AppManagementPageHandlerChromeOs&) = delete;

  ~AppManagementPageHandlerChromeOs() override;

  // Called by AppManagementShelfDelegate.
  void OnPinnedChanged(const std::string& app_id, bool pinned);

  // app_management::mojom::PageHandler:
  void GetSubAppToParentMap(GetSubAppToParentMapCallback callback) override;
  void GetExtensionAppPermissionMessages(
      const std::string& app_id,
      GetExtensionAppPermissionMessagesCallback callback) override;
  void SetPinned(const std::string& app_id, bool pinned) override;
  void SetResizeLocked(const std::string& app_id, bool locked) override;
  void Uninstall(const std::string& app_id) override;
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
  void ShowDefaultAppAssociationsUi() override;
  void OpenStorePage(const std::string& app_id) override;
  void SetAppLocale(const std::string& app_id,
                    const std::string& locale_tag) override;

  // apps::PreferredAppsListHandle::Observer overrides:
  void OnPreferredAppChanged(const std::string& app_id,
                             bool is_preferred_app) override;
  void OnPreferredAppsListWillBeDestroyed(
      apps::PreferredAppsListHandle* handle) override;

  // AppManagementPageHandlerBase:
  app_management::mojom::AppPtr CreateApp(const std::string& app_id) override;

 private:
  AppManagementShelfDelegate shelf_delegate_;
  const raw_ref<Delegate> delegate_;

  base::ScopedObservation<apps::PreferredAppsListHandle,
                          apps::PreferredAppsListHandle::Observer>
      preferred_apps_list_handle_observer_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_APP_MANAGEMENT_PAGE_HANDLER_CHROMEOS_H_
