// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ui/app_list/search/files/file_result.h"
#include "chrome/browser/ui/app_list/search/files/item_suggest_cache.h"
#include "chrome/browser/ui/app_list/search/score_normalizer/score_normalizer.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"

class Profile;

namespace app_list {

class SearchController;

class ZeroStateDriveProvider : public SearchProvider,
                               public drive::DriveIntegrationServiceObserver {
 public:
  ZeroStateDriveProvider(
      Profile* profile,
      SearchController* search_controller,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~ZeroStateDriveProvider() override;

  ZeroStateDriveProvider(const ZeroStateDriveProvider&) = delete;
  ZeroStateDriveProvider& operator=(const ZeroStateDriveProvider&) = delete;

  // drive::DriveIntegrationServiceObserver:
  void OnFileSystemMounted() override;

  // SearchProvider:
  void AppListShown() override;
  ash::AppListSearchResultType ResultType() override;
  void Start(const std::u16string& query) override;

 private:
  void OnFilePathsLocated(
      base::Optional<std::vector<drivefs::mojom::FilePathOrErrorPtr>> paths);

  std::unique_ptr<FileResult> MakeListResult(const base::FilePath& filepath,
                                             const float relevance);
  std::unique_ptr<FileResult> MakeChipResult(const base::FilePath& filepath,
                                             const float relevance);

  Profile* const profile_;
  drive::DriveIntegrationService* const drive_service_;

  ItemSuggestCache item_suggest_cache_;

  // The most recent results retrieved from |item_suggested_cache_|. This is
  // updated on a call to Start and is used only to store the results until
  // OnFilePathsLocated has finished.
  base::Optional<ItemSuggestCache::Results> cache_results_;

  base::TimeTicks query_start_time_;

  // Whether suggested files feature is enabled. True if both the experiment is
  // enabled, and the suggested content toggle is enabled.
  const bool suggested_files_enabled_;

  // The normalizer normalizes the relevance scores of Results
  base::Optional<ScoreNormalizer> normalizer_;

  // Whether we have sent at least one request to ItemSuggest to warm up the
  // results cache.
  bool have_warmed_up_cache_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<ZeroStateDriveProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_
