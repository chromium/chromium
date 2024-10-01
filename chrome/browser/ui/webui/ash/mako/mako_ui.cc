// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/lobster/lobster_controller.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/hash/sha1.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/input_method/editor_helpers.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/ash/lobster/lobster_service_provider.h"
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

constexpr int kEnUSResourceIds[] = {
    IDR_MAKO_ORCA_HTML, IDR_MAKO_PRIVACY_HTML, IDR_MAKO_LOBSTER_HTML,
    IDR_MAKO_ORCA_JS,   IDR_MAKO_LOBSTER_JS,   IDR_MAKO_ORCA_TRANSLATION_EN_JS};

constexpr int kLobsterResourceIds[] = {
    IDR_MAKO_LOBSTER_HTML,
    IDR_MAKO_LOBSTER_JS,
};

} // namespace

MakoUntrustedUIConfig::MakoUntrustedUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIUntrustedScheme,
                                  ash::kChromeUIMakoHost) {}

MakoUntrustedUIConfig::~MakoUntrustedUIConfig() = default;

bool MakoUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return chromeos::features::IsOrcaEnabled() ||
         ash::features::IsLobsterEnabled();
}

bool MakoUntrustedUIConfig::ShouldAutoResizeHost() {
  // With resizing support enabled or when lobster is enabled, we should let web
  // viewport resize according to dimension of web view rather than updating the
  // dimension of web view based on inner web content.
  return !base::FeatureList::IsEnabled(ash::features::kOrcaResizingSupport) &&
         !ash::features::IsLobsterEnabled();
}

MakoUntrustedUI::MakoUntrustedUI(content::WebUI* web_ui)
    : UntrustedTopChromeWebUIController(web_ui) {
  CHECK(chromeos::features::IsOrcaEnabled() ||
        ash::features::IsLobsterEnabled());

  // Setup the data source
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUIMakoURL);

  base::span<const webui::ResourcePath> orca_resources =
      base::make_span(kOrcaResources, kOrcaResourcesSize);

  const bool is_lobster_enabled = LobsterController::IsEnabled();
  const bool should_use_l10n_strings = input_method::ShouldUseL10nStrings();

  auto should_use_resource =
      [&](const webui::ResourcePath& resource_path) -> bool {
    // when lobster is disabled, lobster resources are not allowed.
    if (!is_lobster_enabled &&
        base::Contains(kLobsterResourceIds, resource_path.id)) {
      return false;
    }
    // when l10n is disabled, only EN-US resources are allowed.
    if (!should_use_l10n_strings &&
        !base::Contains(kEnUSResourceIds, resource_path.id)) {
      return false;
    }
    return true;
  };

  // TODO: b:333625296 - Add tests for this conditional behavior
  {
    std::vector<webui::ResourcePath> orca_en_us_resources;
    std::copy_if(orca_resources.begin(), orca_resources.end(),
                 std::back_inserter(orca_en_us_resources), should_use_resource);
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
  if (!chromeos::features::IsOrcaEnabled()) {
    mojo::ReportBadMessage("Editor is disabled by flags.");
    return;
  }

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

void MakoUntrustedUI::BindInterface(
    mojo::PendingReceiver<lobster::mojom::UntrustedLobsterPageHandler>
        pending_receiver) {
  if (!ash::features::IsLobsterEnabled()) {
    mojo::ReportBadMessage("Editor is disabled by flags.");
    return;
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  lobster_page_handler_ = std::make_unique<LobsterPageHandler>(
      LobsterServiceProvider::GetForProfile(profile)->active_session(),
      profile);

  lobster_page_handler_->BindInterface(std::move(pending_receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(MakoUntrustedUI)

}  // namespace ash
