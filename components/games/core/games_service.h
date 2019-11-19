// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAMES_CORE_GAMES_SERVICE_H_
#define COMPONENTS_GAMES_CORE_GAMES_SERVICE_H_

#include "components/games/core/games_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace games {

class GamesService : public KeyedService {
 public:
  ~GamesService() override = default;

  virtual void GetHighlightedGame(HighlightedGameCallback callback) = 0;
};

}  // namespace games

#endif  // COMPONENTS_GAMES_CORE_GAMES_SERVICE_H_
