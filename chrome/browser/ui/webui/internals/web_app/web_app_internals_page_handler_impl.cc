// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/web_app/web_app_internals_page_handler_impl.h"

#include <sstream>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/external_web_app_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

template <typename T>
std::string ConvertToString(const T& value) {
  std::stringstream ss;
  ss << value;
  return ss.str();
}

}  // namespace

WebAppInternalsPageHandlerImpl::WebAppInternalsPageHandlerImpl(Profile* profile)
    : profile_(profile) {}

WebAppInternalsPageHandlerImpl::~WebAppInternalsPageHandlerImpl() = default;

void WebAppInternalsPageHandlerImpl::AddPageResources(
    content::WebUIDataSource* source) {
  source->DisableTrustedTypesCSP();
  source->AddResourcePath("web_app_internals.mojom-lite.js",
                          IDR_WEB_APP_INTERNALS_MOJOM_LITE_JS);
  source->AddResourcePath("web_app_internals.js", IDR_WEB_APP_INTERNALS_JS);
  source->AddResourcePath("web-app", IDR_WEB_APP_INTERNALS_HTML);
}

void WebAppInternalsPageHandlerImpl::IsBmoEnabled(
    IsBmoEnabledCallback callback) {
  std::move(callback).Run(
      base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions));
}

void WebAppInternalsPageHandlerImpl::GetWebApps(GetWebAppsCallback callback) {
  auto* provider = web_app::WebAppProvider::Get(profile_);
  if (!provider) {
    std::move(callback).Run({});
    return;
  }

  web_app::AppRegistrar& registrar_base = provider->registrar();
  web_app::WebAppRegistrar* registrar = registrar_base.AsWebAppRegistrar();
  if (!registrar) {
    std::move(callback).Run({});
    return;
  }

  std::vector<mojom::web_app_internals::WebAppPtr> result;
  for (const web_app::WebApp& web_app : registrar->GetAppsIncludingStubs()) {
    auto info = mojom::web_app_internals::WebApp::New();
    info->name = web_app.name();
    info->id = web_app.app_id();
    info->debug_info = ConvertToString(web_app);
    result.push_back(std::move(info));
  }

  std::move(callback).Run(std::move(result));
}

void WebAppInternalsPageHandlerImpl::GetPreinstalledWebAppDebugInfo(
    GetPreinstalledWebAppDebugInfoCallback callback) {
  auto* provider = web_app::WebAppProvider::Get(profile_);
  if (!provider) {
    std::move(callback).Run({});
    return;
  }

  const web_app::ExternalWebAppManager::DebugInfo* debug_info =
      provider->external_web_app_manager().debug_info();
  if (!debug_info) {
    std::move(callback).Run({});
    return;
  }

  auto info = mojom::web_app_internals::PreinstalledWebAppDebugInfo::New();

  info->is_start_up_task_complete = debug_info->is_start_up_task_complete;

  info->parse_errors = debug_info->parse_errors;

  for (const web_app::ExternalInstallOptions& config :
       debug_info->enabled_configs) {
    info->enabled_configs.push_back(ConvertToString(config));
  }

  for (const std::pair<web_app::ExternalInstallOptions, std::string>&
           disabled_config : debug_info->disabled_configs) {
    auto disabled_config_info = mojom::web_app_internals::DisabledConfig::New();
    disabled_config_info->config = ConvertToString(disabled_config.first);
    disabled_config_info->reason = disabled_config.second;
    info->disabled_configs.push_back(std::move(disabled_config_info));
  }

  for (std::pair<const GURL&, const web_app::PendingAppManager::InstallResult&>
           install_result : debug_info->install_results) {
    auto install_result_info = mojom::web_app_internals::InstallResult::New();
    install_result_info->install_url = install_result.first.spec();
    install_result_info->install_result_code =
        ConvertToString(install_result.second.code);
    install_result_info->did_uninstall_and_replace =
        install_result.second.did_uninstall_and_replace;
    info->install_results.push_back(std::move(install_result_info));
  }

  for (std::pair<const GURL&, const bool&> uninstall_result :
       debug_info->uninstall_results) {
    auto uninstall_result_info =
        mojom::web_app_internals::UninstallResult::New();
    uninstall_result_info->install_url = uninstall_result.first.spec();
    uninstall_result_info->is_success = uninstall_result.second;
    info->uninstall_results.push_back(std::move(uninstall_result_info));
  }

  std::move(callback).Run(std::move(info));
}

void WebAppInternalsPageHandlerImpl::GetExternallyInstalledWebAppPrefs(
    GetExternallyInstalledWebAppPrefsCallback callback) {
  std::move(callback).Run(ConvertToString(
      *profile_->GetPrefs()->GetDictionary(prefs::kWebAppsExtensionIDs)));
}
