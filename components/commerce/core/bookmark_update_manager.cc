// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/bookmark_update_manager.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "url/gurl.h"

namespace commerce {

BookmarkUpdateManager::BookmarkUpdateManager(ShoppingService* service,
                                             bookmarks::BookmarkModel* model,
                                             PrefService* prefs)
    : shopping_service_(service),
      bookmark_model_(model),
      pref_service_(prefs) {}

BookmarkUpdateManager::~BookmarkUpdateManager() = default;

void BookmarkUpdateManager::ScheduleUpdate() {
  // Check the kill switch. This enabled by default, but can be turned off in
  // case we accidentally flood the backend with requests.
  if (!base::FeatureList::IsEnabled(kCommerceAllowOnDemandBookmarkUpdates))
    return;

  // Make sure we don't double-schedule.
  if (scheduled_task_)
    return;

  // By default, time is "null" meaning it is set to 0. In this state, read the
  // preference once and then use the in-memory version from this point on.
  if (last_update_time_.is_null()) {
    last_update_time_ =
        pref_service_->GetTime(kShoppingListBookmarkLastUpdateTime);
  }

  base::TimeDelta time_since_last = base::Time::Now() - last_update_time_;

  base::TimeDelta interval = kShoppingListBookmarkpdateIntervalParam.Get();
  int64_t ms_delay =
      std::clamp((interval - time_since_last).InMilliseconds(),
                 base::Seconds(0L).InMilliseconds(), interval.InMilliseconds());

  scheduled_task_ =
      std::make_unique<base::CancelableOnceClosure>(base::BindOnce(
          &BookmarkUpdateManager::RunUpdate, weak_ptr_factory_.GetWeakPtr()));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, scheduled_task_->callback(), base::Milliseconds(ms_delay));
}

void BookmarkUpdateManager::CancelUpdates() {
  if (scheduled_task_) {
    scheduled_task_->Cancel();
    scheduled_task_ = nullptr;
  }
}

void BookmarkUpdateManager::RunUpdate() {
  // Record the current time as last updated time and immediately schedule the
  // next update.
  last_update_time_ = base::Time::Now();
  pref_service_->SetTime(kShoppingListBookmarkLastUpdateTime,
                         last_update_time_);

  // If something like the enterprise policy was turned off, simply block the
  // update logic. In the future we can observe the preference and remove or
  // re-add the scheduled update, but this is easier for now.
  if (!shopping_service_->IsShoppingListEligible())
    return;

  scheduled_task_ = nullptr;
  ScheduleUpdate();

  std::vector<const bookmarks::BookmarkNode*> nodes =
      GetAllShoppingBookmarks(bookmark_model_);
  std::vector<int64_t> ids;
  for (auto* node : nodes)
    ids.push_back(node->id());

  shopping_service_->GetUpdatedProductInfoForBookmarks(
      ids, base::BindRepeating(&BookmarkUpdateManager::HandleOnDemandResponse,
                               weak_ptr_factory_.GetWeakPtr()));
}

void BookmarkUpdateManager::HandleOnDemandResponse(
    const int64_t bookmark_id,
    const GURL& url,
    absl::optional<ProductInfo> info) {
  if (!info.has_value())
    return;

  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(bookmark_model_, bookmark_id);
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(bookmark_model_, node);

  if (!meta || !meta->has_shopping_specifics())
    return;

  if (PopulateOrUpdateBookmarkMetaIfNeeded(meta.get(), info.value())) {
    power_bookmarks::SetNodePowerBookmarkMeta(bookmark_model_, node,
                                              std::move(meta));
  }
}

}  // namespace commerce
