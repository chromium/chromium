// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/discounts_page_action_controller.h"

#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/shopping_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace commerce {
DiscountsPageActionController::DiscountsShownData::DiscountsShownData() =
    default;
DiscountsPageActionController::DiscountsShownData::~DiscountsShownData() =
    default;

DiscountsPageActionController::DiscountsPageActionController(
    base::RepeatingCallback<void()> notify_callback,
    ShoppingService* shopping_service)
    : CommercePageActionController(std::move(notify_callback)),
      shopping_service_(shopping_service) {}

DiscountsPageActionController::~DiscountsPageActionController() = default;

// static
DiscountsPageActionController::DiscountsShownData*
DiscountsPageActionController::GetOrCreate(ShoppingService* shopping_service) {
  DiscountsShownData* data =
      static_cast<DiscountsShownData*>(shopping_service->GetUserData(
          DiscountsPageActionController::kDiscountsShownDataKey));
  if (!data) {
    auto discounts_shown_data = std::make_unique<DiscountsShownData>();
    data = discounts_shown_data.get();
    shopping_service->SetUserData(
        DiscountsPageActionController::kDiscountsShownDataKey,
        std::move(discounts_shown_data));
  }

  return data;
}

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
  if (!got_discounts_response_for_page_ || !discounts_.has_value() ||
      discounts_.value().empty()) {
    return false;
  }

  if (!commerce::kDiscountOnShoppyPage.Get()) {
    return true;
  }

  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      last_committed_url_,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  DiscountsShownData* shown_data =
      DiscountsPageActionController::GetOrCreate(shopping_service_);

  bool has_been_shown_on_domain =
      shown_data->discount_shown_on_domains.contains(domain);

  if (!has_been_shown_on_domain) {
    shown_data->discount_shown_on_domains.insert(domain);
    return true;
  }

  for (const auto& discount_info : discounts_.value()) {
    if (ShouldAutoShowBubble(discount_info.id,
                             discount_info.is_merchant_wide)) {
      return true;
    }
  }

  return false;
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

  shopping_service_->GetDiscountInfoForUrl(
      url,
      base::BindOnce(&DiscountsPageActionController::HandleDiscountInfoResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DiscountsPageActionController::HandleDiscountInfoResponse(
    const GURL& url,
    const std::vector<DiscountInfo> discounts) {
  if (url != last_committed_url_ || discounts.empty()) {
    got_discounts_response_for_page_ = true;
    NotifyHost();
    return;
  }

  discounts_ = std::move(discounts);
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
    case commerce::DiscountDialogAutoPopupBehavior::kAutoPopupOnce: {
      DiscountsShownData* shown_data =
          DiscountsPageActionController::GetOrCreate(shopping_service_);
      if (shown_data->shown_discount_ids.contains(discount_id)) {
        return false;
      }
      return true;
    }
    case commerce::DiscountDialogAutoPopupBehavior::kAlwaysAutoPopup:
      return true;
    case commerce::DiscountDialogAutoPopupBehavior::kNoAutoPopup:
      return false;
  }
}

void DiscountsPageActionController::DiscountsBubbleShown(uint64_t discount_id) {
  DiscountsShownData* shown_data =
      DiscountsPageActionController::GetOrCreate(shopping_service_);
  shown_data->shown_discount_ids.insert(discount_id);
}

}  // namespace commerce
