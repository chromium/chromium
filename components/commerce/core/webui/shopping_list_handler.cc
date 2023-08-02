// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/webui/shopping_list_handler.h"

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/payments/core/currency_formatter.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "url/gurl.h"

namespace commerce {
namespace {

shopping_list::mojom::ProductInfoPtr ProductInfoToMojoProduct(
    const GURL& url,
    const absl::optional<ProductInfo>& info,
    const std::string& locale) {
  auto product_info = shopping_list::mojom::ProductInfo::New();

  if (!info.has_value()) {
    return product_info;
  }

  product_info->title = info->title;
  product_info->cluster_title = info->product_cluster_title;
  product_info->domain = base::UTF16ToUTF8(
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          GURL(url)));
  product_info->product_url = url;
  product_info->image_url = info->image_url;
  if (info->product_cluster_id.has_value()) {
    product_info->cluster_id = info->product_cluster_id.value();
  }

  std::unique_ptr<payments::CurrencyFormatter> formatter =
      std::make_unique<payments::CurrencyFormatter>(info->currency_code,
                                                    locale);
  formatter->SetMaxFractionalDigits(2);

  product_info->current_price =
      base::UTF16ToUTF8(formatter->Format(base::NumberToString(
          static_cast<float>(info->amount_micros) / kToMicroCurrency)));

  // Only send the previous price if it is higher than the current price.
  if (info->previous_amount_micros.has_value() &&
      info->previous_amount_micros.value() > info->amount_micros) {
    product_info->previous_price =
        base::UTF16ToUTF8(formatter->Format(base::NumberToString(
            static_cast<float>(info->previous_amount_micros.value()) /
            kToMicroCurrency)));
  }

  return product_info;
}

shopping_list::mojom::BookmarkProductInfoPtr BookmarkNodeToMojoProduct(
    bookmarks::BookmarkModel& model,
    const bookmarks::BookmarkNode* node,
    const std::string& locale) {
  auto bookmark_info = shopping_list::mojom::BookmarkProductInfo::New();
  bookmark_info->bookmark_id = node->id();

  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(&model, node);
  const power_bookmarks::ShoppingSpecifics specifics =
      meta->shopping_specifics();

  bookmark_info->info = shopping_list::mojom::ProductInfo::New();
  bookmark_info->info->title = specifics.title();
  bookmark_info->info->domain = base::UTF16ToUTF8(
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          GURL(node->url())));

  bookmark_info->info->product_url = node->url();
  bookmark_info->info->image_url = GURL(meta->lead_image().url());
  bookmark_info->info->cluster_id = specifics.product_cluster_id();

  const power_bookmarks::ProductPrice price = specifics.current_price();
  std::string currency_code = price.currency_code();

  std::unique_ptr<payments::CurrencyFormatter> formatter =
      std::make_unique<payments::CurrencyFormatter>(currency_code, locale);
  formatter->SetMaxFractionalDigits(2);

  bookmark_info->info->current_price =
      base::UTF16ToUTF8(formatter->Format(base::NumberToString(
          static_cast<float>(price.amount_micros()) / kToMicroCurrency)));

  // Only send the previous price if it is higher than the current price. This
  // is exclusively used to decide whether to show the price drop chip in the
  // UI.
  if (specifics.has_previous_price() &&
      specifics.previous_price().amount_micros() >
          specifics.current_price().amount_micros()) {
    const power_bookmarks::ProductPrice previous_price =
        specifics.previous_price();
    bookmark_info->info->previous_price =
        base::UTF16ToUTF8(formatter->Format(base::NumberToString(
            static_cast<float>(previous_price.amount_micros()) /
            kToMicroCurrency)));
  }

  return bookmark_info;
}

shopping_list::mojom::PriceInsightsInfoPtr PriceInsightsInfoToMojoObject(
    const absl::optional<PriceInsightsInfo>& info,
    const std::string& locale) {
  auto insights_info = shopping_list::mojom::PriceInsightsInfo::New();

  if (!info.has_value()) {
    return insights_info;
  }

  insights_info->cluster_id = info->product_cluster_id.value();

  std::unique_ptr<payments::CurrencyFormatter> formatter =
      std::make_unique<payments::CurrencyFormatter>(info->currency_code,
                                                    locale);
  formatter->SetMaxFractionalDigits(2);

  if (info->typical_low_price_micros.has_value() &&
      info->typical_high_price_micros.has_value()) {
    insights_info->typical_low_price =
        base::UTF16ToUTF8(formatter->Format(base::NumberToString(
            static_cast<float>(info->typical_low_price_micros.value()) /
            kToMicroCurrency)));

    insights_info->typical_high_price =
        base::UTF16ToUTF8(formatter->Format(base::NumberToString(
            static_cast<float>(info->typical_high_price_micros.value()) /
            kToMicroCurrency)));
  }

  if (info->catalog_attributes.has_value()) {
    insights_info->catalog_attributes = info->catalog_attributes.value();
  }

  if (info->jackpot_url.has_value()) {
    insights_info->jackpot = info->jackpot_url.value();
  }

  shopping_list::mojom::PriceInsightsInfo::PriceBucket bucket;
  switch (info->price_bucket) {
    case PriceBucket::kUnknown:
      bucket = shopping_list::mojom::PriceInsightsInfo::PriceBucket::kUnknown;
      break;
    case PriceBucket::kLowPrice:
      bucket = shopping_list::mojom::PriceInsightsInfo::PriceBucket::kLow;
      break;
    case PriceBucket::kTypicalPrice:
      bucket = shopping_list::mojom::PriceInsightsInfo::PriceBucket::kTypical;
      break;
    case PriceBucket::kHighPrice:
      bucket = shopping_list::mojom::PriceInsightsInfo::PriceBucket::kHigh;
      break;
  }
  insights_info->bucket = bucket;

  insights_info->has_multiple_catalogs = info->has_multiple_catalogs;

  for (auto history_price : info->catalog_history_prices) {
    auto point = shopping_list::mojom::PricePoint::New();
    point->date = std::get<0>(history_price);

    auto price =
        static_cast<float>(std::get<1>(history_price)) / kToMicroCurrency;
    point->price = price;
    point->formatted_price =
        base::UTF16ToUTF8(formatter->Format(base::NumberToString(price)));
    insights_info->history.push_back(std::move(point));
  }

  insights_info->locale = locale;
  insights_info->currency_code = info->currency_code;

  return insights_info;
}

}  // namespace

using shopping_list::mojom::BookmarkProductInfo;
using shopping_list::mojom::BookmarkProductInfoPtr;

ShoppingListHandler::ShoppingListHandler(
    mojo::PendingRemote<shopping_list::mojom::Page> remote_page,
    mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandler> receiver,
    bookmarks::BookmarkModel* bookmark_model,
    ShoppingService* shopping_service,
    PrefService* prefs,
    feature_engagement::Tracker* tracker,
    const std::string& locale,
    std::unique_ptr<Delegate> delegate)
    : remote_page_(std::move(remote_page)),
      receiver_(this, std::move(receiver)),
      bookmark_model_(bookmark_model),
      shopping_service_(shopping_service),
      pref_service_(prefs),
      tracker_(tracker),
      locale_(locale),
      delegate_(std::move(delegate)) {
  scoped_observation_.Observe(shopping_service_);
  // It is safe to schedule updates and observe bookmarks. If the feature is
  // disabled, no new information will be fetched or provided to the frontend.
  shopping_service_->ScheduleSavedProductUpdate();
}

ShoppingListHandler::~ShoppingListHandler() = default;

void ShoppingListHandler::GetAllPriceTrackedBookmarkProductInfo(
    GetAllPriceTrackedBookmarkProductInfoCallback callback) {
  shopping_service_->WaitForReady(base::BindOnce(
      [](base::WeakPtr<ShoppingListHandler> handler,
         GetAllPriceTrackedBookmarkProductInfoCallback callback,
         ShoppingService* service) {
        if (!service || !service->IsShoppingListEligible() ||
            handler.WasInvalidated()) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback),
                                        std::vector<BookmarkProductInfoPtr>()));
          return;
        }

        service->GetAllPriceTrackedBookmarks(
            base::BindOnce(&ShoppingListHandler::OnFetchPriceTrackedBookmarks,
                           handler, std::move(callback)));
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingListHandler::OnFetchPriceTrackedBookmarks(
    GetAllPriceTrackedBookmarkProductInfoCallback callback,
    std::vector<const bookmarks::BookmarkNode*> bookmarks) {
  std::vector<BookmarkProductInfoPtr> info_list =
      BookmarkListToMojoList(*bookmark_model_, bookmarks, locale_);

  if (!info_list.empty()) {
    // Record usage for price tracking promo.
    tracker_->NotifyEvent("price_tracking_side_panel_shown");
  }

  std::move(callback).Run(std::move(info_list));
}

void ShoppingListHandler::GetAllShoppingBookmarkProductInfo(
    GetAllShoppingBookmarkProductInfoCallback callback) {
  shopping_service_->WaitForReady(base::BindOnce(
      [](base::WeakPtr<ShoppingListHandler> handler,
         GetAllShoppingBookmarkProductInfoCallback callback,
         ShoppingService* service) {
        if (!service || !service->IsShoppingListEligible() ||
            handler.WasInvalidated()) {
          std::move(callback).Run({});
          return;
        }

        std::vector<const bookmarks::BookmarkNode*> bookmarks =
            service->GetAllShoppingBookmarks();

        std::vector<BookmarkProductInfoPtr> info_list = BookmarkListToMojoList(
            *(handler->bookmark_model_), bookmarks, handler->locale_);

        std::move(callback).Run(std::move(info_list));
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingListHandler::TrackPriceForBookmark(int64_t bookmark_id) {
  commerce::SetPriceTrackingStateForBookmark(
      shopping_service_, bookmark_model_,
      bookmarks::GetBookmarkNodeByID(bookmark_model_, bookmark_id), true,
      base::BindOnce(&ShoppingListHandler::onPriceTrackResult,
                     weak_ptr_factory_.GetWeakPtr(), bookmark_id,
                     bookmark_model_, true));
}

void ShoppingListHandler::UntrackPriceForBookmark(int64_t bookmark_id) {
  commerce::SetPriceTrackingStateForBookmark(
      shopping_service_, bookmark_model_,
      bookmarks::GetBookmarkNodeByID(bookmark_model_, bookmark_id), false,
      base::BindOnce(&ShoppingListHandler::onPriceTrackResult,
                     weak_ptr_factory_.GetWeakPtr(), bookmark_id,
                     bookmark_model_, false));
}

void ShoppingListHandler::OnSubscribe(const CommerceSubscription& subscription,
                                      bool succeeded) {
  if (succeeded) {
    HandleSubscriptionChange(subscription, true);
  }
}

void ShoppingListHandler::OnUnsubscribe(
    const CommerceSubscription& subscription,
    bool succeeded) {
  if (succeeded) {
    HandleSubscriptionChange(subscription, false);
  }
}

void ShoppingListHandler::HandleSubscriptionChange(
    const CommerceSubscription& sub,
    bool is_tracking) {
  if (sub.id_type != IdentifierType::kProductClusterId) {
    return;
  }

  uint64_t cluster_id;
  if (!base::StringToUint64(sub.id, &cluster_id)) {
    return;
  }

  std::vector<const bookmarks::BookmarkNode*> bookmarks =
      GetBookmarksWithClusterId(bookmark_model_, cluster_id);
  // Special handling when the unsubscription is caused by bookmark deletion and
  // therefore the bookmark can no longer be retrieved.
  // TODO(crbug.com/1462668): Update mojo call to pass cluster ID and make
  // BookmarkProductInfo a nullable parameter.
  if (!bookmarks.size()) {
    auto bookmark_info = shopping_list::mojom::BookmarkProductInfo::New();
    bookmark_info->info = shopping_list::mojom::ProductInfo::New();
    bookmark_info->info->cluster_id = cluster_id;
    remote_page_->PriceUntrackedForBookmark(std::move(bookmark_info));
    return;
  }
  for (auto* node : bookmarks) {
    auto product = BookmarkNodeToMojoProduct(*bookmark_model_, node, locale_);
    if (is_tracking) {
      remote_page_->PriceTrackedForBookmark(std::move(product));
    } else {
      remote_page_->PriceUntrackedForBookmark(std::move(product));
    }
  }
}

std::vector<BookmarkProductInfoPtr> ShoppingListHandler::BookmarkListToMojoList(
    bookmarks::BookmarkModel& model,
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks,
    const std::string& locale) {
  std::vector<BookmarkProductInfoPtr> info_list;

  for (const bookmarks::BookmarkNode* node : bookmarks) {
    info_list.push_back(BookmarkNodeToMojoProduct(model, node, locale));
  }

  return info_list;
}

void ShoppingListHandler::onPriceTrackResult(int64_t bookmark_id,
                                             bookmarks::BookmarkModel* model,
                                             bool is_tracking,
                                             bool success) {
  if (success)
    return;

  // We only do work here if price tracking failed. When the UI is interacted
  // with, we assume success. In the event it failed, we switch things back.
  // So in this case, if we were trying to untrack and that action failed, set
  // the UI back to "tracking".
  auto* node = bookmarks::GetBookmarkNodeByID(bookmark_model_, bookmark_id);
  auto product = BookmarkNodeToMojoProduct(*bookmark_model_, node, locale_);

  if (!is_tracking) {
    remote_page_->PriceTrackedForBookmark(std::move(product));
  } else {
    remote_page_->PriceUntrackedForBookmark(std::move(product));
  }
  // Pass in whether the failed operation was to track or untrack price. It
  // should be the reverse of the current tracking status since the operation
  // failed.
  remote_page_->OperationFailedForBookmark(
      BookmarkNodeToMojoProduct(*bookmark_model_, node, locale_), is_tracking);
}

void ShoppingListHandler::GetProductInfoForCurrentUrl(
    GetProductInfoForCurrentUrlCallback callback) {
  if (!shopping_service_->IsPriceInsightsEligible() || !delegate_ ||
      !delegate_->GetCurrentTabUrl().has_value()) {
    std::move(callback).Run(shopping_list::mojom::ProductInfo::New());
    return;
  }

  shopping_service_->GetProductInfoForUrl(
      delegate_->GetCurrentTabUrl().value(),
      base::BindOnce(&ShoppingListHandler::OnFetchProductInfoForCurrentUrl,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingListHandler::IsShoppingListEligible(
    IsShoppingListEligibleCallback callback) {
  std::move(callback).Run(shopping_service_->IsShoppingListEligible());
}

void ShoppingListHandler::GetPriceTrackingStatusForCurrentUrl(
    GetPriceTrackingStatusForCurrentUrlCallback callback) {
  const GURL current_url = delegate_->GetCurrentTabUrl().value();
  const bookmarks::BookmarkNode* existing_node =
      bookmark_model_->GetMostRecentlyAddedUserNodeForURL(current_url);
  if (!existing_node) {
    std::move(callback).Run(false);
    return;
  }
  commerce::IsBookmarkPriceTracked(
      shopping_service_, bookmark_model_, existing_node,
      base::BindOnce(
          &ShoppingListHandler::OnGetPriceTrackingStatusForCurrentUrl,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingListHandler::SetPriceTrackingStatusForCurrentUrl(bool track) {
  const bookmarks::BookmarkNode* node =
      delegate_->GetOrAddBookmarkForCurrentUrl();
  if (track) {
    TrackPriceForBookmark(node->id());
  } else {
    UntrackPriceForBookmark(node->id());
  }
}

void ShoppingListHandler::GetParentBookmarkFolderNameForCurrentUrl(
    GetParentBookmarkFolderNameForCurrentUrlCallback callback) {
  const GURL current_url = delegate_->GetCurrentTabUrl().value();
  std::move(callback).Run(
      commerce::GetBookmarkParentNameOrDefault(bookmark_model_, current_url));
}

void ShoppingListHandler::ShowBookmarkEditorForCurrentUrl() {
  delegate_->ShowBookmarkEditorForCurrentUrl();
}

void ShoppingListHandler::OnFetchProductInfoForCurrentUrl(
    GetProductInfoForCurrentUrlCallback callback,
    const GURL& url,
    const absl::optional<ProductInfo>& info) {
  std::move(callback).Run(ProductInfoToMojoProduct(url, info, locale_));
}

void ShoppingListHandler::GetPriceInsightsInfoForCurrentUrl(
    GetPriceInsightsInfoForCurrentUrlCallback callback) {
  if (!shopping_service_->IsPriceInsightsEligible() || !delegate_ ||
      !delegate_->GetCurrentTabUrl().has_value()) {
    std::move(callback).Run(shopping_list::mojom::PriceInsightsInfo::New());
    return;
  }

  shopping_service_->GetPriceInsightsInfoForUrl(
      delegate_->GetCurrentTabUrl().value(),
      base::BindOnce(
          &ShoppingListHandler::OnFetchPriceInsightsInfoForCurrentUrl,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ShoppingListHandler::OnFetchPriceInsightsInfoForCurrentUrl(
    GetPriceInsightsInfoForCurrentUrlCallback callback,
    const GURL& url,
    const absl::optional<PriceInsightsInfo>& info) {
  std::move(callback).Run(PriceInsightsInfoToMojoObject(info, locale_));
}

void ShoppingListHandler::ShowInsightsSidePanelUI() {
  if (delegate_) {
    delegate_->ShowInsightsSidePanelUI();
  }
}

void ShoppingListHandler::OnGetPriceTrackingStatusForCurrentUrl(
    GetPriceTrackingStatusForCurrentUrlCallback callback,
    bool tracked) {
  std::move(callback).Run(tracked);
}

void ShoppingListHandler::OpenUrlInNewTab(const GURL& url) {
  if (delegate_) {
    delegate_->OpenUrlInNewTab(url);
  }
}

void ShoppingListHandler::ShowFeedback() {
  if (delegate_) {
    delegate_->ShowFeedback();
  }
}

}  // namespace commerce
