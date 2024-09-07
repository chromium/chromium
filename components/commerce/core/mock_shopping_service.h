// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_MOCK_SHOPPING_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_MOCK_SHOPPING_SERVICE_H_

#include <map>
#include <memory>
#include <vector>

#include "components/commerce/core/compare/product_group.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace commerce {

class AccountChecker;
class MockClusterManager;
class MockProductSpecificationsService;

// A mock ShoppingService that allows us to decide the response.
class MockShoppingService : public commerce::ShoppingService {
 public:
  // Produces a testing::NiceMock of the MockShoppingService.
  static std::unique_ptr<KeyedService> Build();

  MockShoppingService();
  ~MockShoppingService() override;

  // commerce::ShoppingService overrides.
  MOCK_METHOD(AccountChecker*, GetAccountChecker, (), (override));
  MOCK_METHOD(void,
              GetProductInfoForUrl,
              (const GURL& url, commerce::ProductInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              GetPriceInsightsInfoForUrl,
              (const GURL& url, commerce::PriceInsightsInfoCallback callback),
              (override));
  MOCK_METHOD(const std::vector<commerce::UrlInfo>,
              GetUrlInfosForActiveWebWrappers,
              (),
              (override));
  MOCK_METHOD(
      void,
      GetUrlInfosForWebWrappersWithProducts,
      (base::OnceCallback<void(const std::vector<commerce::UrlInfo>)> callback),
      (override));
  MOCK_METHOD(const std::vector<commerce::UrlInfo>,
              GetUrlInfosForRecentlyViewedWebWrappers,
              (),
              (override));
  MOCK_METHOD(void,
              GetUpdatedProductInfoForBookmarks,
              (const std::vector<int64_t>& bookmark_ids,
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
  MOCK_METHOD(std::optional<ProductInfo>,
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
  MOCK_METHOD(bool, IsMerchantViewerEnabled, (), (override));
  MOCK_METHOD(bool, IsPriceInsightsEligible, (), (override));
  MOCK_METHOD(bool, IsDiscountEligibleToShowOnNavigation, (), (override));
  MOCK_METHOD(bool, IsParcelTrackingEligible, (), (override));
  MOCK_METHOD(void,
              GetDiscountInfoForUrl,
              (const GURL& url, DiscountInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              GetAllParcelStatuses,
              (GetParcelStatusCallback callback),
              (override));
  MOCK_METHOD(void,
              StopTrackingParcel,
              (const std::string& tracking_id,
               base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void,
              StopTrackingAllParcels,
              (base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void,
              GetProductSpecificationsForUrls,
              (const std::vector<GURL>& urls,
               ProductSpecificationsCallback callback),
              (override));
  MOCK_METHOD(ProductSpecificationsService*,
              GetProductSpecificationsService,
              (),
              (override));
  MOCK_METHOD(ClusterManager*, GetClusterManager, (), (override));
  MOCK_METHOD(void,
              QueryHistoryForUrl,
              (const GURL& url,
               history::HistoryService::QueryURLCallback callback),
              (override));

  // Make this mock permissive for all features but default to providing empty
  // data for all accessors of shopping data.
  void SetupPermissiveMock();

  void SetAccountChecker(AccountChecker* account_checker);
  void SetResponseForGetProductInfoForUrl(
      std::optional<commerce::ProductInfo> product_info);
  void SetResponseForGetPriceInsightsInfoForUrl(
      std::optional<commerce::PriceInsightsInfo> price_insights_info);
  void SetResponseForGetUrlInfosForActiveWebWrappers(
      std::vector<commerce::UrlInfo> url_infos);
  void SetResponsesForGetUpdatedProductInfoForBookmarks(
      std::map<int64_t, ProductInfo> bookmark_updates);
  void SetResponseForGetMerchantInfoForUrl(
      std::optional<commerce::MerchantInfo> merchant_info);
  void SetResponseForIsShoppingPage(std::optional<bool> is_shopping_page);
  void SetSubscribeCallbackValue(bool subscribe_should_succeed);
  void SetUnsubscribeCallbackValue(bool unsubscribe_should_succeed);
  void SetIsSubscribedCallbackValue(bool is_subscribed);
  void SetGetAllSubscriptionsCallbackValue(
      std::vector<CommerceSubscription> subscriptions);
  void SetIsShoppingListEligible(bool enabled);
  void SetIsReady(bool ready);
  void SetIsMerchantViewerEnabled(bool is_enabled);
  void SetGetAllPriceTrackedBookmarksCallbackValue(
      std::vector<const bookmarks::BookmarkNode*> bookmarks);
  void SetGetAllShoppingBookmarksValue(
      std::vector<const bookmarks::BookmarkNode*> bookmarks);
  void SetIsPriceInsightsEligible(bool is_eligible);
  void SetIsDiscountEligibleToShowOnNavigation(bool is_eligible);
  void SetResponseForGetDiscountInfoForUrl(
      const std::vector<DiscountInfo>& infos);
  void SetIsParcelTrackingEligible(bool is_eligible);
  void SetGetAllParcelStatusesCallbackValue(
      std::vector<ParcelTrackingStatus> parcels);
  void SetResponseForGetProductSpecificationsForUrls(
      ProductSpecifications specs);
  // TODO(b/362316113): Remove once history service is passed through handler
  // constructor.
  void SetQueryHistoryForUrlCallbackValue(history::QueryURLResult result);

 private:
  std::unique_ptr<MockProductSpecificationsService>
      product_specifications_service_;
  std::unique_ptr<MockClusterManager> cluster_manager_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_MOCK_SHOPPING_SERVICE_H_
