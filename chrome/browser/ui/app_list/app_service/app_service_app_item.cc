// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_service/app_service_app_item.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_service/app_service_context_menu.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"

namespace {

// Returns true if `app_update` should be considered a new app install.
bool IsNewInstall(const apps::AppUpdate& app_update) {
  // Ignore internally-installed apps.
  if (app_update.InstalledInternally() == apps::mojom::OptionalBool::kTrue)
    return false;

  switch (app_update.AppType()) {
    case apps::mojom::AppType::kUnknown:
    case apps::mojom::AppType::kBuiltIn:
    case apps::mojom::AppType::kStandaloneBrowser:
    case apps::mojom::AppType::kSystemWeb:
      // Chrome, Lacros, Settings, etc. are built-in.
      return false;
    case apps::mojom::AppType::kMacOs:
      NOTREACHED();
      return false;
    case apps::mojom::AppType::kArc:
    case apps::mojom::AppType::kCrostini:
    case apps::mojom::AppType::kChromeApp:
    case apps::mojom::AppType::kExtension:
    case apps::mojom::AppType::kWeb:
    case apps::mojom::AppType::kPluginVm:
    case apps::mojom::AppType::kRemote:
    case apps::mojom::AppType::kBorealis:
    case apps::mojom::AppType::kStandaloneBrowserChromeApp:
      // Other app types are user-installed.
      return true;
  }
}

}  // namespace

// static
const char AppServiceAppItem::kItemType[] = "AppServiceAppItem";

AppServiceAppItem::AppServiceAppItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const apps::AppUpdate& app_update)
    : ChromeAppListItem(profile, app_update.AppId()),
      app_type_(app_update.AppType()) {
  OnAppUpdate(app_update, /*in_constructor=*/true);
  if (sync_item && sync_item->item_ordinal.IsValid()) {
    InitFromSync(sync_item);
  } else {
    // Handle the case that the app under construction is a remote app.
    if (app_type_ == apps::mojom::AppType::kRemote) {
      ash::RemoteAppsManager* remote_manager =
          ash::RemoteAppsManagerFactory::GetForProfile(profile);
      if (remote_manager->ShouldAddToFront(app_update.AppId()))
        SetPosition(model_updater->GetPositionBeforeFirstItem());

      const ash::RemoteAppsModel::AppInfo* app_info =
          remote_manager->GetAppInfo(app_update.AppId());
      if (app_info)
        SetChromeFolderId(app_info->folder_id);
    }

    if (!position().IsValid()) {
      // If there is no default positions, the model builder will handle it when
      // the item is inserted.
      SetPosition(CalculateDefaultPositionIfApplicable());
    }

    // Crostini apps and the Terminal System App start in the crostini folder.
    if (app_type_ == apps::mojom::AppType::kCrostini ||
        id() == crostini::kCrostiniTerminalSystemAppId) {
      DCHECK(folder_id().empty());
      SetChromeFolderId(ash::kCrostiniFolderId);
    }
  }

  if (ash::features::IsProductivityLauncherEnabled()) {
    const bool is_new_install = !sync_item && IsNewInstall(app_update);
    DVLOG(1) << "New AppServiceAppItem is_new_install " << is_new_install
             << " from update " << app_update;
    SetIsNewInstall(is_new_install);
  }

  // Set model updater last to avoid being called during construction.
  set_model_updater(model_updater);
}

AppServiceAppItem::~AppServiceAppItem() = default;

void AppServiceAppItem::OnAppUpdate(const apps::AppUpdate& app_update) {
  OnAppUpdate(app_update, /*in_constructor=*/false);
}

void AppServiceAppItem::OnAppUpdate(const apps::AppUpdate& app_update,
                                    bool in_constructor) {
  if (in_constructor || app_update.NameChanged()) {
    SetName(app_update.Name());
  }

  if (in_constructor || app_update.IconKeyChanged()) {
    // Increment icon version to indicate icon needs to be updated.
    IncrementIconVersion();
  }

  if (in_constructor || app_update.IsPlatformAppChanged()) {
    is_platform_app_ =
        app_update.IsPlatformApp() == apps::mojom::OptionalBool::kTrue;
  }

  if (in_constructor || app_update.ReadinessChanged() ||
      app_update.PausedChanged()) {
    if (app_update.Readiness() == apps::mojom::Readiness::kDisabledByPolicy) {
      SetAppStatus(ash::AppStatus::kBlocked);
    } else if (app_update.Paused() == apps::mojom::OptionalBool::kTrue) {
      SetAppStatus(ash::AppStatus::kPaused);
    } else {
      SetAppStatus(ash::AppStatus::kReady);
    }
  }
}

void AppServiceAppItem::LoadIcon() {
  constexpr bool allow_placeholder_icon = true;
  CallLoadIcon(allow_placeholder_icon);
}

void AppServiceAppItem::Activate(int event_flags) {
  // For Crostini apps, non-platform Chrome apps, Web apps, it could be
  // selecting an existing delegate for the app, so call
  // ChromeShelfController's ActivateApp interface. Platform apps or ARC
  // apps, Crostini apps treat activations as a launch. The app can decide
  // whether to show a new window or focus an existing window as it sees fit.
  //
  // TODO(crbug.com/1022541): Move the Chrome special case to ExtensionApps,
  // when AppService Instance feature is done.
  bool is_active_app = false;
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->AppRegistryCache()
      .ForOneApp(id(), [&is_active_app](const apps::AppUpdate& update) {
        if (update.AppType() == apps::mojom::AppType::kCrostini ||
            update.AppType() == apps::mojom::AppType::kWeb ||
            update.AppType() == apps::mojom::AppType::kSystemWeb ||
            (update.AppType() == apps::mojom::AppType::kChromeApp &&
             update.IsPlatformApp() == apps::mojom::OptionalBool::kFalse)) {
          is_active_app = true;
        }
      });
  if (is_active_app) {
    ash::ShelfID shelf_id(id());
    ash::ShelfModel* model = ChromeShelfController::instance()->shelf_model();
    ash::ShelfItemDelegate* delegate = model->GetShelfItemDelegate(shelf_id);
    if (delegate) {
      delegate->ItemSelected(
          /*event=*/nullptr, GetController()->GetAppListDisplayId(),
          ash::LAUNCH_FROM_APP_LIST, /*callback=*/base::DoNothing(),
          /*filter_predicate=*/base::NullCallback());
      return;
    }
  }
  Launch(event_flags, apps::mojom::LaunchSource::kFromAppListGrid);
}

const char* AppServiceAppItem::GetItemType() const {
  return AppServiceAppItem::kItemType;
}

void AppServiceAppItem::GetContextMenuModel(bool add_sort_options,
                                            GetMenuModelCallback callback) {
  context_menu_ = std::make_unique<AppServiceContextMenu>(
      this, profile(), id(), GetController(), add_sort_options);

  context_menu_->GetMenuModel(std::move(callback));
}

app_list::AppContextMenu* AppServiceAppItem::GetAppContextMenu() {
  return context_menu_.get();
}

void AppServiceAppItem::ExecuteLaunchCommand(int event_flags) {
  Launch(event_flags, apps::mojom::LaunchSource::kFromAppListGridContextMenu);

  // TODO(crbug.com/826982): drop the if, and call MaybeDismissAppList
  // unconditionally?
  if (app_type_ == apps::mojom::AppType::kArc ||
      app_type_ == apps::mojom::AppType::kRemote) {
    MaybeDismissAppList();
  }
}

void AppServiceAppItem::Launch(int event_flags,
                               apps::mojom::LaunchSource launch_source) {
  if (ash::features::IsProductivityLauncherEnabled()) {
    // Launching an app clears the "new install" badge.
    SetIsNewInstall(false);
  }
  apps::AppServiceProxyFactory::GetForProfile(profile())->Launch(
      id(), event_flags, launch_source,
      apps::MakeWindowInfo(GetController()->GetAppListDisplayId()));
}

void AppServiceAppItem::CallLoadIcon(bool allow_placeholder_icon) {
  if (base::FeatureList::IsEnabled(features::kAppServiceLoadIconWithoutMojom)) {
    apps::AppServiceProxyFactory::GetForProfile(profile())->LoadIcon(
        apps::ConvertMojomAppTypToAppType(app_type_), id(),
        apps::IconType::kStandard,
        ash::SharedAppListConfig::instance().default_grid_icon_dimension(),
        allow_placeholder_icon,
        base::BindOnce(&AppServiceAppItem::OnLoadIcon,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    apps::AppServiceProxyFactory::GetForProfile(profile())->LoadIcon(
        app_type_, id(), apps::mojom::IconType::kStandard,
        ash::SharedAppListConfig::instance().default_grid_icon_dimension(),
        allow_placeholder_icon,
        apps::MojomIconValueToIconValueCallback(base::BindOnce(
            &AppServiceAppItem::OnLoadIcon, weak_ptr_factory_.GetWeakPtr())));
  }
}

void AppServiceAppItem::OnLoadIcon(apps::IconValuePtr icon_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard) {
    return;
  }
  SetIcon(icon_value->uncompressed);

  if (icon_value->is_placeholder_icon) {
    constexpr bool allow_placeholder_icon = false;
    CallLoadIcon(allow_placeholder_icon);
  }
}
