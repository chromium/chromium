// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/discounts_page_action_controller.h"

#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/shopping_service.h"

namespace commerce {
DiscountsPageActionController::DiscountsPageActionController(
    base::RepeatingCallback<void()> notify_callback,
    ShoppingService* shopping_service)
    : CommercePageActionController(std::move(notify_callback)),
      shopping_service_(shopping_service) {}

DiscountsPageActionController::~DiscountsPageActionController() = default;

std::optional<bool> DiscountsPageActionController::ShouldShowForNavigation() {
  if (!shopping_service_ ||
      !shopping_service_->IsDiscountEligibleToShowOnNavigation()) {
    return false;
  }

  if (!got_discounts_response_for_page_) {
    return std::nullopt;
  }

  return discounts_.has_value();
}

bool DiscountsPageActionController::WantsExpandedUi() {
  return got_discounts_response_for_page_ && discounts_.has_value() &&
         !discounts_.value().empty();
}

void DiscountsPageActionController::ResetForNewNavigation(const GURL& url) {
  if (!shopping_service_ ||
      !shopping_service_->IsDiscountEligibleToShowOnNavigation()) {
    return;
  }

  // Cancel any pending callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  discounts_ = {};
  got_discounts_response_for_page_ = false;
  last_committed_url_ = url;
  NotifyHost();

  shopping_service_->GetDiscountInfoForUrls(
      {url},
      base::BindOnce(&DiscountsPageActionController::HandleDiscountInfoResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DiscountsPageActionController::HandleDiscountInfoResponse(
    const DiscountsMap& discounts_map) {
  CHECK(discounts_map.empty() ||
        (discounts_map.size() == 1 &&
         discounts_map.begin()->first == last_committed_url_));

  if (discounts_map.empty() || discounts_map.begin()->second.empty()) {
    got_discounts_response_for_page_ = true;
    NotifyHost();
    return;
  }

  discounts_ = discounts_map.begin()->second;
  got_discounts_response_for_page_ = true;
  NotifyHost();
}

const std::vector<DiscountInfo>& DiscountsPageActionController::GetDiscounts() {
  return discounts_.value();
}

void DiscountsPageActionController::CouponCodeCopied() {
  coupon_code_copied_ = true;
}

bool DiscountsPageActionController::IsCouponCodeCopied() {
  bool coupon_code_copied = coupon_code_copied_;
  coupon_code_copied_ = false;
  return coupon_code_copied;
}

bool DiscountsPageActionController::ShouldAutoShowBubble(
    uint64_t discount_id,
    bool is_merchant_wide) {
  if (!shopping_service_ ||
      !shopping_service_->IsDiscountEligibleToShowOnNavigation()) {
    return false;
  }
  auto behavior = is_merchant_wide
                      ? static_cast<commerce::DiscountDialogAutoPopupBehavior>(
                            commerce::kMerchantWideBehavior.Get())
                      : static_cast<commerce::DiscountDialogAutoPopupBehavior>(
                            commerce::kNonMerchantWideBehavior.Get());

  switch (behavior) {
    case commerce::DiscountDialogAutoPopupBehavior::kAutoPopupOnce:
      if (shopping_service_->HasDiscountShownBefore(discount_id)) {
        return false;
      }
      shopping_service_->ShownDiscount(discount_id);
      return true;
    case commerce::DiscountDialogAutoPopupBehavior::kAlwaysAutoPopup:
      return true;
    case commerce::DiscountDialogAutoPopupBehavior::kNoAutoPopup:
      return false;
  }
}

}  // namespace commerce
