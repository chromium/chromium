// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_CHROMEOS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_CHROMEOS_H_

#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/app_service/web_apps_base.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace web_app {

// An app publisher (in the App Service sense) of Web Apps.
class WebAppsChromeOs : public WebAppsBase {
 public:
  WebAppsChromeOs(const mojo::Remote<apps::mojom::AppService>& app_service,
                  Profile* profile,
                  apps::InstanceRegistry* instance_registry);
  WebAppsChromeOs(const WebAppsChromeOs&) = delete;
  WebAppsChromeOs& operator=(const WebAppsChromeOs&) = delete;
  ~WebAppsChromeOs() override;

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

  void OnShortcutsMenuIconsRead(
      const std::string& app_id,
      apps::mojom::MenuType menu_type,
      apps::mojom::MenuItemsPtr menu_items,
      GetMenuModelCallback callback,
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);

  void StartPublishingWebApps(
      mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote);

  apps::InstanceRegistry* instance_registry_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_CHROMEOS_H_
