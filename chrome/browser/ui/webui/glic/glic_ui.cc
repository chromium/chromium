// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/glic/glic_ui.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/webui/glic/glic_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/glic_resources.h"
#include "chrome/grit/glic_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/webui_allowlist.h"

namespace glic {

GlicUIConfig::GlicUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme, chrome::kChromeUIGlicHost) {}

bool GlicUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kGlic);
}

GlicUI::GlicUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://glic source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), chrome::kChromeUIGlicHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, base::make_span(kGlicResources),
                              IDR_GLIC_GLIC_HTML);

  // TODO(crbug.com/378951332): Configure an approved CSP.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      "child-src 'self'"
      " https://*.google.com/"
      " https://*.googleplex.com/;");

  auto* command_line = base::CommandLine::ForCurrentProcess();
  source->AddString("glicGuestURL", command_line->GetSwitchValueASCII(
                                        ::switches::kGlicGuestURL));
  source->AddString(
      "glicGuestAPISource",
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_GENERATED_GLIC_API_IMPL_ROLLUP_JS));

  extensions::TabHelper::CreateForWebContents(web_ui->GetWebContents());
}

WEB_UI_CONTROLLER_TYPE_IMPL(GlicUI)

GlicUI::~GlicUI() = default;

void GlicUI::BindInterface(
    mojo::PendingReceiver<glic::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void GlicUI::CreatePageHandler(
    mojo::PendingRemote<glic::mojom::Page> page,
    mojo::PendingReceiver<glic::mojom::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<GlicPageHandler>(std::move(receiver), std::move(page));
}

}  // namespace glic
