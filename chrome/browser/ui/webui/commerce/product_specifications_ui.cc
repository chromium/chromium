// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/product_specifications_ui.h"

#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/commerce_resources.h"
#include "chrome/grit/commerce_resources_map.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/shopping_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace commerce {

ProductSpecificationsUI::ProductSpecificationsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* const profile = Profile::FromWebUI(web_ui);
  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);
  if (!shopping_service ||
      !shopping_service->IsRegionLockedFeatureEnabled(
          kProductSpecifications, kProductSpecificationsRegionLaunched)) {
    return;
  }

  // Set up the chrome://compare source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUICompareHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source, base::make_span(kCommerceResources, kCommerceResourcesSize),
      IDR_COMMERCE_PRODUCT_SPECIFICATIONS_HTML);

  source->AddString("message", "Some example content...");
  source->AddString("pageTitle", "Compare");
  source->AddString("summaryTitle", "Summary");
}

void ProductSpecificationsUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ProductSpecificationsUI)

ProductSpecificationsUI::~ProductSpecificationsUI() = default;

ProductSpecificationsUIConfig::ProductSpecificationsUIConfig()
    : WebUIConfig(content::kChromeUIScheme, kChromeUICompareHost) {}

ProductSpecificationsUIConfig::~ProductSpecificationsUIConfig() = default;

std::unique_ptr<content::WebUIController>
ProductSpecificationsUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                     const GURL& url) {
  return std::make_unique<ProductSpecificationsUI>(web_ui);
}

}  // namespace commerce
