// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_last_active_provider.h"

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/keyed_service/core/service_access_type.h"

namespace ash {

BirchLastActiveProvider::BirchLastActiveProvider(Profile* profile)
    : profile_(profile),
      history_service_(HistoryServiceFactory::GetForProfile(
          profile_,
          ServiceAccessType::EXPLICIT_ACCESS)),
      favicon_service_(FaviconServiceFactory::GetForProfile(
          profile_,
          ServiceAccessType::EXPLICIT_ACCESS)) {}

BirchLastActiveProvider::~BirchLastActiveProvider() = default;

void BirchLastActiveProvider::RequestBirchDataFetch() {
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

  // If the URL hasn't changed, use the last icon.
  if (last_active.url() == previous_url_) {
    std::vector<BirchLastActiveItem> last_active_items;
    last_active_items.emplace_back(last_active.title(), last_active.url(),
                                   last_active.last_visit(),
                                   ui::ImageModel::FromImage(previous_image_));
    Shell::Get()->birch_model()->SetLastActiveItems(
        std::move(last_active_items));
    return;
  }

  // Load the favicon for the page.
  favicon_service_->GetFaviconImageForPageURL(
      last_active.url(),
      base::BindOnce(&BirchLastActiveProvider::OnGotFaviconImage,
                     weak_factory_.GetWeakPtr(), last_active.title(),
                     last_active.url(), last_active.last_visit()),
      &cancelable_task_tracker_);
}

void BirchLastActiveProvider::OnGotFaviconImage(
    const std::u16string& title,
    const GURL& url,
    base::Time last_visit,
    const favicon_base::FaviconImageResult& image_result) {
  // Don't show the result if there's no icon available (should be rare).
  if (image_result.image.IsEmpty()) {
    Shell::Get()->birch_model()->SetLastActiveItems({});
    return;
  }
  // Populate the BirchModel with this URL.
  std::vector<BirchLastActiveItem> last_active_items;
  last_active_items.emplace_back(title, url, last_visit,
                                 ui::ImageModel::FromImage(image_result.image));
  Shell::Get()->birch_model()->SetLastActiveItems(std::move(last_active_items));

  // Cache the data for next time.
  previous_url_ = url;
  previous_image_ = image_result.image;
}

}  // namespace ash
