// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_DRIVE_ZERO_STATE_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_DRIVE_ZERO_STATE_PROVIDER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/ui/app_list/search/files/item_suggest_cache.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"

class Profile;

namespace app_list {

class SearchController;

class DriveZeroStateProvider : public SearchProvider,
                               public drive::DriveIntegrationServiceObserver {
 public:
  DriveZeroStateProvider(
      Profile* profile,
      SearchController* search_controller,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~DriveZeroStateProvider() override;

  DriveZeroStateProvider(const DriveZeroStateProvider&) = delete;
  DriveZeroStateProvider& operator=(const DriveZeroStateProvider&) = delete;

  // SearchProvider:
  void Start(const base::string16& query) override;
  void AppListShown() override;
  ash::AppListSearchResultType ResultType() override;

  // drive::DriveIntegrationServiceObserver:
  void OnFileSystemMounted() override;

 private:
  Profile* const profile_;
  drive::DriveIntegrationService* const drive_service_;

  ItemSuggestCache item_suggest_cache_;

  // Whether the suggested files experiment is enabled.
  const bool suggested_files_enabled_;

  // Whether we have sent at least one request to ItemSuggest to warm up the
  // results cache.
  bool have_warmed_up_cache_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<DriveZeroStateProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_DRIVE_ZERO_STATE_PROVIDER_H_
