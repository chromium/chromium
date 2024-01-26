// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_file_suggest_provider.h"

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/shell.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

namespace {

// Gets a list of `BirchFileItem` given a list of `FileSuggestData`.
// Performs some asynchronous file IO, so should not be run on the UI thread.
std::vector<BirchFileItem> GetFileSuggestionInfo(
    const std::vector<FileSuggestData>& file_suggestions) {
  std::vector<BirchFileItem> results;
  for (const auto& suggestion : file_suggestions) {
    base::File::Info info;
    if (base::GetFileInfo(suggestion.file_path, &info)) {
      // Get the most recent time between last modified and last accessed.
      base::Time timestamp = (info.last_modified > info.last_accessed)
                                 ? info.last_modified
                                 : info.last_accessed;
      results.emplace_back(suggestion.file_path, timestamp);
    }
  }
  return results;
}

}  // namespace

BirchFileSuggestProvider::BirchFileSuggestProvider(Profile* profile)
    : file_suggest_service_(
          FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile)) {
  file_suggest_service_observation_.Observe(file_suggest_service_);
}

BirchFileSuggestProvider::~BirchFileSuggestProvider() = default;

void BirchFileSuggestProvider::OnFileSuggestionUpdated(
    FileSuggestionType type) {
  weak_factory_.InvalidateWeakPtrs();

  if (type == FileSuggestionType::kDriveFile) {
    file_suggest_service_->GetSuggestFileData(
        type,
        base::BindOnce(&BirchFileSuggestProvider::OnSuggestedFileDataUpdated,
                       weak_factory_.GetWeakPtr()));
  }
}

void BirchFileSuggestProvider::OnSuggestedFileDataUpdated(
    const absl::optional<std::vector<FileSuggestData>>& suggest_results) {
  if (!suggest_results) {
    OnFileInfoRetrieved({});
    return;
  }

  // Convert each `FileSuggestData` into a `BirchFileItem`.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetFileSuggestionInfo, suggest_results.value()),
      base::BindOnce(&BirchFileSuggestProvider::OnFileInfoRetrieved,
                     weak_factory_.GetWeakPtr()));
}

void BirchFileSuggestProvider::OnFileInfoRetrieved(
    std::vector<BirchFileItem> file_items) {
  Shell::Get()->birch_model()->SetFileSuggestItems(std::move(file_items));
}

}  // namespace ash
