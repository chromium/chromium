// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_PUBLISHER_HOST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_PUBLISHER_HOST_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

class WebApp;
class WebAppProvider;
class WebAppRegistrar;

// This WebAppsPublisherHost observes web app updates on Lacros, and calls
// WebAppsCrosapi to inform the Ash browser of the current set of web apps.
class WebAppsPublisherHost : public crosapi::mojom::AppController,
                             public WebAppPublisherHelper::Delegate {
 public:
  using LoadIconCallback = WebAppPublisherHelper::LoadIconCallback;

  explicit WebAppsPublisherHost(Profile* profile);
  WebAppsPublisherHost(const WebAppsPublisherHost&) = delete;
  WebAppsPublisherHost& operator=(const WebAppsPublisherHost&) = delete;
  ~WebAppsPublisherHost() override;

  void Init();

  void Shutdown();

  Profile* profile() { return profile_; }
  WebAppRegistrar& registrar() const;

  WebAppPublisherHelper& publisher_helper() { return publisher_helper_; }

  void SetPublisherForTesting(crosapi::mojom::AppPublisher* publisher);

  // TODO(crbug.com/1194709): Add these to crosapi::mojom::AppController:
  content::WebContents* Launch(const std::string& app_id,
                               int32_t event_flags,
                               apps::mojom::LaunchSource launch_source,
                               apps::mojom::WindowInfoPtr window_info);
  content::WebContents* LaunchAppWithFiles(
      const std::string& app_id,
      apps::mojom::LaunchContainer container,
      int32_t event_flags,
      apps::mojom::LaunchSource launch_source,
      apps::mojom::FilePathsPtr file_paths);
  content::WebContents* LaunchAppWithIntent(
      const std::string& app_id,
      int32_t event_flags,
      apps::mojom::IntentPtr intent,
      apps::mojom::LaunchSource launch_source,
      apps::mojom::WindowInfoPtr window_info);

  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission);

  void SetWindowMode(const std::string& app_id,
                     apps::mojom::WindowMode window_mode);

  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int32_t item_id,
                                 int64_t display_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(WebAppsPublisherHostBrowserTest, PauseUnpause);
  FRIEND_TEST_ALL_PREFIXES(WebAppsPublisherHostBrowserTest, OpenNativeSettings);

  void OnReady();

  // crosapi::mojom::AppController:
  void Uninstall(const std::string& app_id,
                 apps::mojom::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApp(const std::string& app_id) override;
  void GetMenuModel(const std::string& app_id,
                    GetMenuModelCallback callback) override;
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconType icon_type,
                int32_t size_hint_in_dip,
                LoadIconCallback callback) override;
  void OpenNativeSettings(const std::string& app_id) override;

  // WebAppPublisherHelper::Delegate:
  void PublishWebApps(std::vector<apps::mojom::AppPtr> apps) override;
  void PublishWebApp(apps::mojom::AppPtr app) override;
  void ModifyWebAppCapabilityAccess(
      const std::string& app_id,
      absl::optional<bool> accessing_camera,
      absl::optional<bool> accessing_microphone) override;

  const WebApp* GetWebApp(const AppId& app_id) const;

  void OnShortcutsMenuIconsRead(
      const std::string& app_id,
      crosapi::mojom::MenuItemsPtr menu_items,
      GetMenuModelCallback callback,
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);

  Profile* const profile_;
  WebAppProvider* const provider_;
  WebAppPublisherHelper publisher_helper_;
  crosapi::mojom::AppPublisher* remote_publisher_ = nullptr;

  mojo::Receiver<crosapi::mojom::AppController> receiver_{this};

  base::WeakPtrFactory<WebAppsPublisherHost> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_PUBLISHER_HOST_H_
