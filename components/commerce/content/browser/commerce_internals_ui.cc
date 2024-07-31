// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/commerce/content/browser/commerce_internals_ui.h"

#include "components/commerce/core/shopping_service.h"
#include "components/grit/commerce_internals_resources.h"
#include "components/grit/commerce_internals_resources_map.h"
#include "content/public/browser/web_ui.h"

namespace commerce {

CommerceInternalsUI::CommerceInternalsUI(
    content::WebUI* web_ui,
    SetupWebUIDataSourceCallback setup_callback,
    ShoppingService* shopping_service)
    : CommerceInternalsUIBase(shopping_service),
      ui::MojoWebUIController(web_ui, true) {
  std::move(setup_callback)
      .Run(base::make_span(kCommerceInternalsResources,
                           kCommerceInternalsResourcesSize),
           IDR_COMMERCE_INTERNALS_COMMERCE_INTERNALS_HTML);
}

CommerceInternalsUI::~CommerceInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(CommerceInternalsUI)

}  // namespace commerce
