// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/mock_shopping_service.h"

#include "base/callback.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace commerce {

// static
std::unique_ptr<KeyedService> MockShoppingService::Build() {
  return std::make_unique<MockShoppingService>();
}

MockShoppingService::MockShoppingService()
    : commerce::ShoppingService(nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr) {}

MockShoppingService::~MockShoppingService() = default;

void MockShoppingService::GetProductInfoForUrl(
    const GURL& url,
    commerce::ProductInfoCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), url, product_info_));
}

void MockShoppingService::GetUpdatedProductInfoForBookmarks(
    const std::vector<int64_t>& bookmark_ids,
    BookmarkProductInfoUpdatedCallback info_updated_callback) {
  for (auto id : bookmark_ids) {
    auto it = bookmark_updates_map_.find(id);

    if (it == bookmark_updates_map_.end())
      continue;

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(info_updated_callback, it->first, GURL(""), it->second));
  }
}

absl::optional<ProductInfo> MockShoppingService::GetAvailableProductInfoForUrl(
    const GURL& url) {
  return product_info_;
}

void MockShoppingService::SetResponseForGetProductInfoForUrl(
    absl::optional<commerce::ProductInfo> product_info) {
  product_info_ = product_info;
}

void MockShoppingService::SetResponsesForGetUpdatedProductInfoForBookmarks(
    std::map<int64_t, ProductInfo> bookmark_updates) {
  bookmark_updates_map_ = bookmark_updates;
}

void MockShoppingService::GetMerchantInfoForUrl(const GURL& url,
                                                MerchantInfoCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), url, std::move(merchant_info_)));
}

void MockShoppingService::SetResponseForGetMerchantInfoForUrl(
    absl::optional<commerce::MerchantInfo> merchant_info) {
  merchant_info_ = std::move(merchant_info);
}

void MockShoppingService::Subscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), subscribe_callback_value_));
}

void MockShoppingService::SetSubscribeCallbackValue(
    bool subscribe_should_succeed) {
  subscribe_callback_value_ = subscribe_should_succeed;
}

void MockShoppingService::Unsubscribe(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    base::OnceCallback<void(bool)> callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), unsubscribe_callback_value_));
}

void MockShoppingService::SetUnsubscribeCallbackValue(
    bool unsubscribe_should_succeed) {
  unsubscribe_callback_value_ = unsubscribe_should_succeed;
}

void MockShoppingService::ScheduleSavedProductUpdate() {}

bool MockShoppingService::IsShoppingListEligible() {
  return is_shopping_list_eligible_;
}

void MockShoppingService::SetIsShoppingListEligible(bool eligible) {
  is_shopping_list_eligible_ = eligible;
}

}  // namespace commerce