// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SUGGEST_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SUGGEST_KEYED_SERVICE_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace app_list {
class DriveFileSuggestionProvider;
struct FileSuggestData;
enum class FileSuggestionType;
class ZeroStateDriveProvider;

// The keyed service that queries for the file suggestions (for both the drive
// files and local files) and exposes those data to consumers such as app list.
// TODO(https://crbug.com/1356347): move this service to a neutral place rather
// than leaving it under the app list directory.
class FileSuggestKeyedService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when file suggestions change.
    virtual void OnFileSuggestionUpdated(FileSuggestionType type) {}
  };

  explicit FileSuggestKeyedService(Profile* profile);
  FileSuggestKeyedService(const FileSuggestKeyedService&) = delete;
  FileSuggestKeyedService& operator=(const FileSuggestKeyedService&) = delete;
  ~FileSuggestKeyedService() override;

  // Requests to update the item suggest cache. Only used by the zero state
  // drive provider. Overridden for tests.
  // TODO(https://crbug.com/1356347): Now the app list relies on this service to
  // fetch the drive suggestion data. Meanwhile, this service relies on the app
  // list to trigger the item cache update. This cyclic dependency could be
  // confusing. The service should update the data cache by its own without
  // depending on the app list code.
  virtual void MaybeUpdateItemSuggestCache(
      base::PassKey<ZeroStateDriveProvider>);

  // Queries for the suggested files of the specified type and returns the
  // suggested file data, including file paths and suggestion reasons, through
  // the callback. The returned suggestions have been filtered by the file
  // last modification time. Only the files that have been modified more
  // recently than a threshold are returned.
  void GetSuggestFileData(
      FileSuggestionType type,
      base::OnceCallback<
          void(const absl::optional<std::vector<FileSuggestData>>&)> callback);

  // Adds/Removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if there is pending fetch on file suggestions.
  bool HasPendingSuggestionFetchForTest() const;

  DriveFileSuggestionProvider* drive_file_suggestion_provider_for_test() {
    return drive_file_suggestion_provider_.get();
  }

 private:
  // Called whenever a suggestion provider updates.
  void OnSuggestionProviderUpdated(FileSuggestionType type);

  // The provider of drive file suggestions.
  std::unique_ptr<DriveFileSuggestionProvider> drive_file_suggestion_provider_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<FileSuggestKeyedService> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SUGGEST_KEYED_SERVICE_H_
