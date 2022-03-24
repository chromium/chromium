// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/games/stub_api.h"

// TODO(crbug.com/1305880)
// This whole file is a temporary fake API for the game index manager, that can
// be deleted and replaced once the real API is implemented.

namespace app_list {

GameData::GameData() = default;

GameData::~GameData() = default;

GameData::GameData(const GameData&) = default;

GameData& GameData::operator=(const GameData&) = default;

absl::optional<GameIndex> GameIndexManager::GetIndex() {
  return absl::nullopt;
}

void GameIndexManager::AddObserver(GameIndexManager::Observer* observer) {}

void GameIndexManager::RemoveObserver(GameIndexManager::Observer* observer) {}

void GameIndexManager::GetIcon(
    const GURL& icon_url,
    base::OnceCallback<void(const SkBitmap*)> callback) {}

std::u16string GameSourceDisplayString(GameSource source) {
  switch (source) {
    case GameSource::kExampleSource:
      return u"Example source";
  }
}

}  // namespace app_list
