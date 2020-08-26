// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "chrome/browser/ui/webui/internals/web_app/web_app_internals_page_handler_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"

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
  auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile_);
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
  for (const web_app::WebApp& web_app : registrar->AllApps()) {
    mojom::web_app_internals::WebAppPtr info(
        mojom::web_app_internals::WebApp::New());
    info->name = web_app.name();
    info->id = web_app.app_id();
    std::stringstream ss;
    ss << web_app;
    info->debug_info = ss.str();
    result.push_back(std::move(info));
  }

  std::move(callback).Run(std::move(result));
}

void WebAppInternalsPageHandlerImpl::GetExternallyInstalledWebAppPrefs(
    GetExternallyInstalledWebAppPrefsCallback callback) {
  std::stringstream ss;
  ss << *profile_->GetPrefs()->GetDictionary(prefs::kWebAppsExtensionIDs);
  std::move(callback).Run(ss.str());
}
