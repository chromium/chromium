// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/price_tracking_utils.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/pref_service.h"

namespace commerce {

namespace {

// Update the bookmarks affected by the subscribe or unsubscribe event if it was
// successful.
void UpdateBookmarksForSubscriptionsResult(
    base::WeakPtr<bookmarks::BookmarkModel> model,
    base::OnceCallback<void(bool)> callback,
    bool enabled,
    uint64_t cluster_id,
    bool success) {
  if (success) {
    std::vector<const bookmarks::BookmarkNode*> results;
    power_bookmarks::PowerBookmarkQueryFields query;
    query.type = power_bookmarks::PowerBookmarkType::SHOPPING;
    power_bookmarks::GetBookmarksMatchingProperties(model.get(), query, -1,
                                                    &results);

    for (const auto* node : results) {
      std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
          power_bookmarks::GetNodePowerBookmarkMeta(model.get(), node);

      if (!meta)
        continue;

      power_bookmarks::ShoppingSpecifics* specifics =
          meta->mutable_shopping_specifics();

      if (!specifics || specifics->product_cluster_id() != cluster_id)
        continue;

      specifics->set_is_price_tracked(enabled);

      power_bookmarks::SetNodePowerBookmarkMeta(model.get(), node,
                                                std::move(meta));
    }
  }

  std::move(callback).Run(success);
}

}  // namespace

bool IsBookmarkPriceTracked(bookmarks::BookmarkModel* model,
                            const bookmarks::BookmarkNode* node) {
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(model, node);

  return meta && meta->has_shopping_specifics() &&
         meta->shopping_specifics().is_price_tracked();
}

bool IsProductBookmark(bookmarks::BookmarkModel* model,
                       const bookmarks::BookmarkNode* node) {
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(model, node);
  return meta && meta->has_shopping_specifics();
}

void SetPriceTrackingStateForClusterId(
    ShoppingService* service,
    bookmarks::BookmarkModel* model,
    const uint64_t cluster_id,
    bool enabled,
    base::OnceCallback<void(bool)> callback) {
  if (!service || !model)
    return;

  std::vector<const bookmarks::BookmarkNode*> product_bookmarks =
      GetBookmarksWithClusterId(model, cluster_id, 1);
  if (product_bookmarks.size() > 0) {
    SetPriceTrackingStateForBookmark(service, model, product_bookmarks[0],
                                     enabled, std::move(callback));
  }
}

void SetPriceTrackingStateForBookmark(ShoppingService* service,
                                      bookmarks::BookmarkModel* model,
                                      const bookmarks::BookmarkNode* node,
                                      bool enabled,
                                      base::OnceCallback<void(bool)> callback) {
  if (!service || !model || !node)
    return;

  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(model, node);

  // If there's no existing meta, check the shopping service. Bookmarks added
  // prior to making shopping meta available should still be trackable upon
  // revisiting the page. This logic is here since it's the result of a direct
  // user action, we don't yet want to passively update "normal" bookmarks.
  if (!meta || !meta->has_shopping_specifics()) {
    absl::optional<ProductInfo> info =
        service->GetAvailableProductInfoForUrl(node->url());

    // If still no information, do nothing.
    if (!info.has_value())
      return;

    std::unique_ptr<power_bookmarks::PowerBookmarkMeta> newMeta =
        std::make_unique<power_bookmarks::PowerBookmarkMeta>();
    bool changed =
        PopulateOrUpdateBookmarkMetaIfNeeded(newMeta.get(), info.value());
    CHECK(changed);

    // Make sure the data is attached to the bookmark and get a copy to use in
    // the rest of this function.
    power_bookmarks::SetNodePowerBookmarkMeta(model, node, std::move(newMeta));
    meta = power_bookmarks::GetNodePowerBookmarkMeta(model, node);
  }

  power_bookmarks::ShoppingSpecifics* specifics =
      meta->mutable_shopping_specifics();

  if (!specifics || !specifics->has_product_cluster_id())
    return;

  std::unique_ptr<std::vector<CommerceSubscription>> subs =
      std::make_unique<std::vector<CommerceSubscription>>();

  absl::optional<UserSeenOffer> user_seen_offer = absl::nullopt;
  if (enabled) {
    user_seen_offer.emplace(base::NumberToString(specifics->offer_id()),
                            specifics->current_price().amount_micros(),
                            specifics->country_code());
  }
  CommerceSubscription sub(
      SubscriptionType::kPriceTrack, IdentifierType::kProductClusterId,
      base::NumberToString(specifics->product_cluster_id()),
      ManagementType::kUserManaged, kUnknownSubscriptionTimestamp,
      std::move(user_seen_offer));

  subs->push_back(std::move(sub));

  base::OnceCallback<void(bool)> update_bookmarks_callback = base::BindOnce(
      &UpdateBookmarksForSubscriptionsResult, model->AsWeakPtr(),
      std::move(callback), enabled, specifics->product_cluster_id());

  if (enabled) {
    service->Subscribe(std::move(subs), std::move(update_bookmarks_callback));
  } else {
    service->Unsubscribe(std::move(subs), std::move(update_bookmarks_callback));
  }
}

std::vector<const bookmarks::BookmarkNode*> GetBookmarksWithClusterId(
    bookmarks::BookmarkModel* model,
    uint64_t cluster_id,
    size_t max_count) {
  std::vector<const bookmarks::BookmarkNode*> results =
      GetAllShoppingBookmarks(model);

  std::vector<const bookmarks::BookmarkNode*> bookmarks_with_cluster;
  for (const auto* node : results) {
    std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
        power_bookmarks::GetNodePowerBookmarkMeta(model, node);

    if (!meta)
      continue;

    power_bookmarks::ShoppingSpecifics* specifics =
        meta->mutable_shopping_specifics();

    if (!specifics || specifics->product_cluster_id() != cluster_id)
      continue;

    bookmarks_with_cluster.push_back(node);

    // If we're only looking for a fixed number of bookmarks, stop if we reach
    // that number.
    if (max_count > 0 && bookmarks_with_cluster.size() >= max_count)
      break;
  }

  return bookmarks_with_cluster;
}

std::vector<const bookmarks::BookmarkNode*> GetAllPriceTrackedBookmarks(
    bookmarks::BookmarkModel* model) {
  std::vector<const bookmarks::BookmarkNode*> results =
      GetAllShoppingBookmarks(model);

  std::vector<const bookmarks::BookmarkNode*> bookmarks_with_cluster;
  for (const auto* node : results) {
    std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
        power_bookmarks::GetNodePowerBookmarkMeta(model, node);

    if (!meta)
      continue;

    const power_bookmarks::ShoppingSpecifics specifics =
        meta->shopping_specifics();

    if (!specifics.is_price_tracked())
      continue;

    bookmarks_with_cluster.push_back(node);
  }

  return bookmarks_with_cluster;
}

std::vector<const bookmarks::BookmarkNode*> GetAllShoppingBookmarks(
    bookmarks::BookmarkModel* model) {
  CHECK(model);

  std::vector<const bookmarks::BookmarkNode*> results;
  power_bookmarks::PowerBookmarkQueryFields query;
  query.type = power_bookmarks::PowerBookmarkType::SHOPPING;
  power_bookmarks::GetBookmarksMatchingProperties(model, query, -1, &results);

  return results;
}

bool PopulateOrUpdateBookmarkMetaIfNeeded(
    power_bookmarks::PowerBookmarkMeta* out_meta,
    const ProductInfo& info) {
  bool changed = false;

  if (out_meta->lead_image().url() != info.image_url.spec()) {
    out_meta->mutable_lead_image()->set_url(info.image_url.spec());
    changed = true;
  }

  power_bookmarks::ShoppingSpecifics* specifics =
      out_meta->mutable_shopping_specifics();

  if (!info.title.empty() && specifics->title() != info.title) {
    specifics->set_title(info.title);
    changed = true;
  }

  if (specifics->country_code() != info.country_code) {
    specifics->set_country_code(info.country_code);
    changed = true;
  }

  if (specifics->current_price().currency_code() != info.currency_code ||
      specifics->current_price().amount_micros() != info.amount_micros) {
    specifics->mutable_current_price()->set_currency_code(info.currency_code);
    specifics->mutable_current_price()->set_amount_micros(info.amount_micros);
    changed = true;
  }

  if (info.previous_amount_micros.has_value() &&
      (specifics->previous_price().currency_code() != info.currency_code ||
       specifics->previous_price().amount_micros() !=
           info.previous_amount_micros.value())) {
    // Intentionally use the same currency code for both current and previous
    // price. Consistency between the values is guaranteed by the shopping
    // service.
    specifics->mutable_previous_price()->set_currency_code(info.currency_code);
    specifics->mutable_previous_price()->set_amount_micros(
        info.previous_amount_micros.value());
    changed = true;
  }

  if (specifics->offer_id() != info.offer_id) {
    specifics->set_offer_id(info.offer_id);
    changed = true;
  }

  // Only update the cluster ID if it was previously empty. Having this value
  // change would cause serious problems elsewhere.
  if (!specifics->has_product_cluster_id()) {
    specifics->set_product_cluster_id(info.product_cluster_id);
    changed = true;
  }
  // Consider adding a DCHECK for old and new cluster ID equality in the else
  // clause for the above.

  return changed;
}

void MaybeEnableEmailNotifications(PrefService* pref_service) {
  if (pref_service) {
    const PrefService::Preference* email_pref =
        pref_service->FindPreference(kPriceEmailNotificationsEnabled);
    if (email_pref && email_pref->IsDefaultValue()) {
      pref_service->SetBoolean(kPriceEmailNotificationsEnabled, true);
    }
  }
}

bool IsEmailDisabledByUser(PrefService* pref_service) {
  if (pref_service) {
    const PrefService::Preference* email_pref =
        pref_service->FindPreference(kPriceEmailNotificationsEnabled);
    if (email_pref && !email_pref->IsDefaultValue() &&
        !email_pref->GetValue()->GetBool()) {
      return true;
    }
  }
  return false;
}

}  // namespace commerce
