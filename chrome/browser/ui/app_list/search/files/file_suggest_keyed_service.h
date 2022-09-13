// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SUGGEST_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SUGGEST_KEYED_SERVICE_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/files/item_suggest_cache.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace drive {
class DriveIntegrationService;
}  // namespace drive

namespace app_list {
enum class DriveSuggestValidationStatus;
struct FileSuggestData;
class ZeroStateDriveProvider;

// The keyed service that queries for the file suggestions (for both the drive
// files and local files) and exposes those data to consumers such as app list.
// TODO(https://crbug.com/1356347): move this service to a neutral place rather
// than leaving it under the app list directory.
class FileSuggestKeyedService : public KeyedService {
 public:
  using GetSuggestDataCallback =
      base::OnceCallback<void(absl::optional<std::vector<FileSuggestData>>)>;

  // The types of the managed suggestion data.
  enum class SuggestionType {
    // The drive file suggestion.
    kItemSuggest
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when file suggestions change.
    virtual void OnFileSuggestionUpdated(SuggestionType type) {}
  };

  explicit FileSuggestKeyedService(Profile* profile);
  FileSuggestKeyedService(const FileSuggestKeyedService&) = delete;
  FileSuggestKeyedService& operator=(const FileSuggestKeyedService&) = delete;
  ~FileSuggestKeyedService() override;

  // Queries for the suggested files of the specified type and returns the
  // suggested file data, including file paths and suggestion reasons, through
  // the callback. The returned suggestions have been filtered by the file
  // last modification time. Only the files that have been modified more
  // recently than a threshold are returned.
  void GetSuggestFileData(SuggestionType type, GetSuggestDataCallback callback);

  // Adds/Removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Requests to update the data in `item_suggest_cache_`. Only used by the zero
  // state drive provider. Overridden for tests.
  // TODO(https://crbug.com/1356347): Now the app list relies on this service to
  // fetch the drive suggestion data. Meanwhile, this service relies on the app
  // list to trigger the item cache update. This cyclic dependency could be
  // confusing. The service should update the data cache by its own without
  // depending on the app list code.
  virtual void MaybeUpdateItemSuggestCache(
      base::PassKey<ZeroStateDriveProvider>);

  ItemSuggestCache* item_suggest_cache_for_test() {
    return item_suggest_cache_.get();
  }

 private:
  // Drive file related member functions ---------------------------------------
  // TODO(https://crbug.com/1360992): move these members to a separate class.

  // Called whenever `item_suggest_cache_` updates.
  void OnItemSuggestCacheUpdated();

  // Handles `GetSuggestFileData()` for drive files.
  void GetDriveSuggestFileData(GetSuggestDataCallback callback);

  // Called when locating drive files through the drive service is completed.
  // Returns the location result through `paths`. `raw_suggest_results` is the
  // file suggestion data before validation.
  void OnDriveFilePathsLocated(
      std::vector<ItemSuggestCache::Result> raw_suggest_results,
      absl::optional<std::vector<drivefs::mojom::FilePathOrErrorPtr>> paths);

  // Ends the validation on drive suggestion file paths and publishes the
  // result.
  void EndDriveFilePathValidation(
      DriveSuggestValidationStatus validation_status,
      const absl::optional<std::vector<FileSuggestData>>& suggest_results);

  const base::raw_ptr<Profile> profile_;

  // Drive file related data members -------------------------------------------
  // TODO(https://crbug.com/1360992): move these members to a separate class.

  const base::raw_ptr<drive::DriveIntegrationService> drive_service_;

  // The drive client from which the raw suggest data (i.e. the data before
  // validation) is fetched.
  std::unique_ptr<ItemSuggestCache> item_suggest_cache_;

  // Guards the callback registered on `item_suggest_cache_`.
  base::CallbackListSubscription item_suggest_subscription_;

  // The callbacks that run when the drive suggest results are ready.
  // Use a callback list to handle the edge case that multiple data consumers
  // wait for the drive suggest results.
  base::OnceCallbackList<GetSuggestDataCallback::RunType>
      on_drive_results_ready_callback_list_;

  // A drive file needs to have been modified more recently than this to be
  // considered valid.
  const base::TimeDelta drive_file_max_last_modified_time_;

  base::ObserverList<Observer> observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to post the task to filter drive suggestion results.
  scoped_refptr<base::SequencedTaskRunner> drive_result_filter_task_runner_;

  // Used to guard the calling to get drive suggestion results.
  base::WeakPtrFactory<FileSuggestKeyedService> drive_result_weak_factory_{
      this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SUGGEST_KEYED_SERVICE_H_
