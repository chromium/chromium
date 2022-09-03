// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SUGGEST_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SUGGEST_KEYED_SERVICE_H_

#include "chrome/browser/ui/app_list/search/files/item_suggest_cache.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace app_list {

// The keyed service that queries for the file suggestions (for both the drive
// files and local files) and exposes those data to consumers such as app list.
// TODO(https://crbug.com/1356347): move this service to a neutral place rather
// than leaving it under the app list directory.
class FileSuggestKeyedService : public KeyedService {
 public:
  // The types of the managed suggestion data.
  enum class SuggestDataType {
    // The drive files' suggestion data.
    kItemSuggest
  };

  explicit FileSuggestKeyedService(Profile* profile);
  FileSuggestKeyedService(const FileSuggestKeyedService&) = delete;
  FileSuggestKeyedService& operator=(const FileSuggestKeyedService&) = delete;
  ~FileSuggestKeyedService() override;

  absl::optional<ItemSuggestCache::Results> GetSuggestData(
      SuggestDataType type);

  // Requests to update the data in `item_suggest_cache_`. Overridden for tests.
  // TODO(https://crbug.com/1356347): Now the app list relies on this service to
  // fetch the drive suggestion data. Meanwhile, this service relies on the app
  // list to trigger the item cache update. This cyclic dependency could be
  // confusing. The service should update the data cache by its own without
  // depending on the app list code.
  virtual void MaybeUpdateItemSuggestCache();

  // Registers a callback to be run whenever data in `item_suggest_cache_`
  // updated.
  base::CallbackListSubscription RegisterItemSuggestUpdateCallback(
      ItemSuggestCache::OnResultsCallback callback);

 private:
  // The drive client that fetches/exposes the drive file suggestions.
  std::unique_ptr<ItemSuggestCache> item_suggest_cache_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_FILE_SUGGEST_KEYED_SERVICE_H_
