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
#include "chrome/browser/apps/app_service/app_web_contents_data.h"
#include "chrome/browser/apps/app_service/media_requests.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
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

// This WebAppsPublisherHost observes AppRegistrar on Lacros, and calls
// WebAppsCrosapi to inform the Ash browser of the current set of web apps.
class WebAppsPublisherHost : public crosapi::mojom::AppController,
                             public WebAppPublisherHelper::Delegate,
                             public AppRegistrarObserver,
                             public MediaCaptureDevicesDispatcher::Observer,
                             public apps::AppWebContentsData::Client {
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
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback);
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
  void OpenNativeSettings(const std::string& app_id);

  void SetWindowMode(const std::string& app_id,
                     apps::mojom::WindowMode window_mode);

 private:
  FRIEND_TEST_ALL_PREFIXES(WebAppsPublisherHostBrowserTest, PauseUnpause);

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

  // WebAppPublisherHelper::Delegate:
  void PublishWebApps(std::vector<apps::mojom::AppPtr> apps) override;
  void PublishWebApp(apps::mojom::AppPtr app) override;

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnWebAppManifestUpdated(const AppId& app_id,
                               base::StringPiece old_name) override;
  void OnWebAppWillBeUninstalled(const AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;
  // TODO(crbug.com/1194709): Add more overrides, guided by WebAppsChromeOs.

  // MediaCaptureDevicesDispatcher::Observer:
  void OnRequestUpdate(int render_process_id,
                       int render_frame_id,
                       blink::mojom::MediaStreamType stream_type,
                       const content::MediaRequestState state) override;

  // apps::AppWebContentsData::Client:
  void OnWebContentsDestroyed(content::WebContents* contents) override;

  const WebApp* GetWebApp(const AppId& app_id) const;
  apps::mojom::AppPtr Convert(const WebApp* web_app,
                              apps::mojom::Readiness readiness);

  void ModifyCapabilityAccess(const std::string& app_id,
                              absl::optional<bool> accessing_camera,
                              absl::optional<bool> accessing_microphone);

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

  base::ScopedObservation<AppRegistrar, AppRegistrarObserver>
      registrar_observation_{this};

  base::ScopedObservation<MediaCaptureDevicesDispatcher,
                          MediaCaptureDevicesDispatcher::Observer>
      media_dispatcher_{this};

  apps::MediaRequests media_requests_;

  base::WeakPtrFactory<WebAppsPublisherHost> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_PUBLISHER_HOST_H_
