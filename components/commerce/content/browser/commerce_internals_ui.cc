// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "components/commerce/content/browser/commerce_internals_ui.h"

#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/shopping_service.h"
#include "components/grit/commerce_internals_resources.h"
#include "components/grit/commerce_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace commerce {

CommerceInternalsUI::CommerceInternalsUI(
    content::WebUI* web_ui,
    ShoppingService* shopping_service)
    : CommerceInternalsUIBase(shopping_service),
      ui::MojoWebUIController(web_ui, true) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      commerce::kChromeUICommerceInternalsHost);
  webui::SetupWebUIDataSource(source, kCommerceInternalsResources,
                              IDR_COMMERCE_INTERNALS_COMMERCE_INTERNALS_HTML);
}

CommerceInternalsUI::~CommerceInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(CommerceInternalsUI)

}  // namespace commerce
