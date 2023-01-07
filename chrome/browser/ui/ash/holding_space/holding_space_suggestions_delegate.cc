// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_suggestions_delegate.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service_factory.h"

namespace ash {
namespace {

// Returns the holding space item type that matches a given suggestion type.
HoldingSpaceItem::Type GetItemTypeFromSuggestionType(
    app_list::FileSuggestionType suggestion_type) {
  switch (suggestion_type) {
    case app_list::FileSuggestionType::kDriveFile:
      return HoldingSpaceItem::Type::kDriveSuggestion;
    case app_list::FileSuggestionType::kLocalFile:
      return HoldingSpaceItem::Type::kLocalSuggestion;
  }
}

// Returns whether `item` represents a pinned file that also exists as a
// suggested file in `suggestions_by_type`.
bool ItemIsPinnedSuggestion(
    const HoldingSpaceItem* item,
    const std::map<app_list::FileSuggestionType,
                   std::vector<app_list::FileSuggestData>>&
        suggestions_by_type) {
  if (item->type() != HoldingSpaceItem::Type::kPinnedFile)
    return false;

  for (const auto& [_, raw_suggestions] : suggestions_by_type) {
    if (base::Contains(raw_suggestions, item->file_path(),
                       [](const app_list::FileSuggestData& suggestion) {
                         return suggestion.file_path;
                       })) {
      return true;
    }
  }

  return false;
}

}  // namespace

HoldingSpaceSuggestionsDelegate::HoldingSpaceSuggestionsDelegate(
    HoldingSpaceKeyedService* service,
    HoldingSpaceModel* model)
    : HoldingSpaceKeyedServiceDelegate(service, model) {
  DCHECK(ash::features::IsHoldingSpaceSuggestionsEnabled());
}

HoldingSpaceSuggestionsDelegate::~HoldingSpaceSuggestionsDelegate() = default;

void HoldingSpaceSuggestionsDelegate::OnHoldingSpaceItemsAdded(
    const std::vector<const HoldingSpaceItem*>& items) {
  if (base::ranges::any_of(items, [&](const HoldingSpaceItem* item) {
        return item->IsInitialized() &&
               ItemIsPinnedSuggestion(item, suggestions_by_type_);
      })) {
    UpdateSuggestionsInModel();
  }
}

void HoldingSpaceSuggestionsDelegate::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  if (base::ranges::any_of(items, [&](const HoldingSpaceItem* item) {
        return item->IsInitialized() &&
               ItemIsPinnedSuggestion(item, suggestions_by_type_);
      })) {
    UpdateSuggestionsInModel();
  }
}

void HoldingSpaceSuggestionsDelegate::OnHoldingSpaceItemInitialized(
    const HoldingSpaceItem* item) {
  if (ItemIsPinnedSuggestion(item, suggestions_by_type_))
    UpdateSuggestionsInModel();
}

void HoldingSpaceSuggestionsDelegate::OnPersistenceRestored() {
  file_suggest_service_observation_.Observe(
      app_list::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
          profile()));

  MaybeFetchSuggestions(app_list::FileSuggestionType::kDriveFile);
  MaybeFetchSuggestions(app_list::FileSuggestionType::kLocalFile);
}

void HoldingSpaceSuggestionsDelegate::OnFileSuggestionUpdated(
    app_list::FileSuggestionType type) {
  MaybeFetchSuggestions(type);
}

void HoldingSpaceSuggestionsDelegate::MaybeFetchSuggestions(
    app_list::FileSuggestionType type) {
  // A data query on `type` has been sent so it is unnecessary to send a request
  // again. Return early.
  if (base::Contains(pending_fetches_, type))
    return;

  // Mark that the query for suggestions of `type` has been sent.
  pending_fetches_.insert(type);

  app_list::FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->GetSuggestFileData(
          type,
          base::BindOnce(&HoldingSpaceSuggestionsDelegate::OnSuggestionsFetched,
                         weak_factory_.GetWeakPtr(), type));
}

void HoldingSpaceSuggestionsDelegate::OnSuggestionsFetched(
    app_list::FileSuggestionType type,
    const absl::optional<std::vector<app_list::FileSuggestData>>& suggestions) {
  // Mark that the suggestions of `type` have been fetched.
  size_t deleted_size = pending_fetches_.erase(type);
  DCHECK_EQ(1u, deleted_size);

  if (!suggestions)
    return;

  // Update `suggestions_by_type_`.
  suggestions_by_type_[type] = *suggestions;

  UpdateSuggestionsInModel();
}

void HoldingSpaceSuggestionsDelegate::UpdateSuggestionsInModel() {
  std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>
      suggestion_items;
  base::FilePath downloads_folder =
      file_manager::util::GetDownloadsFolderForProfile(profile());
  for (const auto& [type, raw_suggestions] : suggestions_by_type_) {
    HoldingSpaceItem::Type item_type = GetItemTypeFromSuggestionType(type);
    for (const auto& suggestion : raw_suggestions) {
      if (suggestion.file_path != downloads_folder &&
          !model()->ContainsItem(HoldingSpaceItem::Type::kPinnedFile,
                                 suggestion.file_path)) {
        suggestion_items.emplace_back(item_type, suggestion.file_path);
      }
    }
  }

  service()->SetSuggestions(suggestion_items);
}

}  // namespace ash
