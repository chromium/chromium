// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/drive_quick_access_provider.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/ui/app_list/search/drive_quick_access_result.h"

namespace app_list {
namespace {

constexpr int kMaxItems = 5;

// Error codes returned by the Drive QuickAccess API call. These values persist
// to logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class FileError {
  kUnknown = 0,
  kFileErrorOk = 1,
  kFileErrorFailed = 2,
  kFileErrorIn_use = 3,
  kFileErrorExists = 4,
  kFileErrorNotFound = 5,
  kFileErrorAccessDenied = 6,
  kFileErrorTooManyOpened = 7,
  kFileErrorNoMemory = 8,
  kFileErrorNoServerSpace = 9,
  kFileErrorNotADirectory = 10,
  kFileErrorInvalidOperation = 11,
  kFileErrorSecurity = 12,
  kFileErrorAbort = 13,
  kFileErrorNotAFile = 14,
  kFileErrorNotEmpty = 15,
  kFileErrorInvalidUrl = 16,
  kFileErrorNoConnection = 17,
  kFileErrorNoLocalSpace = 18,
  kFileErrorServiceUnavailable = 19,
  kMaxValue = kFileErrorServiceUnavailable,
};

void LogFileError(FileError error) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.DriveQuickAccessProvider.FileError",
                            error);
}

void LogDriveFSMounted(bool mounted) {
  UMA_HISTOGRAM_BOOLEAN("Apps.AppList.DriveQuickAccessProvider.DriveFSMounted",
                        mounted);
}

// Given a vector of QuickAccessItems, return only those that exist on-disk.
std::vector<drive::QuickAccessItem> FilterResults(
    const drive::DriveIntegrationService* drive_service,
    const std::vector<drive::QuickAccessItem>& drive_results) {
  std::vector<drive::QuickAccessItem> valid_results;
  int num_filtered = 0;
  for (const auto& result : drive_results) {
    if (base::PathExists(
            drive_service->GetMountPointPath().Append(result.path))) {
      valid_results.emplace_back(result);
      ++num_filtered;
    }
  }
  UMA_HISTOGRAM_EXACT_LINEAR(
      "Apps.AppList.DriveQuickAccessProvider.ValidResults", num_filtered,
      2 * kMaxItems);
  return valid_results;
}

}  // namespace

DriveQuickAccessProvider::DriveQuickAccessProvider(Profile* profile)
    : profile_(profile),
      drive_service_(
          drive::DriveIntegrationServiceFactory::GetForProfile(profile)) {
  DCHECK(profile_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

DriveQuickAccessProvider::~DriveQuickAccessProvider() = default;

void DriveQuickAccessProvider::Start(const base::string16& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClearResultsSilently();
  if (!query.empty())
    return;

  query_start_time_ = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES(
      "Apps.AppList.DriveQuickAccessProvider.TimeFromFetchToZeroStateStart",
      base::TimeTicks::Now() - latest_fetch_start_time_);

  // Results are launched via DriveFS, so DriveFS must be mounted.
  bool drive_fs_mounted = drive_service_ && drive_service_->IsMounted();
  LogDriveFSMounted(drive_fs_mounted);
  if (!drive_fs_mounted)
    return;

  // If there are no items in the cache, the previous call may have failed so
  // retry. We return no results in this case, because waiting for the new
  // results would introduce too much latency.
  UMA_HISTOGRAM_BOOLEAN("Apps.AppList.DriveQuickAccessProvider.CacheEmpty",
                        results_cache_.empty());
  if (results_cache_.empty()) {
    GetQuickAccessItems();
    return;
  }

  SearchProvider::Results results;
  for (const auto& result : results_cache_) {
    results.emplace_back(std::make_unique<DriveQuickAccessResult>(
        drive_service_->GetMountPointPath().Append(result.path),
        result.confidence, profile_));
  }
  UMA_HISTOGRAM_TIMES("Apps.AppList.DriveQuickAccessProvider.Latency",
                      base::TimeTicks::Now() - query_start_time_);
  SwapResults(&results);
}

void DriveQuickAccessProvider::AppListShown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetQuickAccessItems();
}

void DriveQuickAccessProvider::GetQuickAccessItems() {
  LogDriveFSMounted(drive_service_);
  if (!drive_service_)
    return;

  // Invalidate weak pointers for existing callbacks to the Quick Access API.
  weak_factory_.InvalidateWeakPtrs();
  latest_fetch_start_time_ = base::TimeTicks::Now();

  drive_service_->GetQuickAccessItems(
      kMaxItems,
      base::BindOnce(&DriveQuickAccessProvider::OnGetQuickAccessItems,
                     weak_factory_.GetWeakPtr()));
}

void DriveQuickAccessProvider::OnGetQuickAccessItems(
    drive::FileError error,
    std::vector<drive::QuickAccessItem> drive_results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error == drive::FILE_ERROR_OK) {
    UMA_HISTOGRAM_TIMES(
        "Apps.AppList.DriveQuickAccessProvider.GetQuickAccessItemsLatency",
        base::TimeTicks::Now() - latest_fetch_start_time_);
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Apps.AppList.DriveQuickAccessProvider.ApiResults",
        drive_results.size(), 2 * kMaxItems);
  }

  // Error codes are in the range [-18, 0], convert to the histogram enum values
  // in [1,19]. If the value is outside the expected range, log
  // FileError::kUnknown.
  const int error_int = 1 - static_cast<int>(error);
  if (0 <= error_int && error_int <= static_cast<int>(FileError::kMaxValue))
    LogFileError(static_cast<FileError>(error_int));
  else
    LogFileError(FileError::kUnknown);

  // An empty |drive_results| is likely caused by a failed call to ItemSuggest,
  // so don't replace the cache.
  if (!drive_results.empty()) {
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&FilterResults, drive_service_, drive_results),
        base::BindOnce(&DriveQuickAccessProvider::SetResultsCache,
                       weak_factory_.GetWeakPtr()));
  }
}

void DriveQuickAccessProvider::SetResultsCache(
    const std::vector<drive::QuickAccessItem>& drive_results) {
  results_cache_ = std::move(drive_results);
}

}  // namespace app_list
