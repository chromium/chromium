// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_WEB_APP_SETTINGS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_WEB_APP_SETTINGS_PAGE_HANDLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler_base.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

class Profile;

namespace web_app {
class WebAppRegistrar;
}

// PageHandler for the chrome://app-settings page. Connects directly to the
// WebAppProvider to manage settings for web apps.
class WebAppSettingsPageHandler : public AppManagementPageHandlerBase,
                                  public web_app::WebAppRegistrarObserver {
 public:
  WebAppSettingsPageHandler(
      mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
      mojo::PendingRemote<app_management::mojom::Page> page,
      Profile* profile,
      AppManagementPageHandlerBase::Delegate& delegate);

  WebAppSettingsPageHandler(const WebAppSettingsPageHandler&) = delete;
  WebAppSettingsPageHandler& operator=(const WebAppSettingsPageHandler&) =
      delete;

  ~WebAppSettingsPageHandler() override;

  // app_management::mojom::PageHandler:
  void GetExtensionAppPermissionMessages(
      const std::string& app_id,
      GetExtensionAppPermissionMessagesCallback callback) override;
  void GetSubAppToParentMap(GetSubAppToParentMapCallback callback) override;
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
#if BUILDFLAG(IS_MAC)
  void OpenSystemNotificationSettings(const std::string& app_id) override;
#endif

  // web_app::WebAppRegistrarObserver:
  void OnAppRegistrarDestroyed() override;
  void OnWebAppFileHandlerApprovalStateChanged(
      const webapps::AppId& app_id) override;
  void OnWebAppUserLinkCapturingPreferencesChanged(const webapps::AppId& app_id,
                                                   bool is_preferred) override;

  // AppManagementPageHandlerBase:
  app_management::mojom::AppPtr CreateApp(const std::string& app_id) override;

 private:
  const raw_ref<Delegate> delegate_;

  base::ScopedObservation<web_app::WebAppRegistrar,
                          web_app::WebAppRegistrarObserver>
      registrar_observation_{this};

#if BUILDFLAG(IS_MAC)
  base::CallbackListSubscription app_shim_observation_;
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_MANAGEMENT_WEB_APP_SETTINGS_PAGE_HANDLER_H_
