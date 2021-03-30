// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/zero_state_drive_provider.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "components/drive/file_errors.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace app_list {
namespace {

// Schemas of result IDs for the results list and suggestion chips.
constexpr char kListSchema[] = "zero_state_drive://";
constexpr char kChipSchema[] = "drive_chip://";

// Outcome of a call to DriverZeroStateProvider::Start. These values persist to
// logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class Status {
  kOk = 0,
  kDriveFSNotMounted = 1,
  kNoResults = 2,
  kPathLocationFailed = 3,
  kAllFilesErrored = 4,
  kMaxValue = kAllFilesErrored,
};

void LogStatus(Status status) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.DriveZeroStateProvider.Status",
                            status);
}

bool IsSuggestedContentEnabled(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      chromeos::prefs::kSuggestedContentEnabled);
}

// Given an absolute path representing a file in the user's Drive, returns a
// reparented version of the path within the user's drive fs mount.
base::FilePath ReparentToDriveMount(
    const base::FilePath& path,
    const drive::DriveIntegrationService* drive_service) {
  DCHECK(!path.IsAbsolute());
  return drive_service->GetMountPointPath().Append(path.value());
}

}  // namespace

ZeroStateDriveProvider::ZeroStateDriveProvider(
    Profile* profile,
    SearchController* search_controller,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : profile_(profile),
      drive_service_(
          drive::DriveIntegrationServiceFactory::GetForProfile(profile)),
      item_suggest_cache_(profile, std::move(url_loader_factory)),
      suggested_files_enabled_(app_list_features::IsSuggestedFilesEnabled()) {
  DCHECK(profile_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // Warm the results cache if or when drivefs is mounted by fetching from the
  // Drive QuickAccess API. This is necessary only if the suggested files
  // experiment is enabled, so that results are ready for display in the
  // suggested chips on the first launcher open after login. To prevent
  // unnecessary queries to ItemSuggest, only warm the cache if the launcher has
  // been used before.
  const bool launcher_used = profile->GetPrefs()->GetBoolean(
      chromeos::prefs::kLauncherResultEverLaunched);
  const bool gate_on_use = base::GetFieldTrialParamByFeatureAsBool(
      app_list_features::kEnableSuggestedFiles, "gate_warm_on_launcher_use",
      true);
  const bool should_warm = !gate_on_use || launcher_used;
  if (suggested_files_enabled_ && drive_service_ && should_warm) {
    if (drive_service_->IsMounted()) {
      // Drivefs is mounted, so we can fetch results immediately.
      OnFileSystemMounted();
    } else {
      // Wait for DriveFS to be mounted, then fetch results. This happens in
      // OnFileSystemMounted.
      drive_service_->AddObserver(this);
    }
  }
  if (base::FeatureList::IsEnabled(
          app_list_features::kEnableLauncherSearchNormalization)) {
    normalizer_.emplace("zero_state_drive_provider", profile, 25);
  }
}

ZeroStateDriveProvider::~ZeroStateDriveProvider() {
  if (suggested_files_enabled_ && drive_service_)
    drive_service_->RemoveObserver(this);
}

void ZeroStateDriveProvider::OnFileSystemMounted() {
  // This method is called on login, and each time the device wakes from sleep.
  // We only want to warm the cache once.
  if (have_warmed_up_cache_)
    return;
  have_warmed_up_cache_ = true;

  item_suggest_cache_.UpdateCache();
}

void ZeroStateDriveProvider::AppListShown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  item_suggest_cache_.UpdateCache();
}

ash::AppListSearchResultType ZeroStateDriveProvider::ResultType() {
  return ash::AppListSearchResultType::kZeroStateDrive;
}

void ZeroStateDriveProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClearResultsSilently();

  // TODO(crbug.com/1034842): Add query latency metrics.

  // Exit in three cases:
  //  - this search has a non-empty query, we only handle zero-state.
  //  - drive fs isn't mounted, as we launch results via drive fs.
  const bool drive_fs_mounted = drive_service_ && drive_service_->IsMounted();
  if (!query.empty()) {
    return;
  } else if (!drive_fs_mounted) {
    LogStatus(Status::kDriveFSNotMounted);
    return;
  }

  query_start_time_ = base::TimeTicks::Now();

  // Cancel any in-flight queries for this provider.
  weak_factory_.InvalidateWeakPtrs();

  // Get the most recent results from the cache.
  cache_results_ = item_suggest_cache_.GetResults();
  if (!cache_results_) {
    LogStatus(Status::kNoResults);
    return;
  }

  std::vector<std::string> item_ids;
  for (const auto& result : cache_results_->results) {
    item_ids.push_back(result.id);
  }

  drive_service_->LocateFilesByItemIds(
      item_ids, base::BindOnce(&ZeroStateDriveProvider::OnFilePathsLocated,
                               weak_factory_.GetWeakPtr()));
}

void ZeroStateDriveProvider::OnFilePathsLocated(
    base::Optional<std::vector<drivefs::mojom::FilePathOrErrorPtr>> paths) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!paths) {
    LogStatus(Status::kPathLocationFailed);
    return;
  }

  DCHECK(cache_results_);
  DCHECK_EQ(cache_results_->results.size(), paths->size());

  // Assign scores to results by simply using their position in the results
  // list. The order of results from the ItemSuggest API is significant:
  // the first is better than the second, etc. Resulting scores are in [0, 1].
  const double total_items = static_cast<double>(paths->size());
  int item_index = 0;
  bool all_files_errored = true;
  SearchProvider::Results provider_results;
  for (int i = 0; i < static_cast<int>(paths->size()); ++i) {
    const auto& path_or_error = paths.value()[i];
    if (path_or_error->is_error()) {
      continue;
    } else {
      all_files_errored = false;
    }

    const double score = 1.0 - (item_index / total_items);
    ++item_index;

    // TODO(crbug.com/1034842): Use |cache_results_| to attach the session id to
    // the result.

    provider_results.emplace_back(
        MakeListResult(path_or_error->get_path(), score));
    if (suggested_files_enabled_ && IsSuggestedContentEnabled(profile_)) {
      provider_results.emplace_back(
          MakeChipResult(path_or_error->get_path(), score));
    }
  }

  // We expect some files to error sometimes, but we're mainly interested in
  // when all of the files error at once. This also keeps the bucket proportion
  // of the status metric meaningful.
  if (all_files_errored) {
    LogStatus(Status::kAllFilesErrored);
    return;
  }

  cache_results_.reset();

  if (normalizer_.has_value()) {
    normalizer_->RecordResults(provider_results);
    normalizer_->NormalizeResults(&provider_results);
  }

  SwapResults(&provider_results);

  LogStatus(Status::kOk);
  UMA_HISTOGRAM_TIMES("Apps.AppList.DriveZeroStateProvider.Latency",
                      base::TimeTicks::Now() - query_start_time_);
}

std::unique_ptr<FileResult> ZeroStateDriveProvider::MakeListResult(
    const base::FilePath& filepath,
    const float relevance) {
  return std::make_unique<FileResult>(
      kListSchema, ReparentToDriveMount(filepath, drive_service_),
      ash::AppListSearchResultType::kZeroStateDrive,
      ash::SearchResultDisplayType::kList, relevance, profile_);
}

std::unique_ptr<FileResult> ZeroStateDriveProvider::MakeChipResult(
    const base::FilePath& filepath,
    const float relevance) {
  return std::make_unique<FileResult>(
      kChipSchema, ReparentToDriveMount(filepath, drive_service_),
      ash::AppListSearchResultType::kDriveChip,
      ash::SearchResultDisplayType::kChip, relevance, profile_);
}

}  // namespace app_list
