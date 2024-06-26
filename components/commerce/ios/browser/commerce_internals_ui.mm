// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/commerce/ios/browser/commerce_internals_ui.h"

#import "components/commerce/core/commerce_constants.h"
#import "components/commerce/core/shopping_service.h"
#import "components/grit/commerce_internals_resources.h"
#import "components/grit/commerce_internals_resources_map.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ui/base/webui/resource_path.h"

namespace commerce {

CommerceInternalsUI::CommerceInternalsUI(web::WebUIIOS* web_ui,
                                         const std::string& host,
                                         ShoppingService* shopping_service)
    : CommerceInternalsUIBase(shopping_service),
      web::WebUIIOSController(web_ui, host) {
  web::BrowserState* browser_state = web_ui->GetWebState()->GetBrowserState();

  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUICommerceInternalsHost);
  source->SetDefaultResource(IDR_COMMERCE_INTERNALS_COMMERCE_INTERNALS_HTML);
  source->UseStringsJs();
  const base::span<const webui::ResourcePath> resources = base::make_span(
      kCommerceInternalsResources, kCommerceInternalsResourcesSize);

  for (const auto& resource : resources) {
    source->AddResourcePath(resource.path, resource.id);
  }

  web::WebUIIOSDataSource::Add(browser_state, source);
  web_ui->GetWebState()->GetInterfaceBinderForMainFrame()->AddInterface(
      base::BindRepeating(&CommerceInternalsUI::BindInterface,
                          weak_factory_.GetWeakPtr()));
}

CommerceInternalsUI::~CommerceInternalsUI() {
  web_ui()->GetWebState()->GetInterfaceBinderForMainFrame()->RemoveInterface(
      "commerce_internals.mojom.CommerceInternalsHandlerFactory");
}

}  // namespace commerce
