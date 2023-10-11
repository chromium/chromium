// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_MOCK_SHOPPING_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_MOCK_SHOPPING_SERVICE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace commerce {

// A mock ShoppingService that allows us to decide the response.
class MockShoppingService : public commerce::ShoppingService {
 public:
  static std::unique_ptr<KeyedService> Build();

  MockShoppingService();
  ~MockShoppingService() override;

  // commerce::ShoppingService overrides.
  MOCK_METHOD(void,
              GetProductInfoForUrl,
              (const GURL& url, commerce::ProductInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              GetPriceInsightsInfoForUrl,
              (const GURL& url, commerce::PriceInsightsInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              GetUpdatedProductInfoForBookmarks,
              (const std::vector<base::Uuid>& bookmark_uuids,
               BookmarkProductInfoUpdatedCallback info_updated_callback),
              (override));
  MOCK_METHOD(size_t, GetMaxProductBookmarkUpdatesPerBatch, (), (override));
  MOCK_METHOD(void,
              GetMerchantInfoForUrl,
              (const GURL& url, MerchantInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              IsShoppingPage,
              (const GURL& url, IsShoppingPageCallback callback),
              (override));
  MOCK_METHOD(absl::optional<ProductInfo>,
              GetAvailableProductInfoForUrl,
              (const GURL& url),
              (override));
  MOCK_METHOD(void,
              Subscribe,
              (std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
               base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void,
              Unsubscribe,
              (std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
               base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(
      void,
      GetAllSubscriptions,
      (SubscriptionType type,
       base::OnceCallback<void(std::vector<CommerceSubscription>)> callback),
      (override));
  MOCK_METHOD(void,
              IsSubscribed,
              (CommerceSubscription subscription,
               base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(bool,
              IsSubscribedFromCache,
              (const CommerceSubscription& subscription),
              (override));
  MOCK_METHOD(void,
              GetAllPriceTrackedBookmarks,
              (base::OnceCallback<
                  void(std::vector<const bookmarks::BookmarkNode*>)> callback),
              (override));
  MOCK_METHOD(std::vector<const bookmarks::BookmarkNode*>,
              GetAllShoppingBookmarks,
              (),
              (override));
  MOCK_METHOD(void, ScheduleSavedProductUpdate, (), (override));
  MOCK_METHOD(bool, IsShoppingListEligible, (), (override));
  MOCK_METHOD(void,
              WaitForReady,
              (base::OnceCallback<void(ShoppingService*)>),
              (override));
  MOCK_METHOD(void,
              IsClusterIdTrackedByUser,
              (uint64_t cluster_id, base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(bool, IsMerchantViewerEnabled, (), (override));
  MOCK_METHOD(bool, IsPriceInsightsEligible, (), (override));
  MOCK_METHOD(bool, IsDiscountEligibleToShowOnNavigation, (), (override));
  MOCK_METHOD(bool, IsParcelTrackingEligible, (), (override));
  MOCK_METHOD(void,
              GetDiscountInfoForUrls,
              (const std::vector<GURL>& urls, DiscountInfoCallback callback),
              (override));
  MOCK_METHOD(bookmarks::BookmarkModel*,
              GetBookmarkModelUsedForSync,
              (),
              (override));

  void SetResponseForGetProductInfoForUrl(
      absl::optional<commerce::ProductInfo> product_info);
  void SetResponseForGetPriceInsightsInfoForUrl(
      absl::optional<commerce::PriceInsightsInfo> price_insights_info);
  void SetResponsesForGetUpdatedProductInfoForBookmarks(
      std::map<base::Uuid, ProductInfo> bookmark_updates);
  void SetResponseForGetMerchantInfoForUrl(
      absl::optional<commerce::MerchantInfo> merchant_info);
  void SetResponseForIsShoppingPage(absl::optional<bool> is_shopping_page);
  void SetSubscribeCallbackValue(bool subscribe_should_succeed);
  void SetUnsubscribeCallbackValue(bool unsubscribe_should_succeed);
  void SetIsSubscribedCallbackValue(bool is_subscribed);
  void SetGetAllSubscriptionsCallbackValue(
      std::vector<CommerceSubscription> subscriptions);
  void SetIsShoppingListEligible(bool enabled);
  void SetIsReady(bool ready);
  void SetIsClusterIdTrackedByUserResponse(bool is_tracked);
  void SetIsMerchantViewerEnabled(bool is_enabled);
  void SetGetAllPriceTrackedBookmarksCallbackValue(
      std::vector<const bookmarks::BookmarkNode*> bookmarks);
  void SetGetAllShoppingBookmarksValue(
      std::vector<const bookmarks::BookmarkNode*> bookmarks);
  void SetIsPriceInsightsEligible(bool is_eligible);
  void SetIsDiscountEligibleToShowOnNavigation(bool is_eligible);
  void SetResponseForGetDiscountInfoForUrls(const DiscountsMap& discounts_map);
  void SetBookmarkModelUsedForSync(bookmarks::BookmarkModel* bookmark_model);
  void SetIsParcelTrackingEligible(bool is_eligible);

 private:
  // Since the discount API wants a const ref to some map, keep a default
  // instance here.
  DiscountsMap default_discounts_map_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_MOCK_SHOPPING_SERVICE_H_
