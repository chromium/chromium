// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/hash/sha1.h"
#include "chrome/browser/ash/input_method/editor_helpers.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/mako/url_constants.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/orca_resources.h"
#include "chrome/grit/orca_resources_map.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace ash {
namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr int kEnUSResourceIds[] = {IDR_MAKO_ORCA_HTML, IDR_MAKO_PRIVACY_HTML,
                                    IDR_MAKO_ORCA_JS,
                                    IDR_MAKO_ORCA_TRANSLATION_EN_JS};
#else
constexpr int kEnUSResourceIds[] = {IDR_MAKO_ORCA_HTML, IDR_MAKO_PRIVACY_HTML,
                                    IDR_MAKO_ORCA_JS, IDR_MAKO_ORCA_EN};
#endif
} // namespace

MakoUntrustedUIConfig::MakoUntrustedUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIUntrustedScheme,
                                  ash::kChromeUIMakoHost) {}

MakoUntrustedUIConfig::~MakoUntrustedUIConfig() = default;

bool MakoUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return chromeos::features::IsOrcaEnabled();
}

bool MakoUntrustedUIConfig::ShouldAutoResizeHost() {
  // With resizing support enabled, we should let web viewport resize according
  // to dimension of web view rather than updating the dimension of web view
  // based on inner web content.
  return !base::FeatureList::IsEnabled(ash::features::kOrcaResizingSupport);
}

MakoUntrustedUI::MakoUntrustedUI(content::WebUI* web_ui)
    : UntrustedTopChromeWebUIController(web_ui) {
  CHECK(chromeos::features::IsOrcaEnabled());

  // Setup the data source
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUIMakoURL);

  base::span<const webui::ResourcePath> orca_resources =
      base::make_span(kOrcaResources, kOrcaResourcesSize);

  // TODO: b:333625296 - Add tests for this conditional behavior
  if (input_method::ShouldUseL10nStrings()) {
    webui::SetupWebUIDataSource(source, orca_resources, IDR_MAKO_ORCA_HTML);
  } else {
    std::vector<webui::ResourcePath> orca_en_us_resources;
    std::copy_if(orca_resources.begin(), orca_resources.end(),
                 std::back_inserter(orca_en_us_resources),
                 [](const webui::ResourcePath& resource_path) {
                   return base::Contains(kEnUSResourceIds, resource_path.id);
                 });
    webui::SetupWebUIDataSource(source, base::make_span(orca_en_us_resources),
                                IDR_MAKO_ORCA_HTML);
  }

  source->SetDefaultResource(IDR_MAKO_ORCA_HTML);

  // Setup additional CSP overrides
  // Intentional space at end of the strings - things are appended to this.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types goog#html polymer_resin lit-html "
      "polymer-template-event-attribute-policy polymer-html-literal; ");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'unsafe-inline'  chrome-untrusted://theme; ");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src data:; ");
}
MakoUntrustedUI::~MakoUntrustedUI() = default;

void MakoUntrustedUI::BindInterface(
    mojo::PendingReceiver<orca::mojom::EditorClient> pending_receiver) {
  // If mako ui is shown to the user, then we know that EditorMediator is
  // allowed for the current profile and will return a valid instance.
  input_method::EditorMediatorFactory::GetInstance()
      ->GetForProfile(Profile::FromWebUI(web_ui()))
      ->BindEditorClient(std::move(pending_receiver));
}

void MakoUntrustedUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(MakoUntrustedUI)

}  // namespace ash
