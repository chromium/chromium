// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/webui/shopping_list_handler.h"

#include <memory>
#include <vector>

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
    const std::string& locale)
    : remote_page_(std::move(remote_page)),
      receiver_(this, std::move(receiver)),
      bookmark_model_(bookmark_model),
      shopping_service_(shopping_service),
      pref_service_(prefs),
      tracker_(tracker),
      locale_(locale) {
  scoped_observation_.Observe(shopping_service_);
  // It is safe to schedule updates and observe bookmarks. If the feature is
  // disabled, no new information will be fetched or provided to the frontend.
  shopping_service_->ScheduleSavedProductUpdate();
}

ShoppingListHandler::~ShoppingListHandler() = default;

void ShoppingListHandler::GetAllPriceTrackedBookmarkProductInfo(
    GetAllPriceTrackedBookmarkProductInfoCallback callback) {
  if (!shopping_service_->IsShoppingListEligible()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::vector<BookmarkProductInfoPtr>()));
    return;
  }
  GetAllPriceTrackedBookmarks(
      shopping_service_, bookmark_model_,
      base::BindOnce(&ShoppingListHandler::OnFetchPriceTrackedBookmarks,
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
  if (!shopping_service_->IsShoppingListEligible()) {
    std::move(callback).Run({});
    return;
  }
  std::vector<const bookmarks::BookmarkNode*> bookmarks =
      GetAllShoppingBookmarks(bookmark_model_);

  std::vector<BookmarkProductInfoPtr> info_list =
      BookmarkListToMojoList(*bookmark_model_, bookmarks, locale_);

  std::move(callback).Run(std::move(info_list));
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

void ShoppingListHandler::OnSubscribe(
    const std::vector<CommerceSubscription>& subscriptions,
    bool succeeded) {
  if (succeeded) {
    HandleSubscriptionChange(subscriptions, true);
  }
}

void ShoppingListHandler::OnUnsubscribe(
    const std::vector<CommerceSubscription>& subscriptions,
    bool succeeded) {
  if (succeeded) {
    HandleSubscriptionChange(subscriptions, false);
  }
}

void ShoppingListHandler::HandleSubscriptionChange(
    const std::vector<CommerceSubscription>& subscriptions,
    bool is_tracking) {
  for (auto& sub : subscriptions) {
    if (sub.id_type != IdentifierType::kProductClusterId) {
      continue;
    }

    uint64_t cluster_id;
    if (!base::StringToUint64(sub.id, &cluster_id)) {
      continue;
    }

    std::vector<const bookmarks::BookmarkNode*> bookmarks =
        GetBookmarksWithClusterId(bookmark_model_, cluster_id);
    for (auto* node : bookmarks) {
      if (is_tracking) {
        remote_page_->PriceTrackedForBookmark(
            BookmarkNodeToMojoProduct(*bookmark_model_, node, locale_));
      } else {
        remote_page_->PriceUntrackedForBookmark(node->id());
      }
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
  if (!is_tracking) {
    auto* node = bookmarks::GetBookmarkNodeByID(bookmark_model_, bookmark_id);
    remote_page_->PriceTrackedForBookmark(
        BookmarkNodeToMojoProduct(*model, node, locale_));
  } else {
    remote_page_->PriceUntrackedForBookmark(bookmark_id);
  }
  // Pass in whether the failed operation was to track or untrack price. It
  // should be the reverse of the current tracking status since the operation
  // failed.
  remote_page_->OperationFailedForBookmark(bookmark_id, is_tracking);
}
}  // namespace commerce
