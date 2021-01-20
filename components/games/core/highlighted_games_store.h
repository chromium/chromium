// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAMES_CORE_HIGHLIGHTED_GAMES_STORE_H_
#define COMPONENTS_GAMES_CORE_HIGHLIGHTED_GAMES_STORE_H_

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "components/games/core/data_files_parser.h"
#include "components/games/core/games_types.h"
#include "components/games/core/proto/games_catalog.pb.h"
#include "components/games/core/proto/highlighted_games.pb.h"

namespace games {

// This feature store is in charge of parsing the HighlightedGamesResponse proto
// that is downloaded on the client via the Games Data Files Omaha component,
// and then calculating and caching the currently highlighted game.
class HighlightedGamesStore {
 public:
  explicit HighlightedGamesStore(base::Clock* clock);

  // For unit tests.
  explicit HighlightedGamesStore(
      std::unique_ptr<DataFilesParser> data_files_parser,
      base::Clock* clock);

  virtual ~HighlightedGamesStore();

  // Given a catalog, this function will start a flow where the current store
  // may update its cache with the latest highlighted game. It may read the
  // HighlightedGamesResponse from disk and use it with the catalog to resolve
  // the game.
  virtual void ProcessAsync(const base::FilePath& install_dir,
                            const GamesCatalog& catalog,
                            base::OnceClosure done_callback);

  // Allows a caller to make the store reply to the pending callback if it's
  // already caching a valid highlighted game.
  virtual bool TryRespondFromCache();

  // Allows a callee to verify if the store already has the currently
  // highlighted game cache.
  virtual base::Optional<Game> TryGetFromCache();

  // Sets a callback to be invoked at the end of the next flow started by
  // ProcessAsync. We only hold one pending callback at a time.
  virtual void SetPendingCallback(HighlightedGameCallback callback);

  // Will invoke any pending callback with the given failure code.
  virtual void HandleCatalogFailure(ResponseCode failure_code);

 private:
  std::unique_ptr<HighlightedGamesResponse> GetHighlightedGamesResponse(
      const base::FilePath& install_dir);

  void OnHighlightedGamesResponseParsed(
      base::OnceClosure done_callback,
      const GamesCatalog& catalog,
      std::unique_ptr<HighlightedGamesResponse> response);

  void RespondAndInvoke(ResponseCode code,
                        const Game& game,
                        base::OnceClosure done_callback);

  void Respond(ResponseCode code, const Game& game);

  bool IsCurrent(const HighlightedGame& highlighted_game);

  std::unique_ptr<DataFilesParser> data_files_parser_;

  // Task runner delegating tasks to the ThreadPool.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::Clock* clock_;
  std::unique_ptr<HighlightedGame> cached_highlighted_game_;
  std::unique_ptr<Game> cached_game_;
  base::Optional<HighlightedGameCallback> pending_callback_;

  base::WeakPtrFactory<HighlightedGamesStore> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HighlightedGamesStore);
};

}  // namespace games

#endif  // COMPONENTS_GAMES_CORE_HIGHLIGHTED_GAMES_STORE_H_
