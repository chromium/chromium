// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_most_visited_provider.h"

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"

namespace ash {

BirchMostVisitedProvider::BirchMostVisitedProvider(Profile* profile)
    : profile_(profile),
      history_service_(HistoryServiceFactory::GetForProfile(
          profile_,
          ServiceAccessType::EXPLICIT_ACCESS)),
      favicon_service_(FaviconServiceFactory::GetForProfile(
          profile_,
          ServiceAccessType::EXPLICIT_ACCESS)) {}

BirchMostVisitedProvider::~BirchMostVisitedProvider() = default;

void BirchMostVisitedProvider::RequestBirchDataFetch() {
  // Get the most frequently accessed URL.
  history_service_->QueryMostVisitedURLs(
      /*result_count=*/1,
      base::BindOnce(&BirchMostVisitedProvider::OnGotMostVisitedURLs,
                     weak_factory_.GetWeakPtr()),
      &cancelable_task_tracker_);
}

void BirchMostVisitedProvider::OnGotMostVisitedURLs(
    history::MostVisitedURLList urls) {
  if (urls.empty()) {
    Shell::Get()->birch_model()->SetMostVisitedItems({});
    return;
  }
  // Birch only shows the most frequent URL.
  const auto& most_visited_url = urls[0];

  // If the URL hasn't changed, reuse the previous icon.
  if (most_visited_url.url == last_url_) {
    std::vector<BirchMostVisitedItem> most_visited_items;
    most_visited_items.emplace_back(most_visited_url.title,
                                    most_visited_url.url,
                                    ui::ImageModel::FromImage(last_image_));
    Shell::Get()->birch_model()->SetMostVisitedItems(
        std::move(most_visited_items));
    return;
  }

  // Load the favicon for the page.
  favicon_service_->GetFaviconImageForPageURL(
      most_visited_url.url,
      base::BindOnce(&BirchMostVisitedProvider::OnGotFaviconImage,
                     weak_factory_.GetWeakPtr(), most_visited_url.title,
                     most_visited_url.url),
      &cancelable_task_tracker_);
}

void BirchMostVisitedProvider::OnGotFaviconImage(
    const std::u16string& title,
    const GURL& url,
    const favicon_base::FaviconImageResult& image_result) {
  // Don't show the result if there's no icon available (should be rare).
  if (image_result.image.IsEmpty()) {
    Shell::Get()->birch_model()->SetMostVisitedItems({});
    return;
  }
  // Populate the BirchModel with this URL.
  std::vector<BirchMostVisitedItem> most_visited_items;
  most_visited_items.emplace_back(
      title, url, ui::ImageModel::FromImage(image_result.image));
  Shell::Get()->birch_model()->SetMostVisitedItems(
      std::move(most_visited_items));

  // Cache the data for next time.
  last_url_ = url;
  last_image_ = image_result.image;
}

}  // namespace ash
