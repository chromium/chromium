// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_service/app_service_app_item.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/arc/arc_app_context_menu.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_context_menu.h"
#include "chrome/browser/ui/app_list/extension_app_context_menu.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

// static
const char AppServiceAppItem::kItemType[] = "AppServiceAppItem";

// static
std::unique_ptr<app_list::AppContextMenu> AppServiceAppItem::MakeAppContextMenu(
    apps::mojom::AppType app_type,
    AppContextMenuDelegate* delegate,
    Profile* profile,
    const std::string& app_id,
    AppListControllerDelegate* controller,
    bool is_platform_app) {
  switch (app_type) {
    case apps::mojom::AppType::kUnknown:
    case apps::mojom::AppType::kBuiltIn:
      return std::make_unique<app_list::AppContextMenu>(delegate, profile,
                                                        app_id, controller);

    case apps::mojom::AppType::kArc:
      return std::make_unique<ArcAppContextMenu>(delegate, profile, app_id,
                                                 controller);

    case apps::mojom::AppType::kCrostini:
      return std::make_unique<CrostiniAppContextMenu>(profile, app_id,
                                                      controller);

    case apps::mojom::AppType::kExtension:
    case apps::mojom::AppType::kWeb:
      return std::make_unique<app_list::ExtensionAppContextMenu>(
          delegate, profile, app_id, controller, is_platform_app);

    case apps::mojom::AppType::kMacNative:
      NOTREACHED() << "Should not be trying to make a menu for a native app";
      return nullptr;
  }

  return nullptr;
}

AppServiceAppItem::AppServiceAppItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const apps::AppUpdate& app_update)
    : ChromeAppListItem(profile, app_update.AppId()),
      app_type_(app_update.AppType()),
      is_platform_app_(false) {
  OnAppUpdate(app_update, true);
  if (sync_item && sync_item->item_ordinal.IsValid()) {
    UpdateFromSync(sync_item);
  } else {
    SetDefaultPositionIfApplicable(model_updater);

    // Crostini hard-codes its own folder. As Crostini apps are created from
    // scratch, we move them to a default folder.
    if (app_type_ == apps::mojom::AppType::kCrostini) {
      DCHECK(folder_id().empty());
      SetChromeFolderId(crostini::kCrostiniFolderId);
    }
  }

  // Set model updater last to avoid being called during construction.
  set_model_updater(model_updater);
}

AppServiceAppItem::~AppServiceAppItem() = default;

void AppServiceAppItem::OnAppUpdate(const apps::AppUpdate& app_update) {
  OnAppUpdate(app_update, false);
}

void AppServiceAppItem::OnAppUpdate(const apps::AppUpdate& app_update,
                                    bool in_constructor) {
  if (in_constructor || app_update.NameChanged()) {
    SetName(app_update.Name());
  }

  if (in_constructor || app_update.IconKeyChanged()) {
    constexpr bool allow_placeholder_icon = true;
    CallLoadIcon(allow_placeholder_icon);
  }

  if (in_constructor || app_update.IsPlatformAppChanged()) {
    is_platform_app_ =
        app_update.IsPlatformApp() == apps::mojom::OptionalBool::kTrue;
  }
}

void AppServiceAppItem::Activate(int event_flags) {
  // TODO(crbug.com/1022541): Move the Chrome special case to ExtensionApps,
  // when AppService Instance feature is done.
  if (id() == extension_misc::kChromeAppId) {
    ChromeLauncherController::instance()->ActivateApp(
        id(), ash::LAUNCH_FROM_APP_LIST, event_flags,
        GetController()->GetAppListDisplayId());
    return;
  }
  Launch(event_flags, apps::mojom::LaunchSource::kFromAppListGrid);
}

const char* AppServiceAppItem::GetItemType() const {
  return AppServiceAppItem::kItemType;
}

void AppServiceAppItem::GetContextMenuModel(GetMenuModelCallback callback) {
  context_menu_ = MakeAppContextMenu(app_type_, this, profile(), id(),
                                     GetController(), is_platform_app_);
  context_menu_->GetMenuModel(std::move(callback));
}

app_list::AppContextMenu* AppServiceAppItem::GetAppContextMenu() {
  return context_menu_.get();
}

void AppServiceAppItem::ExecuteLaunchCommand(int event_flags) {
  Launch(event_flags, apps::mojom::LaunchSource::kFromAppListGridContextMenu);

  // TODO(crbug.com/826982): drop the if, and call MaybeDismissAppList
  // unconditionally?
  if (app_type_ == apps::mojom::AppType::kArc) {
    MaybeDismissAppList();
  }
}

void AppServiceAppItem::Launch(int event_flags,
                               apps::mojom::LaunchSource launch_source) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  if (proxy) {
    proxy->Launch(id(), event_flags, launch_source,
                  GetController()->GetAppListDisplayId());
  }
}

void AppServiceAppItem::CallLoadIcon(bool allow_placeholder_icon) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  if (proxy) {
    proxy->LoadIcon(app_type_, id(),
                    apps::mojom::IconCompression::kUncompressed,
                    ash::AppListConfig::instance().grid_icon_dimension(),
                    allow_placeholder_icon,
                    base::BindOnce(&AppServiceAppItem::OnLoadIcon,
                                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void AppServiceAppItem::OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
  if (icon_value->icon_compression !=
      apps::mojom::IconCompression::kUncompressed) {
    return;
  }
  SetIcon(icon_value->uncompressed);

  if (icon_value->is_placeholder_icon) {
    constexpr bool allow_placeholder_icon = false;
    CallLoadIcon(allow_placeholder_icon);
  }
}
