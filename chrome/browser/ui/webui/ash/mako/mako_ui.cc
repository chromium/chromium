// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"
#include "chrome/browser/ash/input_method/editor_mediator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/mako/url_constants.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/orca_resources.h"
#include "chrome/grit/orca_resources_map.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/untrusted_bubble_web_ui_controller.h"

namespace ash {

MakoUntrustedUIConfig::MakoUntrustedUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme, ash::kChromeUIMakoHost) {}

MakoUntrustedUIConfig::~MakoUntrustedUIConfig() = default;

std::unique_ptr<content::WebUIController>
MakoUntrustedUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                             const GURL& url) {
  return std::make_unique<MakoUntrustedUI>(web_ui);
}

bool MakoUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return chromeos::features::IsOrcaEnabled();
}

MakoUntrustedUI::MakoUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedBubbleWebUIController(web_ui) {
  CHECK(chromeos::features::IsOrcaEnabled());

  const std::string debug_key_hash = base::SHA1HashString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ash::switches::kOrcaKey));
  // See go/orca-key for the key
  // Commandline looks like:
  //  out/Default/chrome --user-data-dir=/tmp/auuf123 --orca-key="INSERT KEY
  //  HERE" --enable-features=Orca
  const std::string hash =
      "\x7a\xf3\xa1\x57\x28\x48\xc4\x14\x27\x13\x53\x5a\x09\xf3\x0e\xfc\xee\xa6"
      "\xbb\xa4";
  // If key fails to match, crash chrome.
  CHECK_EQ(debug_key_hash, hash);

  // Setup the data source
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUIMakoURL);
  webui::SetupWebUIDataSource(
      source, base::make_span(kOrcaResources, kOrcaResourcesSize),
      IDR_MAKO_ORCA_HTML);
  source->SetDefaultResource(IDR_MAKO_ORCA_HTML);

  // Setup additional CSP overrides
  // Intentional space at end of the strings - things are appended to this.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types goog#html polymer_resin lit-html "
      "polymer-template-event-attribute-policy polymer-html-literal; ");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'unsafe-inline'; ");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src data:; ");
}
MakoUntrustedUI::~MakoUntrustedUI() = default;

void MakoUntrustedUI::BindInterface(
    mojo::PendingReceiver<orca::mojom::EditorClient> pending_receiver) {
  input_method::EditorMediator::Get()->BindEditorClient(
      std::move(pending_receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(MakoUntrustedUI)

}  // namespace ash
