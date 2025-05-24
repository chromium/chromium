// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/commerce_internals_ui_config.h"

#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/commerce/content/browser/commerce_internals_ui.h"
#include "components/commerce/core/commerce_constants.h"

namespace commerce {

CommerceInternalsUIConfig::CommerceInternalsUIConfig()
    : InternalWebUIConfig(commerce::kChromeUICommerceInternalsHost) {}

CommerceInternalsUIConfig::~CommerceInternalsUIConfig() = default;

std::unique_ptr<content::WebUIController>
CommerceInternalsUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                 const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  return std::make_unique<CommerceInternalsUI>(
      web_ui, commerce::ShoppingServiceFactory::GetForBrowserContext(profile));
}

}  // namespace commerce
