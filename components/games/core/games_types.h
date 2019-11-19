// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAMES_CORE_GAMES_TYPES_H_
#define COMPONENTS_GAMES_CORE_GAMES_TYPES_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "components/games/core/proto/game.pb.h"

namespace games {
using HighlightedGameCallback = base::OnceCallback<void(std::unique_ptr<Game>)>;

}  // namespace games

#endif  // COMPONENTS_GAMES_CORE_GAMES_TYPES_H_
