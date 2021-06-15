// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sample_system_web_app_ui/untrusted_sample_system_web_app_ui.h"

#include "chromeos/components/sample_system_web_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_sample_system_web_app_untrusted_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace chromeos {

UntrustedSampleSystemWebAppUIConfig::UntrustedSampleSystemWebAppUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  kChromeUIUntrustedSampleSystemWebAppHost) {}

UntrustedSampleSystemWebAppUIConfig::~UntrustedSampleSystemWebAppUIConfig() =
    default;

std::unique_ptr<content::WebUIController>
UntrustedSampleSystemWebAppUIConfig::CreateWebUIController(
    content::WebUI* web_ui) {
  return std::make_unique<UntrustedSampleSystemWebAppUI>(web_ui);
}

UntrustedSampleSystemWebAppUI::UntrustedSampleSystemWebAppUI(
    content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::Create(kChromeUIUntrustedSampleSystemWebAppURL);
  untrusted_source->AddResourcePaths(
      base::make_span(kChromeosSampleSystemWebAppUntrustedResources,
                      kChromeosSampleSystemWebAppUntrustedResourcesSize));
  untrusted_source->AddFrameAncestor(GURL(kChromeUISampleSystemWebAppURL));

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, untrusted_source);
}

UntrustedSampleSystemWebAppUI::~UntrustedSampleSystemWebAppUI() = default;

}  // namespace chromeos
