// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service.h"

#include "chrome/browser/ui/app_list/search/files/drive_file_suggestion_provider.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_util.h"

namespace app_list {

namespace {
using SuggestResults = std::vector<FileSuggestData>;
}  // namespace

FileSuggestKeyedService::FileSuggestKeyedService(Profile* profile) {
  drive_file_suggestion_provider_ =
      std::make_unique<DriveFileSuggestionProvider>(
          profile, base::BindRepeating(
                       &FileSuggestKeyedService::OnSuggestionProviderUpdated,
                       weak_factory_.GetWeakPtr()));
}

FileSuggestKeyedService::~FileSuggestKeyedService() = default;

void FileSuggestKeyedService::MaybeUpdateItemSuggestCache(
    base::PassKey<ZeroStateDriveProvider>) {
  drive_file_suggestion_provider_->MaybeUpdateItemSuggestCache(
      base::PassKey<FileSuggestKeyedService>());
}

void FileSuggestKeyedService::GetSuggestFileData(
    FileSuggestionType type,
    base::OnceCallback<
        void(const absl::optional<std::vector<FileSuggestData>>&)> callback) {
  switch (type) {
    case FileSuggestionType::kDriveFile:
      drive_file_suggestion_provider_->GetSuggestFileData(std::move(callback));
      return;
  }
  NOTREACHED();
}

void FileSuggestKeyedService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FileSuggestKeyedService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FileSuggestKeyedService::HasPendingSuggestionFetchForTest() const {
  // TODO(https://crbug.com/1352516): modify this code when local file
  // suggestions are supported.
  return drive_file_suggestion_provider_
      ->HasPendingDriveSuggestionFetchForTest();
}

void FileSuggestKeyedService::OnSuggestionProviderUpdated(
    FileSuggestionType type) {
  for (auto& observer : observers_)
    observer.OnFileSuggestionUpdated(type);
}

}  // namespace app_list
