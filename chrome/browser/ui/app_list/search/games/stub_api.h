// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_GAMES_STUB_API_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_GAMES_STUB_API_H_

#include <string>

#include "base/callback.h"
#include "base/observer_list_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

// TODO(crbug.com/1305880)
// This whole file is a temporary fake API for the game index manager, that can
// be deleted and replaced once the real API is implemented.

namespace app_list {

// Each possible source a game can be provided by.
enum class GameSource {
  kExampleSource,
};

// Metadata for a single game in the index of games.
struct GameData {
  // Game title
  std::u16string title;

  GameSource source = GameSource::kExampleSource;

  // What platforms the game is available on.
  absl::optional<std::vector<std::u16string>> platforms;

  // The URL that should be opened when the search result is clicked.
  GURL launch_url;

  // A token uniquely identifying an icon.
  GURL icon_url;

  GameData();
  ~GameData();

  GameData(const GameData&);
  GameData& operator=(const GameData&);
};

// A collection of game data, forming an index.
using GameIndex = std::vector<GameData>;

class GameIndexManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnIndexUpdated(const absl::optional<GameIndex>& index) = 0;
  };

  absl::optional<GameIndex> GetIndex();
  void GetIcon(const GURL& icon_url,
               base::OnceCallback<void(const SkBitmap*)> callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
};

std::u16string GameSourceDisplayString(GameSource source);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_GAMES_STUB_API_H_
