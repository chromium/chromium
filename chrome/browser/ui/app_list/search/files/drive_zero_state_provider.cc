// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/drive_zero_state_provider.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "components/drive/file_errors.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace app_list {
namespace {

constexpr char kSchema[] = "drive_zero_state://";

// Given an absolute path representing a file in the user's Drive, returns a
// reparented version of the path within the user's drive fs mount.
base::FilePath ReparentToDriveMount(
    const base::FilePath& path,
    const drive::DriveIntegrationService* drive_service) {
  DCHECK(path.IsAbsolute());
  return drive_service->GetMountPointPath().Append(path.value());
}

}  // namespace

DriveZeroStateProvider::DriveZeroStateProvider(
    Profile* profile,
    SearchController* search_controller,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : profile_(profile),
      drive_service_(
          drive::DriveIntegrationServiceFactory::GetForProfile(profile)),
      file_tasks_notifier_(
          file_manager::file_tasks::FileTasksNotifier::GetForProfile(profile)),
      item_suggest_cache_(profile, std::move(url_loader_factory)),
      suggested_files_enabled_(app_list_features::IsSuggestedFilesEnabled()) {
  DCHECK(profile_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

DriveZeroStateProvider::~DriveZeroStateProvider() = default;

void DriveZeroStateProvider::OnFileSystemMounted() {
  if (have_warmed_up_cache_)
    return;
  have_warmed_up_cache_ = true;

  // TODO(crbug.com/1034842): Query ItemSuggest. We may need to call
  // SearchController::Start afterwards, or preferably could just publish the
  // results for this search provider.
}

void DriveZeroStateProvider::AppListShown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  item_suggest_cache_.UpdateCache();
}

ash::AppListSearchResultType DriveZeroStateProvider::ResultType() {
  return ash::AppListSearchResultType::kDriveQuickAccess;
}

void DriveZeroStateProvider::Start(const base::string16& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClearResultsSilently();

  // TODO(crbug.com/1034842): Add query latency metrics.

  // Exit in three cases:
  //  - this search has a non-empty query, we only handle zero-state.
  //  - drive fs isn't mounted, as we launch results via drive fs.
  //  - the |file_tasks_notifier_| is unavailable, as we stat files using it.
  const bool drive_fs_mounted = drive_service_ && drive_service_->IsMounted();
  if (!query.empty() || !drive_fs_mounted || !file_tasks_notifier_) {
    // TODO(crbug.com/1034842): Log error metrics.
    return;
  }

  // Cancel any in-flight queries for this provider.
  weak_factory_.InvalidateWeakPtrs();

  // Get the most recent results from the cache.
  cache_results_ = item_suggest_cache_.GetResults();
  if (!cache_results_) {
    // TODO(crbug.com/1034842): Log error metrics.
    return;
  }

  std::vector<std::string> item_ids;
  for (const auto& result : cache_results_->results) {
    item_ids.push_back(result.id);
  }

  drive_service_->LocateFilesByItemIds(
      item_ids, base::BindOnce(&DriveZeroStateProvider::OnFilePathsLocated,
                               weak_factory_.GetWeakPtr()));
}

void DriveZeroStateProvider::OnFilePathsLocated(
    base::Optional<std::vector<drivefs::mojom::FilePathOrErrorPtr>> paths) {
  if (!paths) {
    // TODO(crbug.com/1034842): Log error metrics.
    return;
  }
  DCHECK(cache_results_);
  DCHECK_EQ(cache_results_->results.size(), paths->size());

  // Assign scores to results by simply using their position in the results
  // list. The order of results from the ItemSuggest API is significant:
  // the first is better than the second, etc. Resulting scores are in [0, 1].
  const double total_items = static_cast<double>(paths->size());
  int item_index = 0;
  SearchProvider::Results provider_results;
  for (int i = 0; i < static_cast<int>(paths->size()); ++i) {
    const auto& path_or_error = paths.value()[i];
    if (path_or_error->is_error()) {
      // TODO(crbug.com/1034842): Log error metrics.
      continue;
    }

    const double score = 1.0 - (item_index / total_items);
    ++item_index;

    // TODO(crbug.com/1034842): Use |cache_results_| to attach the session id to
    // the result.

    provider_results.emplace_back(
        MakeResult(path_or_error->get_path(), score, /*is_chip=*/false));
    if (suggested_files_enabled_) {
      provider_results.emplace_back(
          MakeResult(path_or_error->get_path(), score, /*is_chip=*/true));
    }
  }

  cache_results_.reset();
  SwapResults(&provider_results);
}

std::unique_ptr<FileResult> DriveZeroStateProvider::MakeResult(
    const base::FilePath& filepath,
    const float relevance,
    const bool is_chip) {
  return std::make_unique<FileResult>(
      kSchema, ReparentToDriveMount(filepath, drive_service_),
      ash::AppListSearchResultType::kDriveQuickAccessChip,
      is_chip ? ash::SearchResultDisplayType::kChip
              : ash::SearchResultDisplayType::kList,
      relevance, profile_);
}

}  // namespace app_list
