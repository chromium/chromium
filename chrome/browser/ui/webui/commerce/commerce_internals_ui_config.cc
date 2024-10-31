// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/commerce_internals_ui_config.h"

#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "components/commerce/content/browser/commerce_internals_ui.h"
#include "components/commerce/core/commerce_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

namespace commerce {

void SetUpWebUIDataSource(content::WebUI* web_ui,
                          const char* web_ui_host,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), web_ui_host);
  webui::SetupWebUIDataSource(source, resources, default_resource);
}

CommerceInternalsUIConfig::CommerceInternalsUIConfig()
    : WebUIConfig(content::kChromeUIScheme,
                  commerce::kChromeUICommerceInternalsHost) {}

CommerceInternalsUIConfig::~CommerceInternalsUIConfig() = default;

std::unique_ptr<content::WebUIController>
CommerceInternalsUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                 const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  return std::make_unique<CommerceInternalsUI>(
      web_ui,
      base::BindOnce(&SetUpWebUIDataSource, web_ui,
                     commerce::kChromeUICommerceInternalsHost),
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile));
}

}  // namespace commerce
