// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_

#include <string>
#include <vector>

#include "ash/components/drivefs/mojom/drivefs.mojom.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ui/app_list/search/files/file_result.h"
#include "chrome/browser/ui/app_list/search/files/item_suggest_cache.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
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
  ZeroStateDriveProvider(
      Profile* profile,
      SearchController* search_controller,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
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

  void set_session_manager_for_testing(
      session_manager::SessionManager* session_manager) {
    session_manager_ = session_manager;
  }

  // The minimum time between hypothetical queries.
  // These values persist to logs. Entries should not be renumbered and numeric
  // values should never be reused.
  enum class ThrottleInterval {
    kUnknown = 0,
    kFiveMinutes = 1,
    kTenMinutes = 2,
    kFifteenMinutes = 3,
    kThirtyMinutes = 4,
    kMaxValue = kThirtyMinutes,
  };

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
  // tokens haven't been set up yet.
  void MaybeUpdateCache();

  // We are intending to change the triggers of queries to ItemSuggest, but
  // first want to know the QPS impact of the change. This method records
  // metrics to track how many queries we will send under the proposed new
  // logic. It does not actually make any queries. The new logic is as follows:
  // - Query on login and wake from sleep.
  // - Query on every launcher open.
  // - Query at most every 5, 10 or 15 minutes. These are tracked separately in
  //   the metrics.
  void MaybeLogHypotheticalQuery();

  Profile* const profile_;
  drive::DriveIntegrationService* const drive_service_;
  session_manager::SessionManager* session_manager_;

  const base::Time construction_time_;

  ItemSuggestCache item_suggest_cache_;

  // The most recent results retrieved from |item_suggested_cache_|. This is
  // updated on a call to Start and is used only to store the results until
  // OnFilePathsLocated has finished.
  absl::optional<ItemSuggestCache::Results> cache_results_;

  base::TimeTicks query_start_time_;

  // The time we last logged a hypothetical query, keyed by the minimum time
  // between queries in minutes (5, 10 or 15 minutes). See
  // MaybeLogHypotheticalQuery for details.
  std::map<int, base::TimeTicks> last_hypothetical_query_;

  // Whether or not the screen is off due to idling. Used for logging a
  // hypothetical query on wake.
  bool screen_off_ = true;

  // A file needs to have been modified more recently than this to be considered
  // valid.
  const base::TimeDelta max_last_modified_time_;

  // Whether or not zero-state drive files are enabled. True iff the
  // productivity launcher is enabled.
  const bool enabled_;

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
