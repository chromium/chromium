// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_KEYED_SERVICE_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace ash {

struct BirchFileSuggestion {
  BirchFileSuggestion(base::FilePath new_file_path,
                      const absl::optional<base::Time> new_last_modified,
                      const absl::optional<base::Time> new_last_accessed);
  BirchFileSuggestion(BirchFileSuggestion&&);
  BirchFileSuggestion(const BirchFileSuggestion&);
  BirchFileSuggestion& operator=(const BirchFileSuggestion&);
  ~BirchFileSuggestion();

  base::FilePath file_path;
  absl::optional<base::Time> last_modified;
  absl::optional<base::Time> last_accessed;
};

// A keyed service which is used to manage data providers for the birch feature.
// Fetched data will be sent to the `BirchModel` to be stored.
class BirchKeyedService : public KeyedService,
                          public FileSuggestKeyedService::Observer {
 public:
  explicit BirchKeyedService(Profile* profile);
  BirchKeyedService(const BirchKeyedService&) = delete;
  BirchKeyedService& operator=(const BirchKeyedService&) = delete;
  ~BirchKeyedService() override;

  // FileSuggestKeyedService::Observer:
  void OnFileSuggestionUpdated(FileSuggestionType type) override;

  void OnSuggestedFileDataUpdated(
      const absl::optional<std::vector<FileSuggestData>>& suggest_results);

 private:
  void OnFileInfoRetrieved(std::vector<BirchFileSuggestion> file_suggestions);

  const raw_ptr<FileSuggestKeyedService, ExperimentalAsh> file_suggest_service_;

  base::ScopedObservation<FileSuggestKeyedService,
                          FileSuggestKeyedService::Observer>
      file_suggest_service_observation_{this};

  base::WeakPtrFactory<BirchKeyedService> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_KEYED_SERVICE_H_
