// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_app_item.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/bind.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_context_menu.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "content/public/browser/browser_thread.h"

// static
const char CrostiniAppItem::kItemType[] = "CrostiniAppItem";

CrostiniAppItem::CrostiniAppItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const std::string& id,
    const std::string& name)
    : ChromeAppListItem(profile, id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  crostini_app_icon_ = std::make_unique<CrostiniAppIcon>(
      profile, id, ash::AppListConfig::instance().grid_icon_dimension(), this);

  SetName(name);
  UpdateIcon();
  if (sync_item && sync_item->item_ordinal.IsValid()) {
    UpdateFromSync(sync_item);
  } else {
    SetDefaultPositionIfApplicable(model_updater);

    // Crostini app is created from scratch. Move it to default folder.
    DCHECK(folder_id().empty());
    SetChromeFolderId(crostini::kCrostiniFolderId);
  }

  // Set model updater last to avoid being called during construction.
  set_model_updater(model_updater);
}

CrostiniAppItem::~CrostiniAppItem() {}

const char* CrostiniAppItem::GetItemType() const {
  return CrostiniAppItem::kItemType;
}

void CrostiniAppItem::Activate(int event_flags) {
  ChromeLauncherController::instance()->ActivateApp(
      id(), ash::LAUNCH_FROM_APP_LIST, event_flags,
      GetController()->GetAppListDisplayId());
}

void CrostiniAppItem::GetContextMenuModel(GetMenuModelCallback callback) {
  context_menu_ = std::make_unique<CrostiniAppContextMenu>(profile(), id(),
                                                           GetController());
  context_menu_->GetMenuModel(std::move(callback));
}

app_list::AppContextMenu* CrostiniAppItem::GetAppContextMenu() {
  return context_menu_.get();
}

void CrostiniAppItem::UpdateIcon() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetIcon(crostini_app_icon_->image_skia());
}

void CrostiniAppItem::OnIconUpdated(CrostiniAppIcon* icon) {
  UpdateIcon();
}
