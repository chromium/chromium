// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_most_visited_provider.h"

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {

BirchMostVisitedProvider::BirchMostVisitedProvider(Profile* profile)
    : profile_(profile),
      history_service_(HistoryServiceFactory::GetForProfile(
          profile_,
          ServiceAccessType::EXPLICIT_ACCESS)) {}

BirchMostVisitedProvider::~BirchMostVisitedProvider() = default;

void BirchMostVisitedProvider::RequestBirchDataFetch() {
  // `history_service_` can be null in some tests, so check that here.
  if (!history_service_) {
    Shell::Get()->birch_model()->SetMostVisitedItems({});
    return;
  }

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

  std::vector<BirchMostVisitedItem> most_visited_items;
  most_visited_items.emplace_back(most_visited_url.title, most_visited_url.url);
  Shell::Get()->birch_model()->SetMostVisitedItems(
      std::move(most_visited_items));
}

}  // namespace ash
