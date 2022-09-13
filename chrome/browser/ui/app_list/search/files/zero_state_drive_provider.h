// Copyright 2020 The Chromium Authors
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
#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service.h"
#include "chrome/browser/ui/app_list/search/files/item_suggest_cache.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace app_list {
struct FileSuggestData;
class SearchController;

class ZeroStateDriveProvider : public SearchProvider,
                               public drive::DriveIntegrationServiceObserver,
                               public session_manager::SessionManagerObserver,
                               public chromeos::PowerManagerClient::Observer,
                               public FileSuggestKeyedService::Observer {
 public:
  ZeroStateDriveProvider(Profile* profile,
                         SearchController* search_controller,
                         drive::DriveIntegrationService* drive_service,
                         session_manager::SessionManager* session_manager);
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
  // Called when file suggestion data are fetched from the service.
  void OnSuggestFileDataFetched(
      absl::optional<std::vector<FileSuggestData>> suggest_results);

  // Builds the search results from file suggestions then publishes the results.
  void SetSearchResults(std::vector<FileSuggestData> suggest_results);

  std::unique_ptr<FileResult> MakeListResult(
      const base::FilePath& filepath,
      const absl::optional<std::string>& prediction_reason,
      const float relevance);

  // Requests an update from the ItemSuggestCache, but only if the call is long
  // enough after the provider was constructed. This helps ease resource
  // contention at login, and prevents the call from failing because Google auth
  // tokens haven't been set up yet. If the productivity launcher is disabled,
  // this does nothing.
  void MaybeUpdateCache();

  // FileSuggestKeyedService::Observer:
  void OnFileSuggestionUpdated(
      FileSuggestKeyedService::SuggestionType type) override;

  Profile* const profile_;
  drive::DriveIntegrationService* const drive_service_;
  session_manager::SessionManager* const session_manager_;

  const base::raw_ptr<FileSuggestKeyedService> file_suggest_service_;

  const base::Time construction_time_;
  base::TimeTicks query_start_time_;

  // Whether or not the screen is off due to idling.
  bool screen_off_ = true;

  base::ScopedObservation<drive::DriveIntegrationService,
                          drive::DriveIntegrationServiceObserver>
      drive_observation_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_observation_{this};
  base::ScopedObservation<FileSuggestKeyedService,
                          FileSuggestKeyedService::Observer>
      file_suggest_service_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ZeroStateDriveProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_ZERO_STATE_DRIVE_PROVIDER_H_
