// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_LACROS_WEB_APPS_CONTROLLER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_LACROS_WEB_APPS_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/webapps/common/web_app_id.h"
#include "mojo/public/cpp/bindings/receiver.h"

static_assert(BUILDFLAG(IS_CHROMEOS_LACROS), "For Lacros only");

class Profile;

namespace web_app {

class WebApp;
class WebAppProvider;
class WebAppRegistrar;

// This LacrosWebAppsController observes web app updates on Lacros, and calls
// WebAppsCrosapi to inform the Ash browser of the current set of web apps.
class LacrosWebAppsController : public crosapi::mojom::AppController,
                                public WebAppPublisherHelper::Delegate {
 public:
  using LoadIconCallback = WebAppPublisherHelper::LoadIconCallback;

  explicit LacrosWebAppsController(Profile* profile);
  LacrosWebAppsController(const LacrosWebAppsController&) = delete;
  LacrosWebAppsController& operator=(const LacrosWebAppsController&) = delete;
  ~LacrosWebAppsController() override;

  void Init();

  void Shutdown();

  Profile* profile() { return profile_; }
  WebAppRegistrar& registrar() const;

  WebAppPublisherHelper& publisher_helper() { return publisher_helper_; }

  void SetPublisherForTesting(crosapi::mojom::AppPublisher* publisher);

  // crosapi::mojom::AppController:
  void Uninstall(const std::string& app_id,
                 apps::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApp(const std::string& app_id) override;
  void StopApp(const std::string& app_id) override;
  void GetMenuModel(const std::string& app_id,
                    GetMenuModelCallback callback) override;
  void DEPRECATED_LoadIcon(const std::string& app_id,
                           apps::IconKeyPtr icon_key,
                           apps::IconType icon_type,
                           int32_t size_hint_in_dip,
                           apps::LoadIconCallback callback) override;
  void GetCompressedIcon(const std::string& app_id,
                         int32_t size_in_dip,
                         ui::ResourceScaleFactor scale_factor,
                         apps::LoadIconCallback callback) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void UpdateAppSize(const std::string& app_id) override;
  void SetWindowMode(const std::string& app_id,
                     apps::WindowMode window_mode) override;
  void Launch(crosapi::mojom::LaunchParamsPtr launch_params,
              LaunchCallback callback) override;
  void ExecuteContextMenuCommand(
      const std::string& app_id,
      const std::string& id,
      ExecuteContextMenuCommandCallback callback) override;
  void SetPermission(const std::string& app_id,
                     apps::PermissionPtr permission) override;

 private:
  void OnReady();
  void ExecuteContextMenuCommandInternal(
      const std::string& app_id,
      const std::string& id,
      base::OnceCallback<void(std::vector<content::WebContents*>)>
          launch_finished_callback);
  void LaunchInternal(
      const std::string& app_id,
      apps::AppLaunchParams params,
      base::OnceCallback<void(std::vector<content::WebContents*>)>
          launch_finished_callback);

  // WebAppPublisherHelper::Delegate:
  void PublishWebApps(std::vector<apps::AppPtr> apps) override;
  void PublishWebApp(apps::AppPtr app) override;
  void ModifyWebAppCapabilityAccess(
      const std::string& app_id,
      std::optional<bool> accessing_camera,
      std::optional<bool> accessing_microphone) override;

  void ReturnLaunchResults(
      base::OnceCallback<void(crosapi::mojom::LaunchResultPtr)> callback,
      std::vector<content::WebContents*> web_contents);

  const WebApp* GetWebApp(const webapps::AppId& app_id) const;

  void OnShortcutsMenuIconsRead(
      const std::string& app_id,
      crosapi::mojom::MenuItemsPtr menu_items,
      GetMenuModelCallback callback,
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);

  const raw_ptr<Profile> profile_;
  const raw_ptr<WebAppProvider> provider_;
  WebAppPublisherHelper publisher_helper_;
  raw_ptr<crosapi::mojom::AppPublisher> remote_publisher_ = nullptr;
  int remote_publisher_version_ = 0;

  mojo::Receiver<crosapi::mojom::AppController> receiver_{this};

  base::WeakPtrFactory<LacrosWebAppsController> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_LACROS_WEB_APPS_CONTROLLER_H_
