// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_keyed_service.h"

#include "base/files/file.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

// Gets a list of `BirchFileSuggestion` given a list of `FileSuggestData`.
// Performs some asynchronous file IO, so should not be run on the UI thread.
std::vector<BirchFileSuggestion> GetFileSuggestionInfo(
    std::vector<FileSuggestData> file_suggestions) {
  std::vector<BirchFileSuggestion> results;
  for (const auto& suggestion : file_suggestions) {
    base::File::Info info;
    if (base::GetFileInfo(suggestion.file_path, &info)) {
      results.emplace_back(suggestion.file_path, info.last_modified,
                           info.last_accessed);
    }
  }
  return results;
}

}  // namespace

BirchFileSuggestion::BirchFileSuggestion(
    base::FilePath new_file_path,
    const absl::optional<base::Time> new_last_modified,
    const absl::optional<base::Time> new_last_accessed)
    : file_path(new_file_path),
      last_modified(new_last_modified),
      last_accessed(new_last_accessed) {}

BirchFileSuggestion::BirchFileSuggestion(BirchFileSuggestion&&) = default;

BirchFileSuggestion::BirchFileSuggestion(const BirchFileSuggestion&) = default;

BirchFileSuggestion& BirchFileSuggestion::operator=(
    const BirchFileSuggestion&) = default;

BirchFileSuggestion::~BirchFileSuggestion() = default;

BirchKeyedService::BirchKeyedService(Profile* profile)
    : file_suggest_service_(
          FileSuggestKeyedServiceFactory::GetInstance()->GetService(profile)) {
  file_suggest_service_observation_.Observe(file_suggest_service_);
}

BirchKeyedService::~BirchKeyedService() = default;

void BirchKeyedService::OnFileSuggestionUpdated(FileSuggestionType type) {
  weak_factory_.InvalidateWeakPtrs();

  if (type == FileSuggestionType::kDriveFile) {
    file_suggest_service_->GetSuggestFileData(
        type, base::BindOnce(&BirchKeyedService::OnSuggestedFileDataUpdated,
                             weak_factory_.GetWeakPtr()));
  }
}

void BirchKeyedService::OnSuggestedFileDataUpdated(
    const absl::optional<std::vector<FileSuggestData>>& suggest_results) {
  if (suggest_results) {
    // Convert each `FileSuggestData` into a `BirchFileSuggestion`.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&GetFileSuggestionInfo, suggest_results.value()),
        base::BindOnce(&BirchKeyedService::OnFileInfoRetrieved,
                       weak_factory_.GetWeakPtr()));
  }
}

void BirchKeyedService::OnFileInfoRetrieved(
    std::vector<BirchFileSuggestion> file_suggestions) {
  // TODO(b/304289452): Pass `file_suggestions` to the birch model.
}

}  // namespace ash
