// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/game_dashboard/chrome_game_dashboard_delegate.h"

#include "ash/components/arc/session/connection_holder.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"

ChromeGameDashboardDelegate::ChromeGameDashboardDelegate() {}

ChromeGameDashboardDelegate::~ChromeGameDashboardDelegate() {}

void ChromeGameDashboardDelegate::GetIsGame(const std::string& app_id,
                                            IsGameCallback callback) {
  // Get the app category from ArcAppListPrefs.
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  CHECK(profile);
  auto* arc_app_list_prefs = ArcAppListPrefs::Get(profile);
  const auto app_category = arc_app_list_prefs->GetAppCategory(app_id);
  // If the category is anything except `kUndefined`, fire the callback,
  // otherwise, retrieve the category from ARC.
  if (app_category != arc::mojom::AppCategory::kUndefined) {
    std::move(callback).Run(app_category == arc::mojom::AppCategory::kGame);
    return;
  }

  auto* app_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_app_list_prefs->app_connection_holder(), GetAppCategory);
  if (!app_instance) {
    // If there's no app instance, assume the app is not a game.
    std::move(callback).Run(/*is_game=*/false);
    return;
  }

  app_instance->GetAppCategory(
      arc_app_list_prefs->GetAppPackageName(app_id),
      base::BindOnce(&ChromeGameDashboardDelegate::OnReceiveAppCategory,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

std::string ChromeGameDashboardDelegate::GetArcAppName(
    const std::string& app_id) const {
  // Get the app category from ArcAppListPrefs.
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  CHECK(profile);

  auto app_info = ArcAppListPrefs::Get(profile)->GetApp(app_id);
  if (!app_info) {
    LOG(ERROR) << "Failed to get app info: " << app_id << ".";
    return std::string();
  }
  return app_info->name;
}

void ChromeGameDashboardDelegate::OnReceiveAppCategory(
    IsGameCallback callback,
    arc::mojom::AppCategory category) {
  std::move(callback).Run(category == arc::mojom::AppCategory::kGame);
}
