// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_file_suggest_provider.h"

#include <vector>

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {


BirchFileSuggestProvider::BirchFileSuggestProvider(Profile* profile)
    : file_suggest_service_(
          FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile)) {
  file_suggest_service_observation_.Observe(file_suggest_service_);
}

BirchFileSuggestProvider::~BirchFileSuggestProvider() = default;

void BirchFileSuggestProvider::RequestBirchDataFetch() {
  file_suggest_service_->GetSuggestFileData(
      FileSuggestionType::kDriveFile,
      base::BindOnce(&BirchFileSuggestProvider::OnSuggestedFileDataUpdated,
                     weak_factory_.GetWeakPtr()));
}

void BirchFileSuggestProvider::OnFileSuggestionUpdated(
    FileSuggestionType type) {
  weak_factory_.InvalidateWeakPtrs();

  if (type == FileSuggestionType::kDriveFile) {
    RequestBirchDataFetch();
  }
}

void BirchFileSuggestProvider::OnSuggestedFileDataUpdated(
    const std::optional<std::vector<FileSuggestData>>& suggest_results) {
  if (!Shell::HasInstance()) {
    return;
  }
  if (!suggest_results) {
    Shell::Get()->birch_model()->SetFileSuggestItems({});
    return;
  }

  std::vector<BirchFileItem> file_items;
  for (const auto& suggestion : *suggest_results) {
    const base::Time timestamp =
        suggestion.timestamp
            ? *suggestion.timestamp
            : suggestion.secondary_timestamp.value_or(base::Time());
    file_items.emplace_back(suggestion.file_path,
                            suggestion.prediction_reason.value_or(u""),
                            timestamp);
  }
  Shell::Get()->birch_model()->SetFileSuggestItems(std::move(file_items));
}

}  // namespace ash
