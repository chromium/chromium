// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/mock_shopping_service.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/commerce/core/mock_cluster_manager.h"
#include "components/commerce/core/product_specifications/mock_product_specifications_service.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"

namespace commerce {

// static
std::unique_ptr<KeyedService> MockShoppingService::Build() {
  return std::make_unique<testing::NiceMock<MockShoppingService>>();
}

MockShoppingService::MockShoppingService()
    : commerce::ShoppingService("us",
                                "en-us",
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr) {
  product_specifications_service_ =
      std::make_unique<testing::NiceMock<MockProductSpecificationsService>>();
  ON_CALL(*this, GetProductSpecificationsService)
      .WillByDefault(testing::Return(product_specifications_service_.get()));
  cluster_manager_ = std::make_unique<testing::NiceMock<MockClusterManager>>(
      product_specifications_service_.get());
  ON_CALL(*this, GetClusterManager)
      .WillByDefault(testing::Return(cluster_manager_.get()));
}

MockShoppingService::~MockShoppingService() = default;

void MockShoppingService::SetupPermissiveMock() {
  SetIsReady(true);
  SetResponseForGetProductInfoForUrl(std::nullopt);
  SetResponsesForGetUpdatedProductInfoForBookmarks(
      std::map<int64_t, ProductInfo>());
  ON_CALL(*this, GetMaxProductBookmarkUpdatesPerBatch)
      .WillByDefault(testing::Return(30));
  SetResponseForGetMerchantInfoForUrl(std::nullopt);
  SetResponseForIsShoppingPage(std::nullopt);
  SetResponseForGetDiscountInfoForUrl(std::vector<DiscountInfo>());
  SetSubscribeCallbackValue(true);
  SetUnsubscribeCallbackValue(true);
  SetIsSubscribedCallbackValue(true);
  SetGetAllSubscriptionsCallbackValue(std::vector<CommerceSubscription>());
  SetIsShoppingListEligible(true);
  SetIsMerchantViewerEnabled(true);
  SetGetAllPriceTrackedBookmarksCallbackValue(
      std::vector<const bookmarks::BookmarkNode*>());
  SetGetAllShoppingBookmarksValue(
      std::vector<const bookmarks::BookmarkNode*>());
  SetIsPriceInsightsEligible(true);
  SetResponseForGetPriceInsightsInfoForUrl(std::nullopt);
  SetGetAllParcelStatusesCallbackValue(std::vector<ParcelTrackingStatus>());
  SetQueryHistoryForUrlCallbackValue(history::QueryURLResult());
}

void MockShoppingService::SetAccountChecker(AccountChecker* account_checker) {
  ON_CALL(*this, GetAccountChecker)
      .WillByDefault(testing::Return(account_checker));
}

void MockShoppingService::SetResponseForGetProductInfoForUrl(
    std::optional<commerce::ProductInfo> product_info) {
  ON_CALL(*this, GetProductInfoForUrl)
      .WillByDefault([product_info](const GURL& url,
                                    commerce::ProductInfoCallback callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), url, product_info));
      });

  ON_CALL(*this, GetAvailableProductInfoForUrl)
      .WillByDefault(testing::Return(product_info));
}

void MockShoppingService::SetResponseForGetPriceInsightsInfoForUrl(
    std::optional<commerce::PriceInsightsInfo> price_insights_info) {
  ON_CALL(*this, GetPriceInsightsInfoForUrl)
      .WillByDefault(
          [price_insights_info](const GURL& url,
                                commerce::PriceInsightsInfoCallback callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), url, price_insights_info));
          });
}

void MockShoppingService::SetResponseForGetUrlInfosForActiveWebWrappers(
    std::vector<commerce::UrlInfo> url_infos) {
  ON_CALL(*this, GetUrlInfosForActiveWebWrappers)
      .WillByDefault(testing::Return(url_infos));
}

void MockShoppingService::SetResponsesForGetUpdatedProductInfoForBookmarks(
    std::map<int64_t, ProductInfo> bookmark_updates) {
  ON_CALL(*this, GetUpdatedProductInfoForBookmarks)
      .WillByDefault(
          [bookmark_updates = std::move(bookmark_updates)](
              const std::vector<int64_t>& bookmark_ids,
              BookmarkProductInfoUpdatedCallback info_updated_callback) {
            for (auto id : bookmark_ids) {
              auto it = bookmark_updates.find(id);

              if (it == bookmark_updates.end()) {
                continue;
              }

              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(info_updated_callback, it->first,
                                            GURL(""), it->second));
            }
          });
}

void MockShoppingService::SetResponseForGetMerchantInfoForUrl(
    std::optional<commerce::MerchantInfo> merchant_info) {
  ON_CALL(*this, GetMerchantInfoForUrl)
      .WillByDefault([merchant_info](const GURL& url,
                                     MerchantInfoCallback callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), url, merchant_info));
      });
}

void MockShoppingService::SetResponseForIsShoppingPage(
    std::optional<bool> is_shopping_page) {
  ON_CALL(*this, IsShoppingPage)
      .WillByDefault(
          [is_shopping_page](const GURL& url, IsShoppingPageCallback callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), url, is_shopping_page));
          });
}

void MockShoppingService::SetSubscribeCallbackValue(
    bool subscribe_should_succeed) {
  ON_CALL(*this, Subscribe)
      .WillByDefault(
          [subscribe_should_succeed](
              std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
              base::OnceCallback<void(bool)> callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), subscribe_should_succeed));
          });
}

void MockShoppingService::SetUnsubscribeCallbackValue(
    bool unsubscribe_should_succeed) {
  ON_CALL(*this, Unsubscribe)
      .WillByDefault(
          [unsubscribe_should_succeed](
              std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
              base::OnceCallback<void(bool)> callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback),
                                          unsubscribe_should_succeed));
          });
}

void MockShoppingService::SetIsSubscribedCallbackValue(bool is_subscribed) {
  ON_CALL(*this, IsSubscribed)
      .WillByDefault([is_subscribed](CommerceSubscription subscription,
                                     base::OnceCallback<void(bool)> callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), is_subscribed));
      });
  ON_CALL(*this, IsSubscribedFromCache)
      .WillByDefault(testing::Return(is_subscribed));
}

void MockShoppingService::SetGetAllSubscriptionsCallbackValue(
    std::vector<CommerceSubscription> subscriptions) {
  ON_CALL(*this, GetAllSubscriptions)
      .WillByDefault([subs = std::move(subscriptions)](
                         SubscriptionType type,
                         base::OnceCallback<void(
                             std::vector<CommerceSubscription>)> callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), std::move(subs)));
      });
}

void MockShoppingService::SetIsShoppingListEligible(bool eligible) {
  ON_CALL(*this, IsShoppingListEligible)
      .WillByDefault(testing::Return(eligible));
}

void MockShoppingService::SetIsReady(bool ready) {
  ON_CALL(*this, WaitForReady)
      .WillByDefault(
          [ready, this](base::OnceCallback<void(ShoppingService*)> callback) {
            if (ready) {
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(callback), this));
            }
          });
}

void MockShoppingService::SetIsMerchantViewerEnabled(bool is_enabled) {
  ON_CALL(*this, IsMerchantViewerEnabled)
      .WillByDefault(testing::Return(is_enabled));
}

void MockShoppingService::SetGetAllPriceTrackedBookmarksCallbackValue(
    std::vector<const bookmarks::BookmarkNode*> bookmarks) {
  ON_CALL(*this, GetAllPriceTrackedBookmarks)
      .WillByDefault(
          [bookmarks = std::move(bookmarks)](
              base::OnceCallback<void(
                  std::vector<const bookmarks::BookmarkNode*>)> callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), bookmarks));
          });
}

void MockShoppingService::SetGetAllShoppingBookmarksValue(
    std::vector<const bookmarks::BookmarkNode*> bookmarks) {
  ON_CALL(*this, GetAllShoppingBookmarks)
      .WillByDefault(testing::Return(bookmarks));
}

void MockShoppingService::SetIsPriceInsightsEligible(bool is_eligible) {
  ON_CALL(*this, IsPriceInsightsEligible)
      .WillByDefault(testing::Return(is_eligible));
}

void MockShoppingService::SetIsDiscountEligibleToShowOnNavigation(
    bool is_eligible) {
  ON_CALL(*this, IsDiscountEligibleToShowOnNavigation)
      .WillByDefault(testing::Return(is_eligible));
}

void MockShoppingService::SetResponseForGetDiscountInfoForUrl(
    const std::vector<DiscountInfo>& infos) {
  ON_CALL(*this, GetDiscountInfoForUrl)
      .WillByDefault([infos](const GURL& url, DiscountInfoCallback callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), url, infos));
      });
}

void MockShoppingService::SetIsParcelTrackingEligible(bool is_eligible) {
  ON_CALL(*this, IsParcelTrackingEligible)
      .WillByDefault(testing::Return(is_eligible));
}

void MockShoppingService::SetGetAllParcelStatusesCallbackValue(
    std::vector<ParcelTrackingStatus> parcels) {
  ON_CALL(*this, GetAllParcelStatuses)
      .WillByDefault(
          [parcels = std::move(parcels)](GetParcelStatusCallback callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    std::move(callback), true,
                    make_unique<std::vector<ParcelTrackingStatus>>(parcels)));
          });
}

void StopTrackingParcel(const std::string& tracking_id,
                        base::OnceCallback<void(bool)> callback) {}

void MockShoppingService::SetResponseForGetProductSpecificationsForUrls(
    ProductSpecifications specs) {
  ON_CALL(*this, GetProductSpecificationsForUrls)
      .WillByDefault(
          [specs = std::move(specs)](const std::vector<GURL>& urls,
                                     ProductSpecificationsCallback callback) {
            std::vector<uint64_t> ids{0};
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), std::move(ids),
                                          std::optional<ProductSpecifications>(
                                              std::move(specs))));
          });
}

void MockShoppingService::SetQueryHistoryForUrlCallbackValue(
    history::QueryURLResult result) {
  ON_CALL(*this, QueryHistoryForUrl)
      .WillByDefault([result = std::move(result)](
                         const GURL& url,
                         history::HistoryService::QueryURLCallback callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
      });
}

}  // namespace commerce
