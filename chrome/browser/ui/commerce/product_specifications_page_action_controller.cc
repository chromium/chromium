// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/product_specifications_page_action_controller.h"

#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/shopping_service.h"

namespace commerce {

ProductSpecificationsPageActionController::
    ProductSpecificationsPageActionController(
        base::RepeatingCallback<void()> notify_callback,
        ShoppingService* shopping_service)
    : CommercePageActionController(std::move(notify_callback)),
      shopping_service_(shopping_service) {}

ProductSpecificationsPageActionController::
    ~ProductSpecificationsPageActionController() = default;

std::optional<bool>
ProductSpecificationsPageActionController::ShouldShowForNavigation() {
  // If the user isn't eligible for the feature, don't block.
  if (!shopping_service_ || !shopping_service_->GetAccountChecker() ||
      !commerce::IsProductSpecificationsEnabled(
          shopping_service_->GetAccountChecker())) {
    return false;
  }
  // If the page is not yet determined to be a product page, we're "undecided".
  if (!got_product_response_for_page_) {
    return std::nullopt;
  }
  // If we got a response from the shopping service but the response was empty,
  // we don't need to know about the product group info.
  if (got_product_response_for_page_ && !product_info_for_page_.has_value()) {
    return false;
  }
  return product_group_for_page_.has_value();
}

bool ProductSpecificationsPageActionController::WantsExpandedUi() {
  return product_group_for_page_.has_value();
}

void ProductSpecificationsPageActionController::ResetForNewNavigation(
    const GURL& url) {
  if (!shopping_service_ || !shopping_service_->GetAccountChecker() ||
      !commerce::IsProductSpecificationsEnabled(
          shopping_service_->GetAccountChecker())) {
    return;
  }
  // Cancel any pending callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  current_url_ = url;
  got_product_response_for_page_ = false;
  product_group_for_page_ = std::nullopt;
  product_info_for_page_ = std::nullopt;
  // Initiate an update for the icon on navigation since we may not have product
  // info.
  NotifyHost();

  shopping_service_->GetProductInfoForUrl(
      url,
      base::BindOnce(
          &ProductSpecificationsPageActionController::HandleProductInfoResponse,
          weak_ptr_factory_.GetWeakPtr()));
}

void ProductSpecificationsPageActionController::HandleProductInfoResponse(
    const GURL& url,
    const std::optional<const ProductInfo>& info) {
  if (url == current_url_ && info.has_value()) {
    product_info_for_page_ = info;
    product_group_for_page_ =
        shopping_service_->GetProductGroupForCandidateProduct(url);
  }
  got_product_response_for_page_ = true;
  NotifyHost();
}
}  // namespace commerce
