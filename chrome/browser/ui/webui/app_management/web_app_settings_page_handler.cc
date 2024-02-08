// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/web_app_settings_page_handler.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler_base.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/default_apps_util.h"
#endif

WebAppSettingsPageHandler::WebAppSettingsPageHandler(
    mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
    mojo::PendingRemote<app_management::mojom::Page> page,
    Profile* profile,
    AppManagementPageHandlerBase::Delegate& delegate)
    : AppManagementPageHandlerBase(std::move(receiver),
                                   std::move(page),
                                   profile,
                                   delegate) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  registrar_observation_.Observe(&provider->registrar_unsafe());
}

WebAppSettingsPageHandler::~WebAppSettingsPageHandler() = default;

void WebAppSettingsPageHandler::SetResizeLocked(const std::string& app_id,
                                                bool locked) {
  NOTIMPLEMENTED();
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
  apps::AppServiceProxyFactory::GetForProfile(profile())->SetRunOnOsLoginMode(
      app_id, run_on_os_login_mode);
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
