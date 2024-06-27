// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/price_tracking_utils.h"

#include <memory>
#include <unordered_set>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

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
    power_bookmarks::PowerBookmarkQueryFields query;
    query.type = power_bookmarks::PowerBookmarkType::SHOPPING;
    std::vector<const bookmarks::BookmarkNode*> results =
        power_bookmarks::GetBookmarksMatchingProperties(model.get(), query, -1);

    for (const bookmarks::BookmarkNode* node : results) {
      CHECK(node);
      if (model->IsLocalOnlyNode(*node)) {
        continue;
      }

      std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
          power_bookmarks::GetNodePowerBookmarkMeta(model.get(), node);

      if (!meta)
        continue;

      power_bookmarks::ShoppingSpecifics* specifics =
          meta->mutable_shopping_specifics();

      if (!specifics || specifics->product_cluster_id() != cluster_id)
        continue;

      // TODO(b:273526228): Once crrev.com/c/4278641 reaches stable, remove this
      //                    call -- shopping specifics no longer tracks
      //                    subscription state.
      specifics->set_is_price_tracked(enabled);

      // Always use the Windows epoch to keep consistency. This also align with
      // how we set the time fields in the bookmark_specifics.proto and in the
      // subscriptions_manager.cc.
      specifics->set_last_subscription_change_time(
          base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

      power_bookmarks::SetNodePowerBookmarkMeta(model.get(), node,
                                                std::move(meta));
    }
  }

  std::move(callback).Run(success);
}

}  // namespace

void IsBookmarkPriceTracked(ShoppingService* service,
                            bookmarks::BookmarkModel* model,
                            const bookmarks::BookmarkNode* node,
                            base::OnceCallback<void(bool)> callback) {
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(model, node);

  if (!meta || !meta->has_shopping_specifics() ||
      !meta->shopping_specifics().has_product_cluster_id()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  CommerceSubscription sub(
      SubscriptionType::kPriceTrack, IdentifierType::kProductClusterId,
      base::NumberToString(meta->shopping_specifics().product_cluster_id()),
      ManagementType::kUserManaged);

  service->IsSubscribed(sub, std::move(callback));
}

bool IsProductBookmark(bookmarks::BookmarkModel* model,
                       const bookmarks::BookmarkNode* node) {
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(model, node);
  return meta && meta->has_shopping_specifics();
}

std::optional<int64_t> GetBookmarkLastSubscriptionChangeTime(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(model, node);

  if (!meta || !meta->has_shopping_specifics() ||
      !meta->shopping_specifics().has_last_subscription_change_time()) {
    return std::nullopt;
  }
  return std::make_optional<int64_t>(
      meta->shopping_specifics().last_subscription_change_time());
}

void SetPriceTrackingStateForClusterId(
    ShoppingService* service,
    bookmarks::BookmarkModel* model,
    const uint64_t cluster_id,
    bool enabled,
    base::OnceCallback<void(bool)> callback) {
  if (!service || !model) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  std::vector<const bookmarks::BookmarkNode*> product_bookmarks =
      GetBookmarksWithClusterId(model, cluster_id, 1);
  if (product_bookmarks.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
    SetPriceTrackingStateForBookmark(service, model, product_bookmarks[0],
                                     enabled, std::move(callback));
}

void SetPriceTrackingStateForBookmark(
    ShoppingService* service,
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node,
    bool enabled,
    base::OnceCallback<void(bool)> callback,
    bool was_bookmark_created_by_price_tracking) {
  if (!service || !model || !node || model->IsLocalOnlyNode(*node)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(model, node);

  // If there's no existing meta, check the shopping service. Bookmarks added
  // prior to making shopping meta available should still be trackable upon
  // revisiting the page. This logic is here since it's the result of a direct
  // user action, we don't yet want to passively update "normal" bookmarks.
  if (!meta || !meta->has_shopping_specifics()) {
    std::optional<ProductInfo> info =
        service->GetAvailableProductInfoForUrl(node->url());

    // If still no information, do nothing.
    if (!info.has_value()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), false));
      return;
    }

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

  if (!specifics || !specifics->has_product_cluster_id()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  std::unique_ptr<std::vector<CommerceSubscription>> subs =
      std::make_unique<std::vector<CommerceSubscription>>();

  std::optional<UserSeenOffer> user_seen_offer = std::nullopt;
  if (enabled) {
    user_seen_offer.emplace(base::NumberToString(specifics->offer_id()),
                            specifics->current_price().amount_micros(),
                            specifics->country_code(), specifics->locale());
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
    // If the bookmark was created through the price tracking flow, make sure
    // that is recorded. If untracked, this bookmark will be deleted.
    if (was_bookmark_created_by_price_tracking) {
      specifics->set_bookmark_created_by_price_tracking(true);
      power_bookmarks::SetNodePowerBookmarkMeta(model, node, std::move(meta));
    }

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

void GetAllPriceTrackedBookmarks(
    ShoppingService* shopping_service,
    bookmarks::BookmarkModel* bookmark_model,
    base::OnceCallback<void(std::vector<const bookmarks::BookmarkNode*>)>
        callback) {
  if (!shopping_service || !bookmark_model) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::vector<const bookmarks::BookmarkNode*>()));
    return;
  }

  shopping_service->GetAllSubscriptions(
      SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::WeakPtr<ShoppingService> service,
             base::WeakPtr<bookmarks::BookmarkModel> model,
             base::OnceCallback<void(
                 std::vector<const bookmarks::BookmarkNode*>)> callback,
             std::vector<CommerceSubscription> subscriptions) {
            std::vector<const bookmarks::BookmarkNode*> shopping_bookmarks =
                GetAllShoppingBookmarks(model.get());

            // Get all cluster IDs in a map for easier lookup.
            std::unordered_set<uint64_t> cluster_set;
            for (auto sub : subscriptions) {
              if (sub.management_type == ManagementType::kUserManaged) {
                uint64_t cluster_id;
                if (base::StringToUint64(sub.id, &cluster_id)) {
                  cluster_set.insert(cluster_id);
                }
              }
            }

            std::vector<const bookmarks::BookmarkNode*> tracked_bookmarks;
            for (const bookmarks::BookmarkNode* node : shopping_bookmarks) {
              std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
                  power_bookmarks::GetNodePowerBookmarkMeta(model.get(), node);

              if (!meta || !meta->has_shopping_specifics()) {
                continue;
              }

              const power_bookmarks::ShoppingSpecifics specifics =
                  meta->shopping_specifics();

              if (!cluster_set.contains(specifics.product_cluster_id())) {
                continue;
              }

              tracked_bookmarks.push_back(node);
            }
            std::move(callback).Run(std::move(tracked_bookmarks));
          },
          shopping_service->AsWeakPtr(), bookmark_model->AsWeakPtr(),
          std::move(callback)));
}

std::vector<const bookmarks::BookmarkNode*> GetAllShoppingBookmarks(
    bookmarks::BookmarkModel* model) {
  CHECK(model);

  power_bookmarks::PowerBookmarkQueryFields query;
  query.type = power_bookmarks::PowerBookmarkType::SHOPPING;

  std::vector<const bookmarks::BookmarkNode*> nodes =
      power_bookmarks::GetBookmarksMatchingProperties(model, query, -1);

  std::erase_if(nodes, [model](const bookmarks::BookmarkNode* node) {
    return model->IsLocalOnlyNode(*node);
  });

  return nodes;
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
  } else if (!info.previous_amount_micros.has_value() &&
             specifics->has_previous_price()) {
    specifics->clear_previous_price();
    changed = true;
  }

  if (info.offer_id.has_value() &&
      specifics->offer_id() != info.offer_id.value()) {
    specifics->set_offer_id(info.offer_id.value());
    changed = true;
  }

  // Only update the cluster ID if it was previously empty. Having this value
  // change would cause serious problems elsewhere.
  if (info.product_cluster_id.has_value() &&
      !specifics->has_product_cluster_id()) {
    specifics->set_product_cluster_id(info.product_cluster_id.value());
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

bool GetEmailNotificationPrefValue(PrefService* pref_service) {
  return pref_service &&
         pref_service->GetBoolean(kPriceEmailNotificationsEnabled);
}

bool IsEmailNotificationPrefSetByUser(PrefService* pref_service) {
  return pref_service &&
         pref_service->HasPrefPath(kPriceEmailNotificationsEnabled);
}

CommerceSubscription BuildUserSubscriptionForClusterId(uint64_t cluster_id) {
  return CommerceSubscription(
      SubscriptionType::kPriceTrack, IdentifierType::kProductClusterId,
      base::NumberToString(cluster_id), ManagementType::kUserManaged);
}

bool CanTrackPrice(const ProductInfo& info) {
  return info.product_cluster_id.has_value();
}

bool CanTrackPrice(const std::optional<ProductInfo>& info) {
  return info.has_value() && CanTrackPrice(info.value());
}

bool CanTrackPrice(const power_bookmarks::ShoppingSpecifics& specifics) {
  return specifics.has_product_cluster_id();
}

std::optional<std::u16string> GetBookmarkParentName(
    bookmarks::BookmarkModel* model,
    const GURL& url) {
  const bookmarks::BookmarkNode* node =
      model->GetMostRecentlyAddedUserNodeForURL(url);
  if (!node || model->IsLocalOnlyNode(*node)) {
    return std::nullopt;
  }
  return std::optional<std::u16string>(node->parent()->GetTitle());
}

const bookmarks::BookmarkNode* GetShoppingCollectionBookmarkFolder(
    bookmarks::BookmarkModel* model,
    bool create_if_needed) {
  if (!model) {
    return nullptr;
  }

  const base::Uuid collection_uuid =
      base::Uuid::ParseLowercase(bookmarks::kShoppingCollectionUuid);

  const bookmarks::BookmarkNode* collection_node = model->GetNodeByUuid(
      collection_uuid,
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);

  CHECK(!collection_node || collection_node->is_folder());

  if (!collection_node && !create_if_needed) {
    return nullptr;
  }

  if (!collection_node) {
    collection_node = model->AddFolder(
        model->other_node(), model->other_node()->children().size(),
        l10n_util::GetStringUTF16(IDS_SHOPPING_COLLECTION_FOLDER_NAME), nullptr,
        std::nullopt, collection_uuid);
    CHECK_EQ(
        model->GetNodeByUuid(collection_uuid,
                             bookmarks::BookmarkModel::NodeTypeForUuidLookup::
                                 kLocalOrSyncableNodes),
        collection_node);
  }

  return collection_node;
}

bool IsShoppingCollectionBookmarkFolder(const bookmarks::BookmarkNode* node) {
  return node && node->is_folder() &&
         node->uuid() ==
             base::Uuid::ParseLowercase(bookmarks::kShoppingCollectionUuid);
}

std::optional<uint64_t> GetProductClusterIdFromBookmark(
    const GURL& url,
    bookmarks::BookmarkModel* model) {
  const bookmarks::BookmarkNode* node =
      model->GetMostRecentlyAddedUserNodeForURL(url);

  if (!node || model->IsLocalOnlyNode(*node)) {
    return std::nullopt;
  }

  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(model, node);

  if (!meta) {
    return std::nullopt;
  }

  const power_bookmarks::ShoppingSpecifics specifics =
      meta->shopping_specifics();

  return specifics.has_product_cluster_id()
             ? std::optional<uint64_t>(specifics.product_cluster_id())
             : std::nullopt;
}

void RemoveDanglingSubscriptions(
    ShoppingService* shopping_service,
    bookmarks::BookmarkModel* bookmark_model,
    base::OnceCallback<void(size_t)> completed_callback) {
  if (!shopping_service || !bookmark_model) {
    return;
  }

  shopping_service->GetAllSubscriptions(
      commerce::SubscriptionType::kPriceTrack,
      base::BindOnce(
          [](base::WeakPtr<ShoppingService> service,
             bookmarks::BookmarkModel* model,
             base::OnceCallback<void(size_t)> callback,
             std::vector<CommerceSubscription> subscriptions) {
            if (!service) {
              std::move(callback).Run(0);
              return;
            }

            std::unique_ptr<std::vector<CommerceSubscription>> dangling_subs =
                std::make_unique<std::vector<CommerceSubscription>>();

            for (CommerceSubscription sub : subscriptions) {
              if (sub.management_type != ManagementType::kUserManaged) {
                continue;
              }

              uint64_t cluster_id;
              if (!base::StringToUint64(sub.id, &cluster_id)) {
                continue;
              }

              // If there is at least one bookmark with the corresponding
              // subscription, no need to clean up.
              if (GetBookmarksWithClusterId(model, cluster_id, 1).size() > 0) {
                continue;
              }

              dangling_subs->push_back(sub);
            }

            size_t sub_count = dangling_subs->size();
            if (sub_count > 0) {
              service->Unsubscribe(
                  std::move(dangling_subs),
                  base::BindOnce(
                      [](base::OnceCallback<void(size_t)> callback,
                         size_t count,
                         bool success) { std::move(callback).Run(count); },
                      std::move(callback), sub_count));
            } else {
              std::move(callback).Run(0);
            }
          },
          shopping_service->AsWeakPtr(), bookmark_model,
          std::move(completed_callback)));
}

}  // namespace commerce
