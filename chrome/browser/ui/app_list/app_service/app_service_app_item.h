// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_APP_ITEM_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_APP_ITEM_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/services/app_service/public/cpp/app_update.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

class AppServiceAppItem : public ChromeAppListItem,
                          public app_list::AppContextMenuDelegate {
 public:
  static const char kItemType[];

  static std::unique_ptr<app_list::AppContextMenu> MakeAppContextMenu(
      apps::mojom::AppType app_type,
      AppContextMenuDelegate* delegate,
      Profile* profile,
      const std::string& app_id,
      AppListControllerDelegate* controller,
      bool is_platform_app);

  AppServiceAppItem(Profile* profile,
                    AppListModelUpdater* model_updater,
                    const app_list::AppListSyncableService::SyncItem* sync_item,
                    const apps::AppUpdate& app_update);
  ~AppServiceAppItem() override;

  void OnAppUpdate(const apps::AppUpdate& app_update);

 private:
  void OnAppUpdate(const apps::AppUpdate& app_update, bool in_constructor);

  // ChromeAppListItem overrides:
  void Activate(int event_flags) override;
  const char* GetItemType() const override;
  void GetContextMenuModel(GetMenuModelCallback callback) override;
  app_list::AppContextMenu* GetAppContextMenu() override;

  // app_list::AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override;

  void Launch(int event_flags, apps::mojom::LaunchSource launch_source);

  void CallLoadIcon(bool allow_placeholder_icon);
  void OnLoadIcon(apps::mojom::IconValuePtr icon_value);

  apps::mojom::AppType app_type_;
  bool is_platform_app_;

  std::unique_ptr<app_list::AppContextMenu> context_menu_;

  base::WeakPtrFactory<AppServiceAppItem> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppServiceAppItem);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_SERVICE_APP_SERVICE_APP_ITEM_H_
