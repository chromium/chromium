// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service.h"

#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/file_suggest_util.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"

namespace app_list {

namespace {

// Given an absolute path representing a file in the user's Drive, returns a
// reparented version of the path within the user's drive fs mount.
base::FilePath ReparentToDriveMount(
    const base::FilePath& path,
    const drive::DriveIntegrationService* drive_service) {
  DCHECK(!path.IsAbsolute());
  return drive_service->GetMountPointPath().Append(path.value());
}

}  // namespace

FileSuggestKeyedService::FileSuggestKeyedService(Profile* profile)
    : profile_(profile),
      drive_service_(
          drive::DriveIntegrationServiceFactory::GetInstance()->GetForProfile(
              profile_)),
      item_suggest_cache_(std::make_unique<ItemSuggestCache>(
          profile,
          profile->GetDefaultStoragePartition()
              ->GetURLLoaderFactoryForBrowserProcess())) {}

FileSuggestKeyedService::~FileSuggestKeyedService() = default;

void FileSuggestKeyedService::GetSuggestFileData(
    SuggestDataType type,
    GetSuggestDataCallback callback) {
  switch (type) {
    case SuggestDataType::kItemSuggest:
      GetDriveSuggestFileData(std::move(callback));
      return;
  }

  NOTREACHED();
}

void FileSuggestKeyedService::MaybeUpdateItemSuggestCache(
    base::PassKey<ZeroStateDriveProvider>) {
  item_suggest_cache_->MaybeUpdateCache();
}

base::CallbackListSubscription
FileSuggestKeyedService ::RegisterItemSuggestUpdateCallback(
    ItemSuggestCache::OnResultsCallback callback) {
  return item_suggest_cache_->RegisterCallback(callback);
}

void FileSuggestKeyedService::GetDriveSuggestFileData(
    GetSuggestDataCallback callback) {
  // NOTE: Validate the drive cache results whenever drive file suggest data is
  // fetched to guarantee the file path validity.

  const bool has_active_validation =
      !on_drive_results_ready_callback_list_.empty();

  // Add `callback` to the waiting list.
  on_drive_results_ready_callback_list_.AddUnsafe(std::move(callback));

  // Return early if there is an active validation. `callback` will run when
  // validation completes.
  if (has_active_validation)
    return;

  // If there is not any available drive service, return early.
  if (!drive_service_ || !drive_service_->IsMounted()) {
    EndDriveFilePathValidation(
        /*suggest_results=*/absl::nullopt,
        DriveSuggestValidationStatus::kDriveFSNotMounted);
    return;
  } else if (profile_->GetPrefs()->GetBoolean(drive::prefs::kDisableDrive)) {
    EndDriveFilePathValidation(/*suggest_results=*/absl::nullopt,
                               DriveSuggestValidationStatus::kDriveDisabled);
    return;
  }

  const absl::optional<ItemSuggestCache::Results> results_before_validation =
      item_suggest_cache_->GetResults();

  // If there is no available data to validate, return early.
  if (!results_before_validation ||
      results_before_validation->results.empty()) {
    // An empty but non-null value indicates that the cache was updated
    // successfully, and no results were returned.
    if (results_before_validation && results_before_validation->results.empty())
      SetUseLongDelayInDriveSuggestQuery(profile_, true);

    EndDriveFilePathValidation(/*suggest_results=*/absl::nullopt,
                               DriveSuggestValidationStatus::kNoResults);
    return;
  }

  std::vector<std::string> item_ids;
  for (const auto& result : results_before_validation->results)
    item_ids.push_back(result.id);

  drive_service_->LocateFilesByItemIds(
      item_ids,
      base::BindOnce(&FileSuggestKeyedService::OnDriveFilePathsLocated,
                     weak_factory_.GetWeakPtr(),
                     results_before_validation->results));
}

void FileSuggestKeyedService::OnDriveFilePathsLocated(
    std::vector<ItemSuggestCache::Result> raw_suggest_results,
    absl::optional<std::vector<drivefs::mojom::FilePathOrErrorPtr>> paths) {
  // If validation fails, return early.
  if (!paths) {
    EndDriveFilePathValidation(
        /*suggest_results=*/absl::nullopt,
        DriveSuggestValidationStatus::kPathLocationFailed);
    return;
  }

  DCHECK_EQ(raw_suggest_results.size(), paths->size());

  std::vector<FileSuggestData> suggest_results;
  for (size_t index = 0; index < paths->size(); ++index) {
    const auto& path_or_error = paths->at(index);

    // Fail to validate the file path at `index`, so skip this loop iteration.
    if (!path_or_error->is_path())
      continue;

    suggest_results.emplace_back(
        ReparentToDriveMount(path_or_error->get_path(), drive_service_),
        raw_suggest_results[index].prediction_reason);
  }

  // Validation fails on each file, so return early.
  if (suggest_results.empty()) {
    EndDriveFilePathValidation(
        /*suggest_results=*/absl::nullopt,
        DriveSuggestValidationStatus::kAllFilesErrored);
    return;
  }

  // Publish the validated suggest results.
  EndDriveFilePathValidation(suggest_results,
                             DriveSuggestValidationStatus::kOk);
}

void FileSuggestKeyedService::EndDriveFilePathValidation(
    const absl::optional<std::vector<FileSuggestData>>& suggest_results,
    DriveSuggestValidationStatus validation_status) {
  // TODO(https://crbug.com/1356344): when the refactoring code is stable,
  // remove the obsolete histogram originally used by `ZeroStateDriveProvider`.
  base::UmaHistogramEnumeration(
      "Ash.Search.DriveFileSuggestDataValidation.Status", validation_status);

  // Notify observers of the fetched drive suggest results.
  on_drive_results_ready_callback_list_.Notify(suggest_results);
  DCHECK(on_drive_results_ready_callback_list_.empty());
}

}  // namespace app_list
