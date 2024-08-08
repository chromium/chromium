// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_last_active_provider.h"

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {

BirchLastActiveProvider::BirchLastActiveProvider(Profile* profile)
    : profile_(profile),
      history_service_(HistoryServiceFactory::GetForProfile(
          profile_,
          ServiceAccessType::EXPLICIT_ACCESS)) {}

BirchLastActiveProvider::~BirchLastActiveProvider() = default;

void BirchLastActiveProvider::RequestBirchDataFetch() {
  // `history_service_` can be null in some tests, so check that here.
  if (!history_service_) {
    Shell::Get()->birch_model()->SetLastActiveItems({});
    return;
  }

  // Get the last active URL. The query results are sorted most-recent first, so
  // we only need to get the first entry to find the last active URL. We only
  // care about URLs in the last week.
  history::QueryOptions options;
  options.max_count = 1;
  options.SetRecentDayRange(7);
  history_service_->QueryHistory(
      u"", options,
      base::BindOnce(&BirchLastActiveProvider::OnGotHistory,
                     weak_factory_.GetWeakPtr()),
      &cancelable_task_tracker_);
}

void BirchLastActiveProvider::OnGotHistory(history::QueryResults results) {
  if (results.empty()) {
    Shell::Get()->birch_model()->SetLastActiveItems({});
    return;
  }
  const history::URLResult& last_active = results[0];

  std::vector<BirchLastActiveItem> last_active_items;
  last_active_items.emplace_back(last_active.title(), last_active.url(),
                                 last_active.last_visit());
  Shell::Get()->birch_model()->SetLastActiveItems(std::move(last_active_items));
}

}  // namespace ash
