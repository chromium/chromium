// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CONTENT_BROWSER_COMMERCE_INTERNALS_UI_H_
#define COMPONENTS_COMMERCE_CONTENT_BROWSER_COMMERCE_INTERNALS_UI_H_

#include "base/functional/callback.h"
#include "components/commerce/core/internals/commerce_internals_handler.h"
#include "components/commerce/core/internals/commerce_internals_ui_base.h"
#include "components/commerce/core/internals/mojom/commerce_internals.mojom.h"
#include "ui/base/webui/resource_path.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace commerce {

class ShoppingService;

// The class supporting init of the "content" version of the internals page.
class CommerceInternalsUI : public CommerceInternalsUIBase,
                            public ui::MojoWebUIController {
 public:
  using SetupWebUIDataSourceCallback =
      base::OnceCallback<void(base::span<const webui::ResourcePath> resources,
                              int default_resource)>;

  CommerceInternalsUI(content::WebUI* web_ui,
                      SetupWebUIDataSourceCallback setup_callback,
                      ShoppingService* shopping_service);
  CommerceInternalsUI(const CommerceInternalsUI&) = delete;
  CommerceInternalsUI operator&(const CommerceInternalsUI&) = delete;
  ~CommerceInternalsUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CONTENT_BROWSER_COMMERCE_INTERNALS_UI_H_
