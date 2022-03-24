// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_GAMES_GAME_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_GAMES_GAME_PROVIDER_H_

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/games/stub_api.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {

// Provider for cloud gaming search.
class GameProvider : public SearchProvider, public GameIndexManager::Observer {
 public:
  GameProvider(Profile* profile, AppListControllerDelegate* list_controller);
  ~GameProvider() override;

  GameProvider(const GameProvider&) = delete;
  GameProvider& operator=(const GameProvider&) = delete;

  // GameIndexManager::Observer:
  void OnIndexUpdated(const absl::optional<GameIndex>& index) override;

  // SearchProvider:
  ash::AppListSearchResultType ResultType() const override;
  void Start(const std::u16string& query) override;

  void SetGameIndexForTest(const GameIndex& game_index);

 private:
  void OnSearchComplete(std::u16string query,
                        std::vector<std::pair<GameData, double>> matches);

  Profile* const profile_;
  AppListControllerDelegate* list_controller_;

  std::unique_ptr<GameIndexManager> game_index_manager_;
  absl::optional<GameIndex> game_index_;

  base::ScopedObservation<GameIndexManager, GameIndexManager::Observer>
      index_observer_{this};

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<GameProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_GAMES_GAME_PROVIDER_H_
