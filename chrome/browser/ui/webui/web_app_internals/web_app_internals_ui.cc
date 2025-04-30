// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_ui.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_handler.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/web_app_internals_resources.h"
#include "chrome/grit/web_app_internals_resources_map.h"
#include "components/webapps/isolated_web_apps/features.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_util.h"

WebAppInternalsUI::WebAppInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  // Set up the chrome://web-app-internals source.
  content::WebUIDataSource* internals = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIWebAppInternalsHost);
  webui::SetupWebUIDataSource(internals, kWebAppInternalsResources,
                              IDR_WEB_APP_INTERNALS_WEB_APP_INTERNALS_HTML);
  internals->UseStringsJs();
  internals->AddBoolean("isIwaDevModeEnabled",
                        web_app::IsIwaDevModeEnabled(profile));
  internals->AddBoolean(
      "isIwaKeyDistributionDevModeEnabled",
      web_app::IsIwaDevModeEnabled(profile) &&
          base::FeatureList::IsEnabled(web_app::kIwaKeyDistributionDevMode));
  internals->AddBoolean("isIwaPolicyInstallEnabled",
                        content::AreIsolatedWebAppsEnabled(profile));
}

WEB_UI_CONTROLLER_TYPE_IMPL(WebAppInternalsUI)

WebAppInternalsUI::~WebAppInternalsUI() = default;

void WebAppInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::WebAppInternalsHandler> receiver) {
  page_handler_ =
      std::make_unique<WebAppInternalsHandler>(web_ui(), std::move(receiver));
}
