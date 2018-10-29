// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/crostini_app_result.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_context_menu.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_icon_loader.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

namespace app_list {

CrostiniAppResult::CrostiniAppResult(Profile* profile,
                                     const std::string& app_id,
                                     AppListControllerDelegate* controller,
                                     bool is_recommendation)
    : AppResult(profile, app_id, controller, is_recommendation) {
  set_id(app_id);

  icon_loader_ = std::make_unique<CrostiniAppIconLoader>(
      profile,
      AppListConfig::instance().GetPreferredIconDimension(display_type()),
      this);
  icon_loader_->FetchImage(app_id);

  // Load an additional chip icon when it is a recommendation result
  // so that it renders clearly in both a chip and a tile.
  if (display_type() == ash::SearchResultDisplayType::kRecommendation) {
    chip_icon_loader_ = std::make_unique<CrostiniAppIconLoader>(
        profile, AppListConfig::instance().suggestion_chip_icon_dimension(),
        this);
    chip_icon_loader_->FetchImage(app_id);
  }
}

CrostiniAppResult::~CrostiniAppResult() = default;

void CrostiniAppResult::Open(int event_flags) {
  ChromeLauncherController::instance()->ActivateApp(
      id(), ash::LAUNCH_FROM_APP_LIST_SEARCH, event_flags,
      controller()->GetAppListDisplayId());
}

void CrostiniAppResult::GetContextMenuModel(GetMenuModelCallback callback) {
  context_menu_ = std::make_unique<CrostiniAppContextMenu>(profile(), app_id(),
                                                           controller());
  context_menu_->GetMenuModel(std::move(callback));
}

void CrostiniAppResult::ExecuteLaunchCommand(int event_flags) {
  Open(event_flags);
}

void CrostiniAppResult::OnAppImageUpdated(const std::string& app_id,
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
    DCHECK(display_type() == ash::SearchResultDisplayType::kRecommendation);
    SetChipIcon(image);
  } else {
    NOTREACHED();
  }
}

AppContextMenu* CrostiniAppResult::GetAppContextMenu() {
  return context_menu_.get();
}

}  // namespace app_list
