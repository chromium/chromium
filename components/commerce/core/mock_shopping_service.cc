// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/mock_shopping_service.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"

namespace commerce {

// static
std::unique_ptr<KeyedService> MockShoppingService::Build() {
  return std::make_unique<MockShoppingService>();
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
                                nullptr) {
  // Set up some defaults so tests don't have to explicitly set up each.
  SetIsReady(true);
  SetResponseForGetProductInfoForUrl(absl::nullopt);
  SetResponsesForGetUpdatedProductInfoForBookmarks(
      std::map<base::Uuid, ProductInfo>());
  ON_CALL(*this, GetMaxProductBookmarkUpdatesPerBatch)
      .WillByDefault(testing::Return(30));
  SetResponseForGetMerchantInfoForUrl(absl::nullopt);
  SetResponseForIsShoppingPage(absl::nullopt);
  SetSubscribeCallbackValue(true);
  SetUnsubscribeCallbackValue(true);
  SetIsSubscribedCallbackValue(true);
  SetGetAllSubscriptionsCallbackValue(std::vector<CommerceSubscription>());
  SetIsShoppingListEligible(true);
  SetIsClusterIdTrackedByUserResponse(true);
  SetIsMerchantViewerEnabled(true);
  SetGetAllPriceTrackedBookmarksCallbackValue(
      std::vector<const bookmarks::BookmarkNode*>());
  SetGetAllShoppingBookmarksValue(
      std::vector<const bookmarks::BookmarkNode*>());
  SetIsPriceInsightsEligible(true);
  SetResponseForGetPriceInsightsInfoForUrl(absl::nullopt);
}

MockShoppingService::~MockShoppingService() = default;

void MockShoppingService::SetResponseForGetProductInfoForUrl(
    absl::optional<commerce::ProductInfo> product_info) {
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
    absl::optional<commerce::PriceInsightsInfo> price_insights_info) {
  ON_CALL(*this, GetPriceInsightsInfoForUrl)
      .WillByDefault(
          [price_insights_info](const GURL& url,
                                commerce::PriceInsightsInfoCallback callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), url, price_insights_info));
          });
}

void MockShoppingService::SetResponsesForGetUpdatedProductInfoForBookmarks(
    std::map<base::Uuid, ProductInfo> bookmark_updates) {
  ON_CALL(*this, GetUpdatedProductInfoForBookmarks)
      .WillByDefault(
          [bookmark_updates = std::move(bookmark_updates)](
              const std::vector<base::Uuid>& bookmark_uuids,
              BookmarkProductInfoUpdatedCallback info_updated_callback) {
            for (const auto& uuid : bookmark_uuids) {
              auto it = bookmark_updates.find(uuid);

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
    absl::optional<commerce::MerchantInfo> merchant_info) {
  ON_CALL(*this, GetMerchantInfoForUrl)
      .WillByDefault([merchant_info](const GURL& url,
                                     MerchantInfoCallback callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), url, merchant_info));
      });
}

void MockShoppingService::SetResponseForIsShoppingPage(
    absl::optional<bool> is_shopping_page) {
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

void MockShoppingService::SetIsClusterIdTrackedByUserResponse(bool is_tracked) {
  ON_CALL(*this, IsClusterIdTrackedByUser)
      .WillByDefault([is_tracked](uint64_t cluster_id,
                                  base::OnceCallback<void(bool)> callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), is_tracked));
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

void MockShoppingService::SetResponseForGetDiscountInfoForUrls(
    const DiscountsMap& discounts_map) {
  ON_CALL(*this, GetDiscountInfoForUrls)
      .WillByDefault([discounts_map](const std::vector<GURL>& urls,
                                     DiscountInfoCallback callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), discounts_map));
      });
}

}  // namespace commerce
