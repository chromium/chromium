// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAMES_CORE_GAMES_SERVICE_IMPL_H_
#define COMPONENTS_GAMES_CORE_GAMES_SERVICE_IMPL_H_

#include <memory>

#include "base/barrier_closure.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/games/core/catalog_store.h"
#include "components/games/core/games_service.h"
#include "components/games/core/games_types.h"
#include "components/games/core/highlighted_games_store.h"
#include "components/prefs/pref_service.h"

namespace games {

class GamesServiceImpl : public GamesService {
 public:
  explicit GamesServiceImpl(
      std::unique_ptr<CatalogStore> catalog_store,
      std::unique_ptr<HighlightedGamesStore> highlighted_games_store,
      PrefService* prefs);
  ~GamesServiceImpl() override;

  // Sets a callback to asynchronously retrieve the currently highlighted game.
  // The callback will be invoked on the next GenerateHub run.
  void SetHighlightedGameCallback(HighlightedGameCallback callback) override;

  // Starts processing data from the component and asynchronously invokes
  // callbacks that have been set.
  void GenerateHub() override;

  bool is_updating() { return is_updating_; }

 private:
  // This function starts a flow where we'll parse the catalog from disk and get
  // every feature store to update themselves. This is done to reduce the amount
  // of times that we need to read the catalog from disk; while we have the
  // catalog in memory, we're making the most of it. Stores are in charge of
  // decided whether they need to update or not.
  void UpdateStores();

  void OnCatalogReceived(ResponseCode code);

  void DoneUpdating();

  bool IsComponentInstalled();

  void HandleFailure(ResponseCode code);

  // In charge of parsing and temporarily caching the GamesCatalog that we have
  // on disk. Its cache will be cleared when other stores are done updating.
  std::unique_ptr<CatalogStore> catalog_store_;

  // In charge of calculating and caching the highlighted game.
  std::unique_ptr<HighlightedGamesStore> highlighted_games_store_;

  // Will outlive the current instance.
  PrefService* prefs_;

  // Cached data files installation directory. Will be used by the stores to
  // find their data files.
  std::unique_ptr<const base::FilePath> cached_data_files_dir_;

  // Reset on every run, this barrier closure will be used to ensure that the
  // service knows when all the feature stores are done updating themselves.
  base::RepeatingClosure barrier_closure_;

  bool is_updating_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GamesServiceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GamesServiceImpl);
};

}  // namespace games

#endif  // COMPONENTS_GAMES_CORE_GAMES_SERVICE_IMPL_H_
