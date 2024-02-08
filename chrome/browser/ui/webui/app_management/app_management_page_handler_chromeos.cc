// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_page_handler_chromeos.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler_base.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "extensions/common/constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

namespace {

// Returns a list of intent filters that support http/https given an app ID.
apps::IntentFilters GetSupportedLinkIntentFilters(Profile* profile,
                                                  const std::string& app_id) {
  apps::IntentFilters intent_filters;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id,
                 [&app_id, &intent_filters](const apps::AppUpdate& update) {
                   if (update.Readiness() == apps::Readiness::kReady) {
                     for (auto& filter : update.IntentFilters()) {
                       if (apps_util::IsSupportedLinkForApp(app_id, filter)) {
                         intent_filters.emplace_back(std::move(filter));
                       }
                     }
                   }
                 });
  return intent_filters;
}

}  // namespace

AppManagementPageHandlerChromeOs::AppManagementPageHandlerChromeOs(
    mojo::PendingReceiver<app_management::mojom::PageHandler> receiver,
    mojo::PendingRemote<app_management::mojom::Page> page,
    Profile* profile,
    AppManagementPageHandlerBase::Delegate& delegate)
    : AppManagementPageHandlerBase(std::move(receiver),
                                   std::move(page),
                                   profile,
                                   delegate) {}

AppManagementPageHandlerChromeOs::~AppManagementPageHandlerChromeOs() = default;

void AppManagementPageHandlerChromeOs::SetResizeLocked(
    const std::string& app_id,
    bool locked) {
  apps::AppServiceProxyFactory::GetForProfile(profile())->SetResizeLocked(
      app_id, locked);
}

void AppManagementPageHandlerChromeOs::SetPreferredApp(
    const std::string& app_id,
    bool is_preferred_app) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  bool is_preferred_app_for_supported_links =
      proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(app_id);

  if (is_preferred_app && !is_preferred_app_for_supported_links) {
    proxy->SetSupportedLinksPreference(app_id);
  } else if (!is_preferred_app && is_preferred_app_for_supported_links) {
    proxy->RemoveSupportedLinksPreference(app_id);
  }
}

void AppManagementPageHandlerChromeOs::GetOverlappingPreferredApps(
    const std::string& app_id,
    GetOverlappingPreferredAppsCallback callback) {
  auto intent_filters = GetSupportedLinkIntentFilters(profile(), app_id);
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  base::flat_set<std::string> app_ids =
      proxy->PreferredAppsList().FindPreferredAppsForFilters(intent_filters);
  app_ids.erase(app_id);

  // Erase all IDs that do not correspond to installed apps in App Service. Such
  // IDs could be apps that have been uninstalled but did not have their
  // preference updated correctly, or the legacy "use_browser" preference. This
  // prevents attempting to show an overlapping app dialog for an app that
  // doesn't currently exist.
  base::EraseIf(app_ids, [proxy](const std::string& app_id) {
    return !proxy->AppRegistryCache().IsAppInstalled(app_id);
  });
  std::move(callback).Run(std::move(app_ids).extract());
}

void AppManagementPageHandlerChromeOs::SetWindowMode(
    const std::string& app_id,
    apps::WindowMode window_mode) {
  NOTIMPLEMENTED();
}

void AppManagementPageHandlerChromeOs::SetRunOnOsLoginMode(
    const std::string& app_id,
    apps::RunOnOsLoginMode run_on_os_login_mode) {
  NOTIMPLEMENTED();
}

void AppManagementPageHandlerChromeOs::ShowDefaultAppAssociationsUi() {
  NOTIMPLEMENTED();
}

void AppManagementPageHandlerChromeOs::OpenStorePage(
    const std::string& app_id) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  auto* apk_service = ash::ApkWebAppService::Get(profile());
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&proxy, &apk_service](const apps::AppUpdate& update) {
        if (update.InstallSource() == apps::InstallSource::kPlayStore) {
          std::string package_name = update.PublisherId();
          if (apk_service->IsWebAppInstalledFromArc(update.AppId())) {
            package_name =
                apk_service->GetPackageNameForWebApp(update.AppId()).value();
          }
          GURL url("https://play.google.com/store/apps/details?id=" +
                   package_name);
          proxy->LaunchAppWithUrl(arc::kPlayStoreAppId, ui::EF_NONE, url,
                                  apps::LaunchSource::kFromChromeInternal);
        } else if (update.InstallSource() ==
                   apps::InstallSource::kChromeWebStore) {
          GURL url("https://chrome.google.com/webstore/detail/" +
                   update.AppId());
          proxy->LaunchAppWithUrl(extensions::kWebStoreAppId, ui::EF_NONE, url,
                                  apps::LaunchSource::kFromChromeInternal);
        }
      });
}

void AppManagementPageHandlerChromeOs::SetAppLocale(
    const std::string& app_id,
    const std::string& locale_tag) {
  apps::AppServiceProxyFactory::GetForProfile(profile())->SetAppLocale(
      app_id, locale_tag);
}
