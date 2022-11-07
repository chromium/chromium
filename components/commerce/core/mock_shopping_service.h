// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_MOCK_SHOPPING_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_MOCK_SHOPPING_SERVICE_H_

#include <map>
#include <memory>

#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace commerce {

// A mock ShoppingService that allows us to decide the response.
class MockShoppingService : public commerce::ShoppingService {
 public:
  static std::unique_ptr<KeyedService> Build();

  MockShoppingService();
  ~MockShoppingService() override;

  // commerce::ShoppingService overrides.
  void GetProductInfoForUrl(const GURL& url,
                            commerce::ProductInfoCallback callback) override;
  void GetUpdatedProductInfoForBookmarks(
      const std::vector<int64_t>& bookmark_ids,
      BookmarkProductInfoUpdatedCallback info_updated_callback) override;
  void GetMerchantInfoForUrl(const GURL& url,
                             MerchantInfoCallback callback) override;
  absl::optional<ProductInfo> GetAvailableProductInfoForUrl(
      const GURL& url) override;
  void Subscribe(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      base::OnceCallback<void(bool)> callback) override;
  void Unsubscribe(
      std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
      base::OnceCallback<void(bool)> callback) override;
  void ScheduleSavedProductUpdate() override;
  bool IsShoppingListEligible() override;
  void IsClusterIdTrackedByUser(
      uint64_t cluster_id,
      base::OnceCallback<void(bool)> callback) override;

  void SetResponseForGetProductInfoForUrl(
      absl::optional<commerce::ProductInfo> product_info);
  void SetResponsesForGetUpdatedProductInfoForBookmarks(
      std::map<int64_t, ProductInfo> bookmark_updates);
  void SetResponseForGetMerchantInfoForUrl(
      absl::optional<commerce::MerchantInfo> merchant_info);
  void SetSubscribeCallbackValue(bool subscribe_should_succeed);
  void SetUnsubscribeCallbackValue(bool unsubscribe_should_succeed);
  void SetIsShoppingListEligible(bool enabled);
  void SetIsClusterIdTrackedByUserResponse(bool is_tracked);

 private:
  absl::optional<commerce::ProductInfo> product_info_;
  std::map<int64_t, ProductInfo> bookmark_updates_map_;
  absl::optional<commerce::MerchantInfo> merchant_info_;
  bool subscribe_callback_value_{true};
  bool unsubscribe_callback_value_{true};
  bool is_shopping_list_eligible_{true};
  bool is_cluster_id_tracked_{true};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_MOCK_SHOPPING_SERVICE_H_
