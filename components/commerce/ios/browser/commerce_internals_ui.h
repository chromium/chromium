// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_IOS_BROWSER_COMMERCE_INTERNALS_UI_H_
#define COMPONENTS_COMMERCE_IOS_BROWSER_COMMERCE_INTERNALS_UI_H_

#import "base/functional/callback.h"
#import "components/commerce/core/internals/commerce_internals_handler.h"
#import "components/commerce/core/internals/commerce_internals_ui_base.h"
#import "components/commerce/core/internals/mojom/commerce_internals.mojom.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"

namespace web {
class WebUIIOS;
}  // namespace web

namespace commerce {

class ShoppingService;

// The class supporting init of the iOS version of the internals page.
class CommerceInternalsUI : public CommerceInternalsUIBase,
                            public web::WebUIIOSController {
 public:
  CommerceInternalsUI(web::WebUIIOS* web_ui,
                      const std::string& host,
                      ShoppingService* shopping_service);
  CommerceInternalsUI(const CommerceInternalsUI&) = delete;
  CommerceInternalsUI operator&(const CommerceInternalsUI&) = delete;
  ~CommerceInternalsUI() override;

 private:
  base::WeakPtrFactory<CommerceInternalsUI> weak_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_IOS_BROWSER_COMMERCE_INTERNALS_UI_H_
