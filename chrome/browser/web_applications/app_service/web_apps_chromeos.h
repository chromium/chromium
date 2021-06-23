// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_CHROMEOS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_CHROMEOS_H_

#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/web_applications/app_service/web_apps_base.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace web_app {

class WebApp;

// An app publisher (in the App Service sense) of Web Apps.
class WebAppsChromeOs : public WebAppsBase, public ArcAppListPrefs::Observer {
 public:
  WebAppsChromeOs(const mojo::Remote<apps::mojom::AppService>& app_service,
                  Profile* profile,
                  apps::InstanceRegistry* instance_registry);
  WebAppsChromeOs(const WebAppsChromeOs&) = delete;
  WebAppsChromeOs& operator=(const WebAppsChromeOs&) = delete;
  ~WebAppsChromeOs() override;

  void Shutdown() override;

  void ObserveArc();

 private:
  void Initialize();

  // apps::mojom::Publisher overrides.
  void Uninstall(const std::string& app_id,
                 apps::mojom::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApp(const std::string& app_id) override;
  void GetMenuModel(const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    GetMenuModelCallback callback) override;
  void GetMenuModelFromWebAppProvider(const std::string& app_id,
                                      apps::mojom::MenuType menu_type,
                                      apps::mojom::MenuItemsPtr menu_items,
                                      GetMenuModelCallback callback);
  // menu_type is stored as |shortcut_id|.
  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id) override;
  void SetWindowMode(const std::string& app_id,
                     apps::mojom::WindowMode window_mode) override;

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnWebAppWillBeUninstalled(const AppId& app_id) override;

  // ArcAppListPrefs::Observer overrides.
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;
  void OnPackageListInitialRefreshed() override;
  void OnArcAppListPrefsDestroyed() override;

  void OnShortcutsMenuIconsRead(
      const std::string& app_id,
      apps::mojom::MenuType menu_type,
      apps::mojom::MenuItemsPtr menu_items,
      GetMenuModelCallback callback,
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);

  apps::mojom::AppPtr Convert(const WebApp* web_app,
                              apps::mojom::Readiness readiness) override;
  void ConvertWebApps(apps::mojom::Readiness readiness,
                      std::vector<apps::mojom::AppPtr>* apps_out);
  void StartPublishingWebApps(
      mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote);

  // Get the equivalent Chrome app from |arc_package_name| and set the Chrome
  // app badge on the icon effects for the equivalent Chrome apps. If the
  // equivalent ARC app is installed, add the Chrome app badge, otherwise,
  // remove the Chrome app badge.
  void ApplyChromeBadge(const std::string& arc_package_name);

  apps::InstanceRegistry* instance_registry_;

  ArcAppListPrefs* arc_prefs_ = nullptr;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_CHROMEOS_H_
