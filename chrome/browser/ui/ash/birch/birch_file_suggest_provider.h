// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_FILE_SUGGEST_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_FILE_SUGGEST_PROVIDER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace ash {

struct BirchFileItem;

// Manages observing file suggestion changes for the birch feature. Fetched
// file suggest items are send to the `BirchModel` to be stored.
class ASH_EXPORT BirchFileSuggestProvider
    : public FileSuggestKeyedService::Observer {
 public:
  explicit BirchFileSuggestProvider(Profile* profile);
  BirchFileSuggestProvider(const BirchFileSuggestProvider&) = delete;
  BirchFileSuggestProvider& operator=(const BirchFileSuggestProvider&) = delete;
  ~BirchFileSuggestProvider() override;

  // FileSuggestKeyedService::Observer:
  void OnFileSuggestionUpdated(FileSuggestionType type) override;

  void OnSuggestedFileDataUpdated(
      const absl::optional<std::vector<FileSuggestData>>& suggest_results);

  void set_file_suggest_service_for_test(
      FileSuggestKeyedService* suggest_service) {
    file_suggest_service_ = suggest_service;
  }

 private:
  void OnFileInfoRetrieved(std::vector<BirchFileItem> file_items);

  raw_ptr<FileSuggestKeyedService> file_suggest_service_;

  base::ScopedObservation<FileSuggestKeyedService,
                          FileSuggestKeyedService::Observer>
      file_suggest_service_observation_{this};

  base::WeakPtrFactory<BirchFileSuggestProvider> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_FILE_SUGGEST_PROVIDER_H_
