// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_model_builder.h"

#include <vector>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_item.h"

ArcAppModelBuilder::ArcAppModelBuilder(AppListControllerDelegate* controller)
    : AppListModelBuilder(controller, ArcAppItem::kItemType) {}

ArcAppModelBuilder::~ArcAppModelBuilder() {
  prefs_->RemoveObserver(this);
}

void ArcAppModelBuilder::InsertApp(std::unique_ptr<ChromeAppListItem> app) {
  const std::string app_id = app->id();
  AppListModelBuilder::InsertApp(std::move(app));
  icon_loader_->FetchImage(app_id);
}

void ArcAppModelBuilder::RemoveApp(const std::string& id,
                                   bool unsynced_change) {
  AppListModelBuilder::RemoveApp(id, unsynced_change);
  icon_loader_->ClearImage(id);
}

void ArcAppModelBuilder::BuildModel() {
  icon_loader_ = std::make_unique<ArcAppIconLoader>(
      profile(), ash::AppListConfig::instance().grid_icon_dimension(), this);

  prefs_ = ArcAppListPrefs::Get(profile());
  DCHECK(prefs_);

  std::vector<std::string> app_ids = prefs_->GetAppIds();
  for (auto& app_id : app_ids) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs_->GetApp(app_id);
    if (!app_info) {
      NOTREACHED() << "App " << app_id << " was not found";
      continue;
    }

    OnAppRegistered(app_id, *app_info);
  }

  prefs_->AddObserver(this);
}

ArcAppItem* ArcAppModelBuilder::GetArcAppItem(const std::string& app_id) {
  return static_cast<ArcAppItem*>(GetAppItem(app_id));
}

std::unique_ptr<ArcAppItem> ArcAppModelBuilder::CreateApp(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  return std::make_unique<ArcAppItem>(
      profile(), model_updater(), GetSyncItem(app_id), app_id, app_info.name);
}

void ArcAppModelBuilder::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (app_info.show_in_launcher)
    InsertApp(CreateApp(app_id, app_info));
}

void ArcAppModelBuilder::OnAppRemoved(const std::string& app_id) {
  // Don't sync app removal in case it was caused by disabling Google Play
  // Store.
  const bool unsynced_change = !arc::IsArcPlayStoreEnabledForProfile(profile());
  RemoveApp(app_id, unsynced_change);
}

void ArcAppModelBuilder::OnAppImageUpdated(const std::string& app_id,
                                           const gfx::ImageSkia& image) {
  ArcAppItem* app_item = GetArcAppItem(app_id);
  if (!app_item) {
    VLOG(2) << "Could not update the icon of ARC app(" << app_id
            << ") because it was not found.";
    return;
  }
  app_item->SetIcon(image);
}

void ArcAppModelBuilder::OnAppNameUpdated(const std::string& app_id,
                                          const std::string& name) {
  ArcAppItem* app_item = GetArcAppItem(app_id);
  if (!app_item) {
    VLOG(2) << "Could not update the name of ARC app(" << app_id
            << ") because it was not found.";
    return;
  }

  app_item->SetName(name);
}
