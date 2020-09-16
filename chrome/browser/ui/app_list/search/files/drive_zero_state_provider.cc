// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/drive_zero_state_provider.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/ui/app_list/search/drive_quick_access_chip_result.h"
#include "chrome/browser/ui/app_list/search/drive_quick_access_result.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace app_list {

DriveZeroStateProvider::DriveZeroStateProvider(
    Profile* profile,
    SearchController* search_controller)
    : profile_(profile),
      drive_service_(
          drive::DriveIntegrationServiceFactory::GetForProfile(profile)),
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

ash::AppListSearchResultType DriveZeroStateProvider::ResultType() {
  return ash::AppListSearchResultType::kDriveQuickAccess;
}

void DriveZeroStateProvider::Start(const base::string16& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClearResultsSilently();
  if (!query.empty())
    return;

  // TODO(crbug.com/1034842): Convert results cache into search results.
}

void DriveZeroStateProvider::AppListShown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/1034842): Query ItemSuggest, consider rate-limiting.
}

}  // namespace app_list
