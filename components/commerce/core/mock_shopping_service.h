// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_MOCK_SHOPPING_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_MOCK_SHOPPING_SERVICE_H_

#include <memory>

#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace commerce {

// A mock ShoppingService that allows us to decide the response.
class MockShoppingService : public commerce::ShoppingService {
 public:
  MockShoppingService();
  ~MockShoppingService() override;

  // commerce::ShoppingService overrides.
  void GetProductInfoForUrl(const GURL& url,
                            commerce::ProductInfoCallback callback) override;
  void GetMerchantInfoForUrl(const GURL& url,
                             MerchantInfoCallback callback) override;
  void Subscribe(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      base::OnceCallback<void(bool)> callback) override;
  void Unsubscribe(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      base::OnceCallback<void(bool)> callback) override;

  void SetResponseForGetProductInfoForUrl(
      absl::optional<commerce::ProductInfo> product_info);
  void SetResponseForGetMerchantInfoForUrl(
      absl::optional<commerce::MerchantInfo> merchant_info);
  void SetSubscribeCallbackValue(bool subscribe_should_succeed);
  void SetUnsubscribeCallbackValue(bool unsubscribe_should_succeed);

 private:
  absl::optional<commerce::ProductInfo> product_info_;
  absl::optional<commerce::MerchantInfo> merchant_info_;
  bool subscribe_callback_value_{true};
  bool unsubscribe_callback_value_{true};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_MOCK_SHOPPING_SERVICE_H_
