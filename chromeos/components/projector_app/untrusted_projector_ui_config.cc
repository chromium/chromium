// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/projector_app/untrusted_projector_ui_config.h"

#include "chromeos/components/projector_app/projector_app_constants.h"
#include "chromeos/grit/chromeos_projector_app_untrusted_resources.h"
#include "chromeos/grit/chromeos_projector_app_untrusted_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

content::WebUIDataSource* CreateProjectorHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIUntrustedProjectorAppUrl);

  const auto resources =
      base::make_span(kChromeosProjectorAppUntrustedResources,
                      kChromeosProjectorAppUntrustedResourcesSize);
  source->AddResourcePaths(resources);

  // TODO(b/193579885): Add ink WASM.
  // TODO(b/193579885): Override content security policy to support loading wasm
  // resources.

  source->AddFrameAncestor(GURL(kChromeUITrustedProjectorAppUrl));

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
