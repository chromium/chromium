// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps_base.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
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

}  // namespace

WebAppsBase::WebAppsBase(
    const mojo::Remote<apps::mojom::AppService>& app_service,
    Profile* profile)
    : profile_(profile),
      app_service_(nullptr),
      app_type_(GetWebAppType()),
      publisher_helper_(profile_, app_type_, this) {
  Initialize(app_service);
}

WebAppsBase::~WebAppsBase() = default;

void WebAppsBase::Shutdown() {
  if (provider_) {
    registrar_observation_.Reset();
    content_settings_observation_.Reset();
  }
}

const WebApp* WebAppsBase::GetWebApp(const AppId& app_id) const {
  // GetRegistrar() might return nullptr if the legacy bookmark apps registry is
  // enabled. This may happen in migration browser tests.
  return GetRegistrar() ? GetRegistrar()->GetAppById(app_id) : nullptr;
}

void WebAppsBase::OnWebAppInstalled(const AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (web_app && Accepts(app_id)) {
    Publish(Convert(web_app, apps::mojom::Readiness::kReady), subscribers_);
  }
}

void WebAppsBase::OnWebAppWillBeUninstalled(const AppId& app_id) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app || !Accepts(app_id)) {
    return;
  }

  Publish(publisher_helper().ConvertUninstalledWebApp(web_app), subscribers_);
}

void WebAppsBase::Initialize(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  DCHECK(profile_);
  if (!AreWebAppsEnabled(profile_)) {
    return;
  }

  provider_ = WebAppProvider::Get(profile_);
  DCHECK(provider_);

  registrar_observation_.Observe(&provider_->registrar());
  content_settings_observation_.Observe(
      HostContentSettingsMapFactory::GetForProfile(profile_));

  PublisherBase::Initialize(app_service, app_type_);
  app_service_ = app_service.get();
}

const WebAppRegistrar* WebAppsBase::GetRegistrar() const {
  DCHECK(provider_);
  return provider_->registrar().AsWebAppRegistrar();
}

void WebAppsBase::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  DCHECK(provider_);

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppsBase::StartPublishingWebApps,
                                AsWeakPtr(), std::move(subscriber_remote)));
}

void WebAppsBase::LoadIcon(const std::string& app_id,
                           apps::mojom::IconKeyPtr icon_key,
                           apps::mojom::IconType icon_type,
                           int32_t size_hint_in_dip,
                           bool allow_placeholder_icon,
                           LoadIconCallback callback) {
  publisher_helper().LoadIcon(app_id, std::move(icon_key), std::move(icon_type),
                              size_hint_in_dip, allow_placeholder_icon,
                              std::move(callback));
}

void WebAppsBase::Launch(const std::string& app_id,
                         int32_t event_flags,
                         apps::mojom::LaunchSource launch_source,
                         apps::mojom::WindowInfoPtr window_info) {
  publisher_helper().Launch(app_id, event_flags, std::move(launch_source),
                            std::move(window_info));
}

void WebAppsBase::LaunchAppWithFiles(const std::string& app_id,
                                     apps::mojom::LaunchContainer container,
                                     int32_t event_flags,
                                     apps::mojom::LaunchSource launch_source,
                                     apps::mojom::FilePathsPtr file_paths) {
  publisher_helper().LaunchAppWithFiles(app_id, std::move(container),
                                        event_flags, std::move(launch_source),
                                        std::move(file_paths));
}

void WebAppsBase::LaunchAppWithIntent(const std::string& app_id,
                                      int32_t event_flags,
                                      apps::mojom::IntentPtr intent,
                                      apps::mojom::LaunchSource launch_source,
                                      apps::mojom::WindowInfoPtr window_info) {
  publisher_helper().LaunchAppWithIntent(app_id, event_flags, std::move(intent),
                                         std::move(launch_source),
                                         std::move(window_info));
}

void WebAppsBase::SetPermission(const std::string& app_id,
                                apps::mojom::PermissionPtr permission) {
  if (!profile_) {
    return;
  }

  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  DCHECK(host_content_settings_map);

  const GURL url = web_app->start_url();

  ContentSettingsType permission_type =
      static_cast<ContentSettingsType>(permission->permission_id);
  if (!WebAppPublisherHelper::IsSupportedWebAppPermissionType(
          permission_type)) {
    return;
  }

  DCHECK_EQ(permission->value_type,
            apps::mojom::PermissionValueType::kTriState);
  ContentSetting permission_value = CONTENT_SETTING_DEFAULT;
  switch (static_cast<apps::mojom::TriState>(permission->value)) {
    case apps::mojom::TriState::kAllow:
      permission_value = CONTENT_SETTING_ALLOW;
      break;
    case apps::mojom::TriState::kAsk:
      permission_value = CONTENT_SETTING_ASK;
      break;
    case apps::mojom::TriState::kBlock:
      permission_value = CONTENT_SETTING_BLOCK;
      break;
    default:  // Return if value is invalid.
      return;
  }

  host_content_settings_map->SetContentSettingDefaultScope(
      url, url, permission_type, permission_value);
}

void WebAppsBase::OpenNativeSettings(const std::string& app_id) {
  if (!profile_) {
    return;
  }

  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  chrome::ShowSiteSettings(profile_, web_app->start_url());
}

void WebAppsBase::PublishWebApp(apps::mojom::AppPtr app) {
  Publish(std::move(app), subscribers_);
}

void WebAppsBase::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  // If content_type is not one of the supported permissions, do nothing.
  if (!WebAppPublisherHelper::IsSupportedWebAppPermissionType(content_type)) {
    return;
  }

  if (!profile_) {
    return;
  }

  const WebAppRegistrar* registrar = GetRegistrar();
  // Can be nullptr in tests.
  if (!registrar) {
    return;
  }

  for (const WebApp& web_app : registrar->GetApps()) {
    if (primary_pattern.Matches(web_app.start_url()) &&
        Accepts(web_app.app_id())) {
      apps::mojom::AppPtr app = apps::mojom::App::New();
      app->app_type = app_type_;
      app->app_id = web_app.app_id();
      publisher_helper().PopulateWebAppPermissions(&web_app, &app->permissions);

      Publish(std::move(app), subscribers_);
    }
  }
}

void WebAppsBase::OnWebAppLastLaunchTimeChanged(
    const std::string& app_id,
    const base::Time& last_launch_time) {
  const WebApp* web_app = GetWebApp(app_id);
  if (web_app && Accepts(app_id)) {
    Publish(publisher_helper().ConvertLaunchedWebApp(web_app), subscribers_);
  }
}

void WebAppsBase::OnWebAppManifestUpdated(const AppId& app_id,
                                          base::StringPiece old_name) {
  const WebApp* web_app = GetWebApp(app_id);
  if (web_app && Accepts(app_id)) {
    Publish(Convert(web_app, apps::mojom::Readiness::kReady), subscribers_);
  }
}

void WebAppsBase::OnAppRegistrarDestroyed() {
  registrar_observation_.Reset();
}

void WebAppsBase::OnWebAppLocallyInstalledStateChanged(
    const AppId& app_id,
    bool is_locally_installed) {
  const WebApp* web_app = GetWebApp(app_id);
  if (!web_app)
    return;
  auto app = apps::mojom::App::New();
  app->app_type = app_type_;
  app->app_id = app_id;
  app->icon_key = publisher_helper().MakeIconKey(web_app);
  Publish(std::move(app), subscribers_);
}

void WebAppsBase::ConvertWebApps(apps::mojom::Readiness readiness,
                                 std::vector<apps::mojom::AppPtr>* apps_out) {
  const WebAppRegistrar* registrar = GetRegistrar();
  // Can be nullptr in tests.
  if (!registrar)
    return;

  for (const WebApp& web_app : registrar->GetApps()) {
    if (Accepts(web_app.app_id())) {
      apps_out->push_back(Convert(&web_app, readiness));
    }
  }
}

void WebAppsBase::StartPublishingWebApps(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote) {
  std::vector<apps::mojom::AppPtr> apps;
  ConvertWebApps(apps::mojom::Readiness::kReady, &apps);

  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps), app_type_,
                     true /* should_notify_initialized */);

  subscribers_.Add(std::move(subscriber));
}

}  // namespace web_app
