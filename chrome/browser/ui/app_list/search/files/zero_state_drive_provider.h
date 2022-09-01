// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ui/app_list/search/files/file_result.h"
#include "chrome/browser/ui/app_list/search/files/item_suggest_cache.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace app_list {

class SearchController;

class ZeroStateDriveProvider : public SearchProvider,
                               public drive::DriveIntegrationServiceObserver,
                               public session_manager::SessionManagerObserver,
                               public chromeos::PowerManagerClient::Observer {
 public:
  ZeroStateDriveProvider(Profile* profile,
                         SearchController* search_controller,
                         drive::DriveIntegrationService* drive_service,
                         session_manager::SessionManager* session_manager,
                         std::unique_ptr<ItemSuggestCache> item_suggest_cache);
  ~ZeroStateDriveProvider() override;

  ZeroStateDriveProvider(const ZeroStateDriveProvider&) = delete;
  ZeroStateDriveProvider& operator=(const ZeroStateDriveProvider&) = delete;

  // drive::DriveIntegrationServiceObserver:
  void OnFileSystemMounted() override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // chromeos::PowerManagerClient::Observer:
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& proto) override;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StartZeroState() override;
  void ViewClosing() override;
  ash::AppListSearchResultType ResultType() const override;
  bool ShouldBlockZeroState() const override;

 private:
  void OnFilePathsLocated(
      absl::optional<std::vector<drivefs::mojom::FilePathOrErrorPtr>> paths);

  void SetSearchResults(
      std::vector<absl::optional<base::FilePath>> filtered_paths);

  std::unique_ptr<FileResult> MakeListResult(
      const base::FilePath& filepath,
      const absl::optional<std::string>& prediction_reason,
      const float relevance);

  // Callback for when the ItemSuggestCache updates its results.
  void OnCacheUpdated();

  // Requests an update from the ItemSuggestCache, but only if the call is long
  // enough after the provider was constructed. This helps ease resource
  // contention at login, and prevents the call from failing because Google auth
  // tokens haven't been set up yet. If the productivity launcher is disabled,
  // this does nothing.
  void MaybeUpdateCache();

  Profile* const profile_;
  drive::DriveIntegrationService* const drive_service_;
  session_manager::SessionManager* const session_manager_;

  std::unique_ptr<ItemSuggestCache> item_suggest_cache_;
  base::CallbackListSubscription item_suggest_subscription_;

  // The most recent results retrieved from |item_suggested_cache_|. This is
  // updated on a call to Start and is used only to store the results until
  // OnFilePathsLocated has finished.
  absl::optional<ItemSuggestCache::Results> cache_results_;

  const base::Time construction_time_;
  base::TimeTicks query_start_time_;

  // Whether or not the screen is off due to idling.
  bool screen_off_ = true;

  // A file needs to have been modified more recently than this to be considered
  // valid.
  const base::TimeDelta max_last_modified_time_;

  base::ScopedObservation<drive::DriveIntegrationService,
                          drive::DriveIntegrationServiceObserver>
      drive_observation_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<ZeroStateDriveProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_
