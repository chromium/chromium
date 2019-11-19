// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/games_service_impl.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "components/games/core/games_prefs.h"
#include "components/games/core/proto/game.pb.h"

namespace games {

GamesServiceImpl::GamesServiceImpl(PrefService* prefs) : prefs_(prefs) {}

GamesServiceImpl::~GamesServiceImpl() = default;

void GamesServiceImpl::GetHighlightedGame(HighlightedGameCallback callback) {
  // If we don't have the install dir pref, then the Games component wasn't
  // downloaded and we cannot provide the surface with a highlighted game.
  base::FilePath data_file_path;
  if (!prefs::TryGetInstallDirPath(prefs_, &data_file_path)) {
    // TODO crbug.com/1018201: Add callback error handling.
    return;
  }

  auto game_proto = std::make_unique<Game>();
  game_proto->set_title("Some Game!");
  std::move(callback).Run(std::move(game_proto));
}

}  // namespace games
