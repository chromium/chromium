// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/hats/hats_ui.h"

#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/hats_resources.h"
#include "chrome/grit/hats_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

HatsUIConfig::HatsUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  chrome::kChromeUIUntrustedHatsHost) {}

bool HatsUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kHaTSWebUI);
}

std::unique_ptr<content::WebUIController> HatsUIConfig::CreateWebUIController(
    content::WebUI* web_ui,
    const GURL& url) {
  return std::make_unique<HatsUI>(web_ui);
}

HatsUI::HatsUI(content::WebUI* web_ui) : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUntrustedHatsURL);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source, base::make_span(kHatsResources, kHatsResourcesSize),
      IDR_HATS_HATS_HTML);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src "
      // The SHA256 hash of the initial inline JS.
      // Must be replaced when the inline js of hats.html changes.
      // The new hash can be viewed via terminal or via developer tools on
      // chrome-untrusted://hats if the page throws an error.
      "'sha256-gE2l7O/qvxOSNGhz8GPZzb9y0Ca6tLZWE1M0p/uvGt8=' "
      // Scripts loaded transitively from the initial one are allowed:
      "'strict-dynamic' "
      ";");

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src "

      // Unfortunately the HATS javascript does inject inline CSS:
      "'unsafe-inline' "

      // Origins of the CSS resources:
      "https://gstatic.com "
      "https://www.gstatic.com "
      "https://fonts.gstatic.com "
      "https://fonts.googleapis.com "
      ";");

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FontSrc,
      "font-src https://fonts.gstatic.com ;");

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src https://www.gstatic.com ;");

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      "frame-src https://scone-pa.clients6.google.com/ ;");

  // TODO(crbug.com/1481674): Enable TrustedType.
  source->DisableTrustedTypesCSP();
}

HatsUI::~HatsUI() = default;

void HatsUI::SetHatsPageHandlerDelegate(HatsPageHandlerDelegate* delegate) {
  page_handler_delegate_ = delegate;
}

void HatsUI::BindInterface(
    mojo::PendingReceiver<hats::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void HatsUI::CreatePageHandler(
    mojo::PendingRemote<hats::mojom::Page> page,
    mojo::PendingReceiver<hats::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<HatsPageHandler>(
      std::move(receiver), std::move(page), page_handler_delegate_);
}

WEB_UI_CONTROLLER_TYPE_IMPL(HatsUI)
