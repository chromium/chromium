// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps.h"

#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/webapps/browser/installable/installable_metrics.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#endif

using apps::IconEffects;

namespace web_app {

namespace {

apps::mojom::AppType GetWebAppType() {
// After moving the ordinary Web Apps to Lacros chrome, the remaining web
// apps in ash Chrome will be only System Web Apps. Change the app type
// to kSystemWeb for this case and the kWeb app type will be published from
// the publisher for Lacros web apps.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (crosapi::browser_util::IsLacrosEnabled() &&
      base::FeatureList::IsEnabled(features::kWebAppsCrosapi)) {
    return apps::mojom::AppType::kSystemWeb;
  }
#endif

  return apps::mojom::AppType::kWeb;
}

bool ShouldObserveMediaRequests() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The publisher helper owned by WebAppsPublisherHost observes media requests,
  // not the publisher helper owned by WebApps.
  return false;
#else
  return true;
#endif
}

}  // namespace

WebApps::WebApps(const mojo::Remote<apps::mojom::AppService>& app_service,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                 apps::InstanceRegistry* instance_registry,
#endif
                 Profile* profile)
    : profile_(profile),
      provider_(WebAppProvider::GetForLocalAppsUnchecked(profile_)),
      app_service_(nullptr),
      app_type_(GetWebAppType()),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      instance_registry_(instance_registry),
#endif
      publisher_helper_(profile_,
                        provider_,
                        app_type_,
                        this,
                        ShouldObserveMediaRequests()) {
  Initialize(app_service);
}

WebApps::~WebApps() = default;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// static
void WebApps::UninstallImpl(WebAppProvider* provider,
                            const std::string& app_id,
                            apps::mojom::UninstallSource uninstall_source,
                            gfx::NativeWindow parent_window) {
  WebAppUiManagerImpl* web_app_ui_manager = WebAppUiManagerImpl::Get(provider);
  if (!web_app_ui_manager) {
    return;
  }

  WebAppDialogManager& web_app_dialog_manager =
      web_app_ui_manager->dialog_manager();
  if (web_app_dialog_manager.CanUserUninstallWebApp(app_id)) {
    webapps::WebappUninstallSource webapp_uninstall_source =
        WebAppPublisherHelper::ConvertUninstallSourceToWebAppUninstallSource(
            uninstall_source);
    web_app_dialog_manager.UninstallWebApp(app_id, webapp_uninstall_source,
                                           parent_window, base::DoNothing());
  }
}
#endif

void WebApps::Shutdown() {
  if (provider_) {
    publisher_helper().Shutdown();
  }
}

const WebApp* WebApps::GetWebApp(const AppId& app_id) const {
  DCHECK(provider_);
  return provider_->registrar().GetAppById(app_id);
}

bool WebApps::Accepts(const std::string& app_id) const {
  return WebAppPublisherHelper::Accepts(app_id);
}

void WebApps::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  DCHECK(profile_);
  if (!AreWebAppsEnabled(profile_)) {
    return;
  }

  DCHECK(provider_);

  PublisherBase::Initialize(app_service, app_type_);
  app_service_ = app_service.get();
}

void WebApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  DCHECK(provider_);

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebApps::StartPublishingWebApps, AsWeakPtr(),
                                std::move(subscriber_remote)));
}

void WebApps::LoadIcon(const std::string& app_id,
                       apps::mojom::IconKeyPtr icon_key,
                       apps::mojom::IconType icon_type,
                       int32_t size_hint_in_dip,
                       bool allow_placeholder_icon,
                       LoadIconCallback callback) {
  publisher_helper().LoadIcon(app_id, std::move(icon_key), std::move(icon_type),
                              size_hint_in_dip, allow_placeholder_icon,
                              std::move(callback));
}

void WebApps::Launch(const std::string& app_id,
                     int32_t event_flags,
                     apps::mojom::LaunchSource launch_source,
                     apps::mojom::WindowInfoPtr window_info) {
  publisher_helper().Launch(app_id, event_flags, launch_source,
                            std::move(window_info));
}

void WebApps::LaunchAppWithFiles(const std::string& app_id,
                                 int32_t event_flags,
                                 apps::mojom::LaunchSource launch_source,
                                 apps::mojom::FilePathsPtr file_paths) {
  publisher_helper().LaunchAppWithFiles(app_id, event_flags, launch_source,
                                        std::move(file_paths));
}

void WebApps::LaunchAppWithIntent(const std::string& app_id,
                                  int32_t event_flags,
                                  apps::mojom::IntentPtr intent,
                                  apps::mojom::LaunchSource launch_source,
                                  apps::mojom::WindowInfoPtr window_info) {
  publisher_helper().LaunchAppWithIntent(app_id, event_flags, std::move(intent),
                                         launch_source, std::move(window_info));
}

void WebApps::SetPermission(const std::string& app_id,
                            apps::mojom::PermissionPtr permission) {
  publisher_helper().SetPermission(app_id, std::move(permission));
}

void WebApps::OpenNativeSettings(const std::string& app_id) {
  publisher_helper().OpenNativeSettings(app_id);
}

void WebApps::PublishWebApps(std::vector<apps::mojom::AppPtr> apps) {
  const bool should_notify_initialized = false;
  if (subscribers_.size() == 1) {
    auto& subscriber = *subscribers_.begin();
    subscriber->OnApps(std::move(apps), app_type(), should_notify_initialized);
    return;
  }
  for (auto& subscriber : subscribers_) {
    std::vector<apps::mojom::AppPtr> cloned_apps;
    for (const auto& app : apps)
      cloned_apps.push_back(app.Clone());
    subscriber->OnApps(std::move(cloned_apps), app_type(),
                       should_notify_initialized);
  }
}

void WebApps::PublishWebApp(apps::mojom::AppPtr app) {
  Publish(std::move(app), subscribers_);
}

void WebApps::ModifyWebAppCapabilityAccess(
    const std::string& app_id,
    absl::optional<bool> accessing_camera,
    absl::optional<bool> accessing_microphone) {
  ModifyCapabilityAccess(subscribers_, app_id, std::move(accessing_camera),
                         std::move(accessing_microphone));
}

void WebApps::ConvertWebApps(std::vector<apps::mojom::AppPtr>* apps_out) {
  DCHECK(provider_);

  for (const WebApp& web_app : provider_->registrar().GetApps()) {
    if (Accepts(web_app.app_id())) {
      apps_out->push_back(publisher_helper().ConvertWebApp(&web_app));
    }
  }
}

void WebApps::StartPublishingWebApps(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote) {
  std::vector<apps::mojom::AppPtr> apps;
  ConvertWebApps(&apps);

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps), app_type_,
                     true /* should_notify_initialized */);

  subscribers_.Add(std::move(subscriber));
}

}  // namespace web_app
