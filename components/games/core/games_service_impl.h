// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAMES_CORE_GAMES_SERVICE_IMPL_H_
#define COMPONENTS_GAMES_CORE_GAMES_SERVICE_IMPL_H_

#include <memory>

#include "components/games/core/games_service.h"
#include "components/games/core/games_types.h"
#include "components/prefs/pref_service.h"

namespace games {

class GamesServiceImpl : public GamesService {
 public:
  explicit GamesServiceImpl(PrefService* prefs);
  ~GamesServiceImpl() override;

  void GetHighlightedGame(HighlightedGameCallback callback) override;

  // Will outlive the current instance.
  PrefService* prefs_;
};

}  // namespace games

#endif  // COMPONENTS_GAMES_CORE_GAMES_SERVICE_IMPL_H_
