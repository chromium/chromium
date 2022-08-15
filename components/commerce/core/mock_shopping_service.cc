// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/mock_shopping_service.h"

namespace commerce {

MockShoppingService::MockShoppingService()
    : commerce::ShoppingService(nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr) {}

MockShoppingService::~MockShoppingService() = default;

void MockShoppingService::GetProductInfoForUrl(
    const GURL& url,
    commerce::ProductInfoCallback callback) {
  std::move(callback).Run(url, product_info_);
}

void MockShoppingService::SetResponseForGetProductInfoForUrl(
    absl::optional<commerce::ProductInfo> product_info) {
  product_info_ = product_info;
}

void MockShoppingService::GetMerchantInfoForUrl(const GURL& url,
                                                MerchantInfoCallback callback) {
  std::move(callback).Run(url, std::move(merchant_info_));
}

void MockShoppingService::SetResponseForGetMerchantInfoForUrl(
    absl::optional<commerce::MerchantInfo> merchant_info) {
  merchant_info_ = std::move(merchant_info);
}

void MockShoppingService::Subscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(subscribe_callback_value_);
}

void MockShoppingService::SetSubscribeCallbackValue(
    bool subscribe_should_succeed) {
  subscribe_callback_value_ = subscribe_should_succeed;
}

void MockShoppingService::Unsubscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(unsubscribe_callback_value_);
}

void MockShoppingService::SetUnsubscribeCallbackValue(
    bool unsubscribe_should_succeed) {
  unsubscribe_callback_value_ = unsubscribe_should_succeed;
}

}  // namespace commerce