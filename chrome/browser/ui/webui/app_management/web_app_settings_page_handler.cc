// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/web_app_settings_page_handler.h"

#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler_base.h"
#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/url_formatter/elide_url.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/default_apps_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#endif

namespace {

std::vector<std::string> GetSupportedLinks(const std::string& app_id,
                                           web_app::WebAppProvider& provider) {
  GURL app_scope = provider.registrar_unsafe().GetAppScope(app_id);
  if (!web_app::IsValidScopeForLinkCapturing(app_scope)) {
    return std::vector<std::string>();
  }

  std::string scope_str(app_scope.host());
  if (app_scope.has_port()) {
    scope_str += ":" + app_scope.port();
  }
  scope_str += app_scope.path();
  if (scope_str.back() == '/') {
    scope_str = scope_str + "*";
  } else {
    scope_str = scope_str + "/*";
  }
  return {scope_str};
}

std::string GetFormattedOrigin(const webapps::AppId& app_id,
                               web_app::WebAppProvider& provider) {
  GURL origin_url = provider.registrar_unsafe().GetAppStartUrl(app_id);
  // Format origin URL to not show the scheme and default port numbers.
  std::u16string origin_url_formatted =
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(origin_url),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  return base::UTF16ToUTF8(origin_url_formatted);
}

// Returns a list of origin URLs from scope_extensions of an app's Manifest.
std::vector<std::string> GetScopeExtensions(const webapps::AppId& app_id,
                                            web_app::WebAppProvider& provider) {
  std::vector<std::string> scope_extensions_vector;

  for (const auto& scope_extension :
       provider.registrar_unsafe().GetScopeExtensions(app_id)) {
    url::Origin origin = scope_extension.origin;
    // Format origin URL to not show the scheme and default port numbers.
    std::u16string origin_formatted =
        url_formatter::FormatOriginForSecurityDisplay(
            origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    if (scope_extension.has_origin_wildcard) {
      origin_formatted = u"*." + origin_formatted;
    }
    scope_extensions_vector.push_back(base::UTF16ToUTF8(origin_formatted));
  }
  return scope_extensions_vector;
}

web_app::RunOnOsLoginMode ConvertOsLoginModeToWebAppConstants(
    apps::RunOnOsLoginMode login_mode) {
  web_app::RunOnOsLoginMode web_app_constant_login_mode =
      web_app::RunOnOsLoginMode::kMinValue;
  switch (login_mode) {
    case apps::RunOnOsLoginMode::kWindowed:
      web_app_constant_login_mode = web_app::RunOnOsLoginMode::kWindowed;
      break;
    case apps::RunOnOsLoginMode::kNotRun:
      web_app_constant_login_mode = web_app::RunOnOsLoginMode::kNotRun;
      break;
    case apps::RunOnOsLoginMode::kUnknown:
      web_app_constant_login_mode = web_app::RunOnOsLoginMode::kNotRun;
      break;
  }
  return web_app_constant_login_mode;
}

}  // namespace

WebAppSettingsPageHandler::WebAppSettingsPageHandler(
    mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
    mojo::PendingRemote<app_management::mojom::Page> page,
    Profile* profile,
    AppManagementPageHandlerBase::Delegate& delegate)
    : AppManagementPageHandlerBase(std::move(receiver),
                                   std::move(page),
                                   profile),
      delegate_(delegate) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  registrar_observation_.Observe(&provider->registrar_unsafe());
#if BUILDFLAG(IS_MAC)
  app_shim_observation_ = AppShimRegistry::Get()->RegisterAppChangedCallback(
      base::BindRepeating(&WebAppSettingsPageHandler::NotifyAppChanged,
                          base::Unretained(this)));
#endif
}

WebAppSettingsPageHandler::~WebAppSettingsPageHandler() = default;

void WebAppSettingsPageHandler::GetSubAppToParentMap(
    GetSubAppToParentMapCallback callback) {
  NOTIMPLEMENTED();
}

void WebAppSettingsPageHandler::GetExtensionAppPermissionMessages(
    const std::string& app_id,
    GetExtensionAppPermissionMessagesCallback callback) {
  NOTIMPLEMENTED();
}

void WebAppSettingsPageHandler::SetPinned(const std::string& app_id,
                                          bool pinned) {
  NOTIMPLEMENTED();
}

void WebAppSettingsPageHandler::SetResizeLocked(const std::string& app_id,
                                                bool locked) {
  NOTIMPLEMENTED();
}

void WebAppSettingsPageHandler::Uninstall(const std::string& app_id) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile());

  if (!provider->registrar_unsafe().CanUserUninstallWebApp(app_id)) {
    return;
  }

  provider->ui_manager().PresentUserUninstallDialog(
      app_id, webapps::WebappUninstallSource::kAppManagement,
      delegate_->GetUninstallAnchorWindow(), base::DoNothing());
}

void WebAppSettingsPageHandler::SetPreferredApp(const std::string& app_id,
                                                bool is_preferred_app) {
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile());

  provider->scheduler().SetAppCapturesSupportedLinksDisableOverlapping(
      app_id, is_preferred_app, base::DoNothing());
}

void WebAppSettingsPageHandler::GetOverlappingPreferredApps(
    const std::string& app_id,
    GetOverlappingPreferredAppsCallback callback) {
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile());
  provider->scheduler().ScheduleCallbackWithResult(
      "AppManagementPageHandlerBase::GetOverlappingPreferredApps",
      web_app::AllAppsLockDescription(),
      base::BindOnce(
          [](const webapps::AppId& app_id, web_app::AllAppsLock& all_apps_lock,
             base::Value::Dict& debug_value) {
            return all_apps_lock.registrar().GetOverlappingAppsMatchingScope(
                app_id);
          },
          app_id),
      std::move(callback), /*arg_for_shutdown=*/std::vector<std::string>());
}

void WebAppSettingsPageHandler::UpdateAppSize(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void WebAppSettingsPageHandler::SetWindowMode(const std::string& app_id,
                                              apps::WindowMode window_mode) {
  auto* provider = web_app::WebAppProvider::GetForLocalAppsUnchecked(profile());

  // Changing window mode is not allowed for isolated web apps.
  if (provider->registrar_unsafe().IsIsolated(app_id)) {
    return;
  }

  apps::AppServiceProxyFactory::GetForProfile(profile())->SetWindowMode(
      app_id, window_mode);
}

void WebAppSettingsPageHandler::SetRunOnOsLoginMode(
    const std::string& app_id,
    apps::RunOnOsLoginMode run_on_os_login_mode) {
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile());
  provider->scheduler().SetRunOnOsLoginMode(
      app_id, ConvertOsLoginModeToWebAppConstants(run_on_os_login_mode),
      base::DoNothing());
}

void WebAppSettingsPageHandler::ShowDefaultAppAssociationsUi() {
#if BUILDFLAG(IS_WIN)
  base::win::LaunchDefaultAppsSettingsModernDialog({});
#else
  NOTIMPLEMENTED();
#endif
}

void WebAppSettingsPageHandler::OpenStorePage(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void WebAppSettingsPageHandler::SetAppLocale(const std::string& app_id,
                                             const std::string& locale_tag) {
  NOTIMPLEMENTED();
}

#if BUILDFLAG(IS_MAC)
void WebAppSettingsPageHandler::OpenSystemNotificationSettings(
    const std::string& app_id) {
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kNotifications,
      web_app::GetBundleIdentifierForShim(app_id));
}
#endif

void WebAppSettingsPageHandler::OnAppRegistrarDestroyed() {
  registrar_observation_.Reset();
}

void WebAppSettingsPageHandler::OnWebAppFileHandlerApprovalStateChanged(
    const webapps::AppId& app_id) {
  NotifyAppChanged(app_id);
}

void WebAppSettingsPageHandler::OnWebAppUserLinkCapturingPreferencesChanged(
    const webapps::AppId& app_id,
    bool is_preferred) {
  NotifyAppChanged(app_id);
}

app_management::mojom::AppPtr WebAppSettingsPageHandler::CreateApp(
    const std::string& app_id) {
  app_management::mojom::AppPtr app =
      AppManagementPageHandlerBase::CreateApp(app_id);
  if (!app) {
    return nullptr;
  }

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile());
  CHECK(provider);
  app->is_preferred_app =
      provider->registrar_unsafe().CapturesLinksInScope(app_id);

  // This allows us to bypass showing the supported links item if the feature is
  // disabled.
  if (apps::features::ShouldShowLinkCapturingUX()) {
    app->supported_links = GetSupportedLinks(app->id, *provider);
  } else {
    app->supported_links = std::vector<std::string>();
  }

  if (!provider->registrar_unsafe().GetScopeExtensions(app->id).empty()) {
    app->formatted_origin = GetFormattedOrigin(app->id, *provider);
    app->scope_extensions = GetScopeExtensions(app->id, *provider);
  }

  app->hide_window_mode = provider->registrar_unsafe().IsIsolated(app->id);

  app->show_system_notifications_settings_link = false;
#if BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kAppShimNotificationAttribution)) {
    auto system_permission_status =
        AppShimRegistry::Get()->GetNotificationPermissionStatusForApp(app->id);
    app->show_system_notifications_settings_link =
        system_permission_status !=
            mac_notifications::mojom::PermissionStatus::kGranted &&
        system_permission_status !=
            mac_notifications::mojom::PermissionStatus::kNotDetermined;
  }
#endif

  return app;
}
