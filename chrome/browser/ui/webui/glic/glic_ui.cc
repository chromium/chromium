// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/glic/glic_ui.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/glic/guest_util.h"
#include "chrome/browser/ui/webui/glic/glic_page_handler.h"
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
#include "ui/webui/webui_util.h"

namespace glic {

GlicUIConfig::GlicUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme, chrome::kChromeUIGlicHost) {}

bool GlicUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kGlic);
}

GlicUI::GlicUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  // Set up the chrome://glic source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, chrome::kChromeUIGlicHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, kGlicResources, IDR_GLIC_GLIC_HTML);

  // Register auto-granted permissions.
  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  allowlist->RegisterAutoGrantedPermission(source->GetOrigin(),
                                           ContentSettingsType::GEOLOCATION);

  auto* command_line = base::CommandLine::ForCurrentProcess();

  // Set up guest URL via cli flag or default to finch param value.
  source->AddString("glicGuestURL", GetGuestURL().spec());

  // Set up guest api source.
  // This comes from 'glic_api_injection' in
  // chrome/browser/resources/glic/BUILD.gn.
  source->AddString(
      "glicGuestAPISource",
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_GLIC_GLIC_API_GLIC_API_INJECTED_CLIENT_ROLLUP_JS));

  // TODO(crbug.com/378951332): Configure an approved CSP.
  // Set up csp override by cli flag or default to finch param value. This will
  // be removed when we go to canary since it will no longer be needed once
  // crbug.com/378951332 is addressed.
  bool hasCSPOverride = command_line->HasSwitch(::switches::kCSPOverride);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      hasCSPOverride
          ? command_line->GetSwitchValueASCII(::switches::kCSPOverride)
          : features::kGlicWebUICSPOverride.Get());

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
    mojo::PendingReceiver<glic::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<GlicPageHandler>(
      web_ui()->GetWebContents()->GetBrowserContext(), std::move(receiver));
}

}  // namespace glic
