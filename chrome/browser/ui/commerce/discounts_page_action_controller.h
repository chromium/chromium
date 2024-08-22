// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_DISCOUNTS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_COMMERCE_DISCOUNTS_PAGE_ACTION_CONTROLLER_H_

#include "base/containers/flat_set.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ui/commerce/commerce_page_action_controller.h"
#include "components/commerce/core/commerce_types.h"

class GURL;

namespace commerce {

class ShoppingService;

class DiscountsPageActionController : public CommercePageActionController {
 public:
  static constexpr char kDiscountsShownDataKey[] =
      "commerce.discounts_shown_data";
  struct DiscountsShownData : public base::SupportsUserData::Data {
    base::flat_set<uint64_t> shown_discount_ids;
    base::flat_set<std::string> discount_shown_on_domains;

    DiscountsShownData();
    ~DiscountsShownData() override;
  };

  static DiscountsShownData* GetOrCreate(ShoppingService* shopping_service);

  DiscountsPageActionController(base::RepeatingCallback<void()> notify_callback,
                                ShoppingService* shopping_service);
  DiscountsPageActionController(const DiscountsPageActionController&) = delete;
  DiscountsPageActionController& operator=(
      const DiscountsPageActionController&) = delete;
  ~DiscountsPageActionController() override;

  // CommercePageActionController:
  std::optional<bool> ShouldShowForNavigation() override;
  bool WantsExpandedUi() override;
  void ResetForNewNavigation(const GURL& url) override;

  const std::vector<DiscountInfo>& GetDiscounts();
  void CouponCodeCopied();
  bool IsCouponCodeCopied();
  bool ShouldAutoShowBubble(uint64_t discount_id, bool is_merchant_wide);
  void DiscountsBubbleShown(uint64_t discount_id);

 private:
  void HandleDiscountInfoResponse(const GURL& url,
                                  const std::vector<DiscountInfo> discounts);
  // The shopping service is tied to the lifetime of the browser context
  // which will always outlive this tab helper.
  raw_ptr<ShoppingService> shopping_service_;
  GURL last_committed_url_;
  bool got_discounts_response_for_page_ = false;
  // The last discounts that were fetched for the last committed URL.
  std::optional<std::vector<DiscountInfo>> discounts_;
  bool coupon_code_copied_ = false;

  base::WeakPtrFactory<DiscountsPageActionController> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_COMMERCE_DISCOUNTS_PAGE_ACTION_CONTROLLER_H_
