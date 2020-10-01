// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_DRIVE_QUICK_ACCESS_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_DRIVE_QUICK_ACCESS_PROVIDER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"

class Profile;

namespace app_list {

class SearchController;

// DriveQuickAccessProvider dispatches queries to extensions and fetches the
// results from them via chrome.launcherSearchProvider API.
//
// TODO(crbug.com/1034842): This is deprecated in favour of
// DriveZeroStateProvider. This class and related results classes can be
// deleted.
class DriveQuickAccessProvider : public SearchProvider,
                                 public drive::DriveIntegrationServiceObserver {
 public:
  DriveQuickAccessProvider(Profile* profile,
                           SearchController* search_controller);
  ~DriveQuickAccessProvider() override;

  // SearchProvider:
  void Start(const base::string16& query) override;
  void AppListShown() override;
  ash::AppListSearchResultType ResultType() override;

  // drive::DriveIntegrationServiceObserver:
  void OnFileSystemMounted() override;

 private:
  void GetQuickAccessItems(base::OnceCallback<void()> on_done);
  void OnGetQuickAccessItems(base::OnceCallback<void()> on_done,
                             drive::FileError error,
                             std::vector<drive::QuickAccessItem> drive_results);
  void SetResultsCache(
      base::OnceCallback<void()> on_done,
      const std::vector<drive::QuickAccessItem>& drive_results);
  void PublishResults(
      const std::vector<base::FilePath>& results,
      std::vector<file_manager::file_tasks::FileTasksNotifier::FileAvailability>
          availability);

  void StartSearchController();

  Profile* const profile_;
  drive::DriveIntegrationService* const drive_service_;
  file_manager::file_tasks::FileTasksNotifier* const file_tasks_notifier_;
  SearchController* const search_controller_;

  // Whether the suggested files experiment is enabled.
  const bool suggested_files_enabled_;

  // Whether we have sent at least one request to ItemSuggest to warm up the
  // results cache.
  bool have_warmed_up_cache_ = false;

  // Stores the last-returned results from the QuickAccess API.
  std::vector<drive::QuickAccessItem> results_cache_;

  base::TimeTicks query_start_time_;
  base::TimeTicks latest_fetch_start_time_;

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // Factory for general use.
  base::WeakPtrFactory<DriveQuickAccessProvider> weak_ptr_factory_{this};
  // Factory only for weak pointers for Drive QuickAccess API calls. Using two
  // factories allows in-flight API calls to be cancelled independently of other
  // tasks by invalidating only this factory's weak pointers.
  base::WeakPtrFactory<DriveQuickAccessProvider> quick_access_weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(DriveQuickAccessProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_DRIVE_QUICK_ACCESS_PROVIDER_H_
