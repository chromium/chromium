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
#include "chrome/browser/ui/app_list/search/search_provider.h"

class Profile;

namespace app_list {

// DriveQuickAccessProvider dispatches queries to extensions and fetches the
// results from them via chrome.launcherSearchProvider API.
class DriveQuickAccessProvider : public SearchProvider {
 public:
  explicit DriveQuickAccessProvider(Profile* profile);
  ~DriveQuickAccessProvider() override;

  // SearchProvider:
  void Start(const base::string16& query) override;
  void AppListShown() override;

 private:
  void GetQuickAccessItems();
  void OnGetQuickAccessItems(drive::FileError error,
                             std::vector<drive::QuickAccessItem> drive_results);
  void SetResultsCache(
      const std::vector<drive::QuickAccessItem>& drive_results);

  Profile* const profile_;
  drive::DriveIntegrationService* const drive_service_;
  // Stores the last-returned results from the QuickAccess API.
  std::vector<drive::QuickAccessItem> results_cache_;

  base::TimeTicks query_start_time_;
  base::TimeTicks latest_fetch_start_time_;

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<DriveQuickAccessProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DriveQuickAccessProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_DRIVE_QUICK_ACCESS_PROVIDER_H_
