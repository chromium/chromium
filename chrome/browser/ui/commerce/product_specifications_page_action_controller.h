// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_PRODUCT_SPECIFICATIONS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_COMMERCE_PRODUCT_SPECIFICATIONS_PAGE_ACTION_CONTROLLER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/commerce/commerce_page_action_controller.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/compare/product_group.h"

namespace commerce {

class ShoppingService;

class ProductSpecificationsPageActionController
    : public CommercePageActionController {
 public:
  ProductSpecificationsPageActionController(
      base::RepeatingCallback<void()> notify_callback,
      ShoppingService* shopping_service);
  ProductSpecificationsPageActionController(
      const ProductSpecificationsPageActionController&) = delete;
  ProductSpecificationsPageActionController& operator=(
      const ProductSpecificationsPageActionController&) = delete;
  ~ProductSpecificationsPageActionController() override;

  // CommercePageActionController impl:
  std::optional<bool> ShouldShowForNavigation() override;
  bool WantsExpandedUi() override;
  void ResetForNewNavigation(const GURL& url) override;

 private:
  void HandleProductInfoResponse(const GURL& url,
                                 const std::optional<const ProductInfo>& info);

  // The URL for the most recent navigation.
  GURL current_url_;

  // Whether we have got the response for checking if the current page is a
  // product page.
  bool got_product_response_for_page_{false};

  // The product info available for the current page if available.
  std::optional<ProductInfo> product_info_for_page_;

  // The product group that current page can be added to if available.
  std::optional<ProductGroup> product_group_for_page_;

  // The shopping service is tied to the lifetime of the browser context
  // which will always outlive this tab helper.
  raw_ptr<ShoppingService> shopping_service_;

  base::WeakPtrFactory<ProductSpecificationsPageActionController>
      weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_COMMERCE_PRODUCT_SPECIFICATIONS_PAGE_ACTION_CONTROLLER_H_
