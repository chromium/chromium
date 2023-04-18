// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/game_dashboard/chrome_game_dashboard_delegate.h"

#include "ash/components/arc/mojom/app.mojom-shared.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/profiles/profile_manager.h"

bool ChromeGameDashboardDelegate::IsGame(const std::string& app_id) const {
  return ArcAppListPrefsFactory::GetForBrowserContext(
             ProfileManager::GetPrimaryUserProfile())
             ->GetAppCategory(app_id) == arc::mojom::AppCategory::kGame;
}
