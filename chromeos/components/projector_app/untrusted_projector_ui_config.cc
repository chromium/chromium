// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/projector_app/untrusted_projector_ui_config.h"

#include "chromeos/components/projector_app/projector_app_constants.h"
#include "chromeos/grit/chromeos_projector_app_bundle_resources.h"
#include "chromeos/grit/chromeos_projector_app_bundle_resources_map.h"
#include "chromeos/grit/chromeos_projector_app_untrusted_resources.h"
#include "chromeos/grit/chromeos_projector_app_untrusted_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_generated_resources_map.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

content::WebUIDataSource* CreateProjectorHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIUntrustedProjectorAppUrl);

  source->AddResourcePaths(
      base::make_span(kChromeosProjectorAppUntrustedResources,
                      kChromeosProjectorAppUntrustedResourcesSize));
  source->AddResourcePaths(
      base::make_span(kChromeosProjectorAppBundleResources,
                      kChromeosProjectorAppBundleResourcesSize));
  source->AddResourcePath("",
                          IDR_CHROMEOS_PROJECTOR_APP_UNTRUSTED_APP_INDEX_HTML);

  // Allows WebUI resources like Polymer and PostMessageAPI to be accessible
  // inside the untrusted iframe.
  source->AddResourcePaths(
      base::make_span(kWebuiGeneratedResources, kWebuiGeneratedResourcesSize));

  // Provide a list of specific script resources(javascript files and inlined
  // scripts inside html) or their sha-256 hashes to allow to be executed.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      // Allows loading javascript files from the current origin
      "script-src 'self';");

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc,
      "connect-src 'self' https://www.googleapis.com;");

  // TODO(b/197120695): re-enable trusted type after fixing the issue that icon
  // template is setting innerHTML.
  source->DisableTrustedTypesCSP();

  // TODO(b/193579885): Add ink WASM.
  // TODO(b/193579885): Override content security policy to support loading wasm
  // resources.

  source->AddFrameAncestor(GURL(kChromeUITrustedProjectorUrl));

  // TODO(b/201666699): Move this into a delegate, and populate with real data.
  source->AddBoolean("isDevChannel", true);
  source->UseStringsJs();

  return source;
}

// The implementation for the untrusted projector WebUI.
class UntrustedProjectorUI : public ui::UntrustedWebUIController {
 public:
  explicit UntrustedProjectorUI(content::WebUI* web_ui)
      : ui::UntrustedWebUIController(web_ui) {
    auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
    content::WebUIDataSource::Add(browser_context, CreateProjectorHTMLSource());
  }

  UntrustedProjectorUI(const UntrustedProjectorUI&) = delete;
  UntrustedProjectorUI& operator=(const UntrustedProjectorUI&) = delete;
  ~UntrustedProjectorUI() override = default;
};

}  // namespace

UntrustedProjectorUIConfig::UntrustedProjectorUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  kChromeUIProjectorAppHost) {}

UntrustedProjectorUIConfig::~UntrustedProjectorUIConfig() = default;

std::unique_ptr<content::WebUIController>
UntrustedProjectorUIConfig::CreateWebUIController(content::WebUI* web_ui) {
  return std::make_unique<UntrustedProjectorUI>(web_ui);
}

}  // namespace chromeos
