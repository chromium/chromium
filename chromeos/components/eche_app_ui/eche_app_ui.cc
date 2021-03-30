// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chromeos/components/eche_app_ui/eche_app_manager.h"
#include "chromeos/components/eche_app_ui/eche_app_ui.h"
#include "chromeos/components/eche_app_ui/mojom/eche_app.mojom.h"
#include "chromeos/components/eche_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_eche_app_resources.h"
#include "chromeos/grit/chromeos_eche_bundle_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace eche_app {

EcheAppUI::EcheAppUI(content::WebUI* web_ui,
                     BindSignalingMessageExchangerCallback exchanger_callback)
    : ui::MojoWebUIController(web_ui),
      bind_exchanger_callback_(std::move(exchanger_callback)) {
  auto html_source =
      base::WrapUnique(content::WebUIDataSource::Create(kChromeUIEcheAppHost));

  html_source->AddResourcePath("", IDR_CHROMEOS_ECHE_INDEX_HTML);
  html_source->AddResourcePath("system_assets/app_icon_32.png",
                               IDR_CHROMEOS_ECHE_APP_ICON_32_PNG);
  html_source->AddResourcePath("system_assets/app_icon_256.png",
                               IDR_CHROMEOS_ECHE_APP_ICON_256_PNG);
  html_source->AddResourcePath("js/app_bundle.js",
                               IDR_CHROMEOS_ECHE_APP_BUNDLE_JS);
  html_source->AddResourcePath("assets/app_bundle.css",
                               IDR_CHROMEOS_ECHE_APP_BUNDLE_CSS);
  html_source->AddResourcePath(
      "eche_app.mojom-lite.js",
      IDR_CHROMEOS_ECHE_APP_CHROMEOS_COMPONENTS_ECHE_APP_UI_MOJOM_ECHE_APP_MOJOM_LITE_JS);
  html_source->AddResourcePath("browser_proxy.js",
                               IDR_CHROMEOS_ECHE_APP_BROWSER_PROXY_JS);

  // DisableTrustedTypesCSP to support TrustedTypePolicy named 'goog#html'.
  // It is the Closure templating system that renders our UI, as it does many
  // other web apps using it.
  html_source->DisableTrustedTypesCSP();
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source.release());
}

EcheAppUI::~EcheAppUI() = default;

void EcheAppUI::BindInterface(
    mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver) {
  bind_exchanger_callback_.Run(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(EcheAppUI)

}  // namespace eche_app
}  // namespace chromeos
