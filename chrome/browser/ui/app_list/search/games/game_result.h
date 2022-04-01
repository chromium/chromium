// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_GAMES_GAME_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_GAMES_GAME_RESULT_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service.h"
#include "chrome/browser/apps/app_discovery_service/result.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "url/gurl.h"

class AppListControllerDelegate;
class Profile;
class SkBitmap;

namespace app_list {

// Search result for cloud gaming search.
class GameResult : public ChromeSearchResult {
 public:
  GameResult(Profile* profile,
             AppListControllerDelegate* list_controller,
             apps::AppDiscoveryService* app_discovery_service,
             const apps::Result& game,
             double relevance,
             const std::u16string& query);
  ~GameResult() override;

  GameResult(const GameResult&) = delete;
  GameResult& operator=(const GameResult&) = delete;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;

 private:
  void UpdateText(const apps::Result& game, const std::u16string& query);
  void SetGenericIcon();
  void OnIconLoaded(const SkBitmap* bitmap);

  Profile* profile_;
  AppListControllerDelegate* list_controller_;

  const GURL launch_url_;

  base::WeakPtrFactory<GameResult> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_GAMES_GAME_RESULT_H_
