// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_suggestions_delegate.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "base/containers/adapters.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"

namespace ash {
namespace {

// Returns the holding space item type that matches a given suggestion type.
HoldingSpaceItem::Type GetItemTypeFromSuggestionType(
    FileSuggestionType suggestion_type) {
  switch (suggestion_type) {
    case FileSuggestionType::kDriveFile:
      return HoldingSpaceItem::Type::kDriveSuggestion;
    case FileSuggestionType::kLocalFile:
      return HoldingSpaceItem::Type::kLocalSuggestion;
  }
}

// Returns whether `item` represents a pinned file that also exists as a
// suggested file in `suggestions_by_type`.
bool ItemIsPinnedSuggestion(
    const HoldingSpaceItem* item,
    std::map<HoldingSpaceItem::Type, std::vector<base::FilePath>>&
        suggestions_by_type) {
  if (item->type() != HoldingSpaceItem::Type::kPinnedFile)
    return false;

  for (const auto& [_, suggested_file_paths] : suggestions_by_type) {
    if (base::Contains(suggested_file_paths, item->file().file_path)) {
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
  DCHECK(features::IsHoldingSpaceSuggestionsEnabled());
}

HoldingSpaceSuggestionsDelegate::~HoldingSpaceSuggestionsDelegate() = default;

void HoldingSpaceSuggestionsDelegate::RefreshSuggestions() {
  MaybeFetchSuggestions(FileSuggestionType::kDriveFile);
  MaybeFetchSuggestions(FileSuggestionType::kLocalFile);
}

void HoldingSpaceSuggestionsDelegate::RemoveSuggestions(
    const std::vector<base::FilePath>& absolute_file_paths) {
  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->RemoveSuggestionsAndNotify(absolute_file_paths);
}

void HoldingSpaceSuggestionsDelegate::OnHoldingSpaceItemsAdded(
    const std::vector<const HoldingSpaceItem*>& items) {
  if (base::ranges::any_of(items, [&](const HoldingSpaceItem* item) {
        return item->IsInitialized() &&
               ItemIsPinnedSuggestion(item, suggestions_by_type_);
      })) {
    // Update suggestions asynchronously to avoid updating suggestions along
    // with other model updates.
    MaybeScheduleUpdateSuggestionsInModel();
  }
}

void HoldingSpaceSuggestionsDelegate::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  if (base::ranges::any_of(items, [&](const HoldingSpaceItem* item) {
        return item->IsInitialized() &&
               ItemIsPinnedSuggestion(item, suggestions_by_type_);
      })) {
    // Update suggestions asynchronously to avoid updating suggestions along
    // with other model updates.
    MaybeScheduleUpdateSuggestionsInModel();
  }
}

void HoldingSpaceSuggestionsDelegate::OnHoldingSpaceItemInitialized(
    const HoldingSpaceItem* item) {
  if (ItemIsPinnedSuggestion(item, suggestions_by_type_)) {
    // Update suggestions asynchronously to avoid updating suggestions along
    // with other model updates.
    MaybeScheduleUpdateSuggestionsInModel();
  }
}

void HoldingSpaceSuggestionsDelegate::OnPersistenceRestored() {
  // Initialize `suggestions_by_type_` with the restored suggestions. The model
  // items are iterated reversely so that the suggestions of the same category
  // in `suggestions_by_type_` follow the relevance order.
  DCHECK(suggestions_by_type_.empty());
  for (const auto& item : base::Reversed(model()->items())) {
    // Skip if `item` is not a suggestion.
    if (HoldingSpaceItem::IsSuggestionType(item->type())) {
      suggestions_by_type_[item->type()].push_back(item->file().file_path);
    }
  }

  file_suggest_service_observation_.Observe(
      FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile()));

  MaybeFetchSuggestions(FileSuggestionType::kDriveFile);
  MaybeFetchSuggestions(FileSuggestionType::kLocalFile);
}

void HoldingSpaceSuggestionsDelegate::OnFileSuggestionUpdated(
    FileSuggestionType type) {
  MaybeFetchSuggestions(type);
}

void HoldingSpaceSuggestionsDelegate::MaybeFetchSuggestions(
    FileSuggestionType type) {
  // A data query on `type` has been sent so it is unnecessary to send a request
  // again. Return early.
  if (base::Contains(pending_fetches_, type))
    return;

  // Mark that the query for suggestions of `type` has been sent.
  pending_fetches_.insert(type);

  FileSuggestKeyedServiceFactory::GetInstance()
      ->GetService(profile())
      ->GetSuggestFileData(
          type,
          base::BindOnce(&HoldingSpaceSuggestionsDelegate::OnSuggestionsFetched,
                         weak_factory_.GetWeakPtr(), type));
}

void HoldingSpaceSuggestionsDelegate::MaybeScheduleUpdateSuggestionsInModel() {
  // Return early if the task of updating model suggestions has been scheduled.
  if (suggestion_update_timer_.IsRunning())
    return;

  suggestion_update_timer_.Start(
      FROM_HERE, /*delay=*/base::TimeDelta(),
      base::BindOnce(&HoldingSpaceSuggestionsDelegate::UpdateSuggestionsInModel,
                     weak_factory_.GetWeakPtr()));
}

void HoldingSpaceSuggestionsDelegate::OnSuggestionsFetched(
    FileSuggestionType type,
    const std::optional<std::vector<FileSuggestData>>& suggestions) {
  // Mark that the suggestions of `type` have been fetched.
  size_t deleted_size = pending_fetches_.erase(type);
  DCHECK_EQ(1u, deleted_size);

  if (!suggestions)
    return;

  // Extract file paths from `suggestions`.
  std::vector<base::FilePath> updated_suggestions(suggestions->size());
  base::ranges::transform(*suggestions, updated_suggestions.begin(),
                          &FileSuggestData::file_path);

  // No-op if `updated_suggestions` are unchanged.
  const HoldingSpaceItem::Type item_type = GetItemTypeFromSuggestionType(type);
  if (auto it = suggestions_by_type_.find(item_type);
      it != suggestions_by_type_.end() && it->second == updated_suggestions) {
    return;
  }

  // Update cache and model.
  suggestions_by_type_[item_type] = std::move(updated_suggestions);
  UpdateSuggestionsInModel();
}

void HoldingSpaceSuggestionsDelegate::UpdateSuggestionsInModel() {
  std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>
      suggestion_items;
  base::FilePath downloads_folder =
      file_manager::util::GetDownloadsFolderForProfile(profile());
  for (const auto& [type, suggested_file_paths] : suggestions_by_type_) {
    for (const auto& file_path : suggested_file_paths) {
      if (file_path != downloads_folder &&
          !model()->ContainsItem(HoldingSpaceItem::Type::kPinnedFile,
                                 file_path)) {
        suggestion_items.emplace_back(type, file_path);
      }
    }
  }

  service()->SetSuggestions(suggestion_items);
}

}  // namespace ash
