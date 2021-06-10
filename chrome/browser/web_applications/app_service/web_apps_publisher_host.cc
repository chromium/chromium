// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps_publisher_host.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/one_shot_event.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using apps::IconEffects;

namespace web_app {

WebAppsPublisherHost::WebAppsPublisherHost(Profile* profile)
    : profile_(profile),
      provider_(WebAppProvider::Get(profile)),
      publisher_helper_(profile, apps::mojom::AppType::kWeb, this) {}

WebAppsPublisherHost::~WebAppsPublisherHost() = default;

void WebAppsPublisherHost::Init() {
  // Allow for web app migration tests.
  if (!provider_->registrar().AsWebAppRegistrar()) {
    return;
  }

  if (!remote_publisher_) {
    auto* service = chromeos::LacrosService::Get();
    if (!service) {
      return;
    }
    if (!service->IsAvailable<crosapi::mojom::AppPublisher>()) {
      return;
    }
    if (!service->init_params()->web_apps_enabled) {
      return;
    }

    service->GetRemote<crosapi::mojom::AppPublisher>()->RegisterAppController(
        receiver_.BindNewPipeAndPassRemoteWithVersion());
    remote_publisher_ =
        service->GetRemote<crosapi::mojom::AppPublisher>().get();
  }

  media_dispatcher_.Observe(MediaCaptureDevicesDispatcher::GetInstance());

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppsPublisherHost::OnReady,
                                weak_ptr_factory_.GetWeakPtr()));
  registrar_observation_.Observe(&registrar());
}

void WebAppsPublisherHost::Shutdown() {
  registrar_observation_.Reset();
  publisher_helper().Shutdown();
}

WebAppRegistrar& WebAppsPublisherHost::registrar() const {
  return *provider_->registrar().AsWebAppRegistrar();
}

void WebAppsPublisherHost::SetPublisherForTesting(
    crosapi::mojom::AppPublisher* publisher) {
  remote_publisher_ = publisher;
}

void WebAppsPublisherHost::OnReady() {
  if (!remote_publisher_ || !registrar_observation_.IsObserving()) {
    return;
  }

  std::vector<apps::mojom::AppPtr> apps;
  for (const WebApp& web_app : registrar().GetApps()) {
    apps.push_back(Convert(&web_app, apps::mojom::Readiness::kReady));
  }
  remote_publisher_->OnApps(std::move(apps));
}

void WebAppsPublisherHost::Uninstall(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source,
    bool clear_site_data,
    bool report_abuse) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  publisher_helper().UninstallWebApp(web_app, uninstall_source, clear_site_data,
                                     report_abuse);
}

void WebAppsPublisherHost::PauseApp(const std::string& app_id) {
  publisher_helper().PauseApp(app_id);
}

void WebAppsPublisherHost::UnpauseApp(const std::string& app_id) {
  publisher_helper().UnpauseApp(app_id);
}

void WebAppsPublisherHost::LoadIcon(const std::string& app_id,
                                    apps::mojom::IconKeyPtr icon_key,
                                    apps::mojom::IconType icon_type,
                                    int32_t size_hint_in_dip,
                                    bool allow_placeholder_icon,
                                    LoadIconCallback callback) {
  publisher_helper().LoadIcon(app_id, std::move(icon_key), std::move(icon_type),
                              size_hint_in_dip, allow_placeholder_icon,
                              std::move(callback));
}

content::WebContents* WebAppsPublisherHost::Launch(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  return publisher_helper().Launch(
      app_id, event_flags, std::move(launch_source), std::move(window_info));
}

content::WebContents* WebAppsPublisherHost::LaunchAppWithFiles(
    const std::string& app_id,
    apps::mojom::LaunchContainer container,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::FilePathsPtr file_paths) {
  return publisher_helper().LaunchAppWithFiles(
      app_id, std::move(container), event_flags, std::move(launch_source),
      std::move(file_paths));
}

content::WebContents* WebAppsPublisherHost::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  return publisher_helper().LaunchAppWithIntent(
      app_id, event_flags, std::move(intent), std::move(launch_source),
      std::move(window_info));
}

void WebAppsPublisherHost::SetPermission(
    const std::string& app_id,
    apps::mojom::PermissionPtr permission) {
  publisher_helper().SetPermission(app_id, std::move(permission));
}

void WebAppsPublisherHost::OpenNativeSettings(const std::string& app_id) {
  publisher_helper().OpenNativeSettings(app_id);
}

void WebAppsPublisherHost::SetWindowMode(const std::string& app_id,
                                         apps::mojom::WindowMode window_mode) {
  return publisher_helper().SetWindowMode(app_id, window_mode);
}

void WebAppsPublisherHost::OnWebAppInstalled(const AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  PublishWebApp(Convert(web_app, apps::mojom::Readiness::kReady));
}

void WebAppsPublisherHost::OnWebAppManifestUpdated(const AppId& app_id,
                                                   base::StringPiece old_name) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  PublishWebApp(Convert(web_app, apps::mojom::Readiness::kReady));
}

void WebAppsPublisherHost::OnWebAppWillBeUninstalled(const AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  // TODO(crbug.com/1194709): Keep consistent behavior with WebAppsChromeOs:
  // remove notifications for app, update paused apps.

  auto result = media_requests_.RemoveRequests(app_id);
  ModifyCapabilityAccess(app_id, result.camera, result.microphone);

  PublishWebApp(publisher_helper().ConvertUninstalledWebApp(web_app));
}

void WebAppsPublisherHost::OnAppRegistrarDestroyed() {
  registrar_observation_.Reset();
}

void WebAppsPublisherHost::OnRequestUpdate(
    int render_process_id,
    int render_frame_id,
    blink::mojom::MediaStreamType stream_type,
    const content::MediaRequestState state) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_process_id, render_frame_id));

  if (!web_contents) {
    return;
  }

  absl::optional<AppId> app_id =
      FindInstalledAppWithUrlInScope(profile(), web_contents->GetURL(),
                                     /*window_only=*/false);
  if (!app_id.has_value()) {
    return;
  }

  const WebApp* web_app = GetWebApp(app_id.value());
  if (!web_app) {
    return;
  }

  if (media_requests_.IsNewRequest(app_id.value(), web_contents, state)) {
    content::WebContentsUserData<
        apps::AppWebContentsData>::CreateForWebContents(web_contents, this);
  }

  auto result = media_requests_.UpdateRequests(app_id.value(), web_contents,
                                               stream_type, state);
  ModifyCapabilityAccess(app_id.value(), result.camera, result.microphone);
}

void WebAppsPublisherHost::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  absl::optional<AppId> app_id = FindInstalledAppWithUrlInScope(
      profile(), web_contents->GetLastCommittedURL(),
      /*window_only=*/false);
  if (!app_id.has_value()) {
    return;
  }

  const WebApp* web_app = GetWebApp(app_id.value());
  if (!web_app) {
    return;
  }

  auto result =
      media_requests_.OnWebContentsDestroyed(app_id.value(), web_contents);
  ModifyCapabilityAccess(app_id.value(), result.camera, result.microphone);
}

const WebApp* WebAppsPublisherHost::GetWebApp(const AppId& app_id) const {
  return registrar().GetAppById(app_id);
}

apps::mojom::AppPtr WebAppsPublisherHost::Convert(
    const WebApp* web_app,
    apps::mojom::Readiness readiness) {
  DCHECK(web_app->chromeos_data().has_value());
  apps::mojom::AppPtr app =
      publisher_helper().ConvertWebApp(web_app, readiness);
  app->icon_key = publisher_helper().MakeIconKey(web_app);
  return app;
}

void WebAppsPublisherHost::PublishWebApps(
    std::vector<apps::mojom::AppPtr> apps) {
  if (!remote_publisher_) {
    return;
  }

  remote_publisher_->OnApps(std::move(apps));
}

void WebAppsPublisherHost::PublishWebApp(apps::mojom::AppPtr app) {
  if (!remote_publisher_) {
    return;
  }

  std::vector<apps::mojom::AppPtr> apps;
  apps.push_back(std::move(app));
  remote_publisher_->OnApps(std::move(apps));
}

void WebAppsPublisherHost::ModifyCapabilityAccess(
    const std::string& app_id,
    absl::optional<bool> accessing_camera,
    absl::optional<bool> accessing_microphone) {
  if (!remote_publisher_) {
    return;
  }

  if (!accessing_camera.has_value() && !accessing_microphone.has_value()) {
    return;
  }

  std::vector<apps::mojom::CapabilityAccessPtr> capability_accesses;
  auto capability_access = apps::mojom::CapabilityAccess::New();
  capability_access->app_id = app_id;

  if (accessing_camera.has_value()) {
    capability_access->camera = accessing_camera.value()
                                    ? apps::mojom::OptionalBool::kTrue
                                    : apps::mojom::OptionalBool::kFalse;
  }

  if (accessing_microphone.has_value()) {
    capability_access->microphone = accessing_microphone.value()
                                        ? apps::mojom::OptionalBool::kTrue
                                        : apps::mojom::OptionalBool::kFalse;
  }

  capability_accesses.push_back(std::move(capability_access));
  remote_publisher_->OnCapabilityAccesses(std::move(capability_accesses));
}

}  // namespace web_app
