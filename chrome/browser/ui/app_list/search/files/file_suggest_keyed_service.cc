// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service.h"

#include "chrome/browser/ui/app_list/search/files/drive_file_suggestion_provider.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_util.h"
#include "chrome/browser/ui/app_list/search/files/local_file_suggestion_provider.h"

namespace app_list {

namespace {
using SuggestResults = std::vector<FileSuggestData>;
}  // namespace

FileSuggestKeyedService::FileSuggestKeyedService(
    Profile* profile,
    PersistentProto<RemovedResultsProto> proto)
    : proto_(std::move(proto)) {
  proto_.RegisterOnRead(
      base::BindOnce(&FileSuggestKeyedService::OnRemovedSuggestionProtoReady,
                     weak_factory_.GetWeakPtr()));
  proto_.Init();

  drive_file_suggestion_provider_ =
      std::make_unique<DriveFileSuggestionProvider>(
          profile, base::BindRepeating(
                       &FileSuggestKeyedService::OnSuggestionProviderUpdated,
                       weak_factory_.GetWeakPtr()));

  local_file_suggestion_provider_ =
      std::make_unique<LocalFileSuggestionProvider>(
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
    GetSuggestFileDataCallback callback) {
  // Always return null if `proto_` is not ready.
  if (!proto_.initialized()) {
    std::move(callback).Run(/*suggestions=*/absl::nullopt);
    return;
  }

  GetSuggestFileDataCallback filter_suggestions_callback =
      base::BindOnce(&FileSuggestKeyedService::FilterRemovedSuggestions,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  switch (type) {
    case FileSuggestionType::kDriveFile:
      drive_file_suggestion_provider_->GetSuggestFileData(
          std::move(filter_suggestions_callback));
      return;
    case FileSuggestionType::kLocalFile:
      local_file_suggestion_provider_->GetSuggestFileData(
          std::move(filter_suggestions_callback));
      return;
  }
}

PersistentProto<RemovedResultsProto>* FileSuggestKeyedService::GetProto(
    base::PassKey<RankerDelegate>) {
  return &proto_;
}

void FileSuggestKeyedService::RemoveFileSuggestion(
    FileSuggestionType type,
    const std::string& suggestion_id) {
  if (proto_.initialized()) {
    // Record the string ID of `result` to the storage proto's map.
    // Note: We are using a map for its set capabilities; the map value is
    // arbitrary.
    proto_->mutable_removed_ids()->insert({suggestion_id, false});
    proto_.StartWrite();

    // Suggestions of `type` update because of a new removed suggestion. Notify
    // clients of this update.
    OnSuggestionProviderUpdated(type);
  }
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

bool FileSuggestKeyedService::IsReadyForTest() const {
  // TODO(https://crbug.com/1352515): check whether local file suggestions are
  // ready when local file suggestions are supported by the service.
  return proto_.initialized();
}

void FileSuggestKeyedService::FilterRemovedSuggestions(
    GetSuggestFileDataCallback callback,
    const absl::optional<std::vector<FileSuggestData>>& suggestions) {
  DCHECK(proto_.initialized());

  // There are no candidate suggestions to filter. Therefore, return early.
  if (!suggestions.has_value() || suggestions->empty()) {
    std::move(callback).Run(suggestions);
    return;
  }

  std::vector<FileSuggestData> filtered_suggestions;
  for (const auto& suggestion : *suggestions) {
    if (!proto_->removed_ids().contains(suggestion.id)) {
      // Skip the suggestions whose ids exist in `proto_`.
      filtered_suggestions.push_back(suggestion);
    }
  }

  std::move(callback).Run(filtered_suggestions);
}

void FileSuggestKeyedService::OnRemovedSuggestionProtoReady(
    ReadStatus read_status) {
  OnSuggestionProviderUpdated(FileSuggestionType::kDriveFile);

  // TODO(https://crbug.com/1352515): check whether local file suggestions are
  // ready when local file suggestions are supported by the service.
  OnSuggestionProviderUpdated(FileSuggestionType::kLocalFile);
}

}  // namespace app_list
