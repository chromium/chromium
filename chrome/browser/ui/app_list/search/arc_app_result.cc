// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc_app_result.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_context_menu.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"
#include "chrome/browser/ui/app_list/search/search_util.h"

namespace {
const char kArcAppPrefix[] = "arc://";
}

namespace app_list {

ArcAppResult::ArcAppResult(Profile* profile,
                           const std::string& app_id,
                           AppListControllerDelegate* controller,
                           bool is_recommendation)
    : AppResult(profile, app_id, controller, is_recommendation) {
  std::string id = kArcAppPrefix;
  id += app_id;
  set_id(id);
  icon_loader_ = std::make_unique<ArcAppIconLoader>(
      profile,
      AppListConfig::instance().GetPreferredIconDimension(display_type()),
      this);
  icon_loader_->FetchImage(app_id);
  // Load an additional chip icon when it is a recommendation result
  // so that it renders clearly in both a chip and a tile.
  if (display_type() == ash::SearchResultDisplayType::kRecommendation) {
    chip_icon_loader_ = std::make_unique<ArcAppIconLoader>(
        profile, AppListConfig::instance().suggestion_chip_icon_dimension(),
        this);
    chip_icon_loader_->FetchImage(app_id);
  }
}

ArcAppResult::~ArcAppResult() {}

void ArcAppResult::OnAppImageUpdated(const std::string& app_id,
                                     const gfx::ImageSkia& image) {
  const gfx::Size icon_size(
      AppListConfig::instance().GetPreferredIconDimension(display_type()),
      AppListConfig::instance().GetPreferredIconDimension(display_type()));
  const gfx::Size chip_icon_size(
      AppListConfig::instance().suggestion_chip_icon_dimension(),
      AppListConfig::instance().suggestion_chip_icon_dimension());
  DCHECK(icon_size != chip_icon_size);

  if (image.size() == icon_size) {
    SetIcon(image);
  } else if (image.size() == chip_icon_size) {
    DCHECK_EQ(ash::SearchResultDisplayType::kRecommendation, display_type());
    SetChipIcon(image);
  } else {
    NOTREACHED();
  }
}

void ArcAppResult::ExecuteLaunchCommand(int event_flags) {
  Launch(event_flags, GetContextMenuAppLaunchInteraction());
}

void ArcAppResult::Open(int event_flags) {
  Launch(event_flags, GetAppLaunchInteraction());
}

void ArcAppResult::GetContextMenuModel(GetMenuModelCallback callback) {
  context_menu_ = std::make_unique<ArcAppContextMenu>(this, profile(), app_id(),
                                                      controller());
  context_menu_->GetMenuModel(std::move(callback));
}

AppContextMenu* ArcAppResult::GetAppContextMenu() {
  return context_menu_.get();
}

void ArcAppResult::Launch(int event_flags,
                          arc::UserInteractionType interaction) {
  // Record the search metric if the result is not a suggested app.
  if (display_type() != ash::SearchResultDisplayType::kRecommendation)
    RecordHistogram(APP_SEARCH_RESULT);

  arc::LaunchApp(profile(), app_id(), event_flags, interaction,
                 controller()->GetAppListDisplayId());
}

arc::UserInteractionType ArcAppResult::GetAppLaunchInteraction() {
  return display_type() == ash::SearchResultDisplayType::kRecommendation
             ? arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP
             : arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SEARCH;
}

arc::UserInteractionType ArcAppResult::GetContextMenuAppLaunchInteraction() {
  return display_type() == ash::SearchResultDisplayType::kRecommendation
             ? arc::UserInteractionType::
                   APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP_CONTEXT_MENU
             : arc::UserInteractionType::
                   APP_STARTED_FROM_LAUNCHER_SEARCH_CONTEXT_MENU;
}

}  // namespace app_list
