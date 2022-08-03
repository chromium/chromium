// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/mock_shopping_service.h"

namespace commerce {

MockShoppingService::MockShoppingService()
    : commerce::ShoppingService(nullptr, nullptr, nullptr, nullptr, nullptr) {}

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

}  // namespace commerce