// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_service/app_service_app_model_builder.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ui/app_list/app_service/app_service_app_item.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync/protocol/app_list_specifics.pb.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool ShouldShowInLauncher(const apps::AppUpdate& update) {
  apps::mojom::Readiness readiness = update.Readiness();
  switch (readiness) {
    case apps::mojom::Readiness::kReady:
    case apps::mojom::Readiness::kDisabledByUser:
    case apps::mojom::Readiness::kDisabledByBlocklist:
    case apps::mojom::Readiness::kDisabledByPolicy:
    case apps::mojom::Readiness::kTerminated:
      return update.ShowInLauncher() == apps::mojom::OptionalBool::kTrue;
    default:
      return false;
  }
}

}  // namespace

AppServiceAppModelBuilder::AppServiceAppModelBuilder(
    AppListControllerDelegate* controller)
    : AppListModelBuilder(controller, AppServiceAppItem::kItemType) {}

AppServiceAppModelBuilder::~AppServiceAppModelBuilder() = default;

void AppServiceAppModelBuilder::BuildModel() {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->AppRegistryCache().ForEachApp(
      [this](const apps::AppUpdate& update) { OnAppUpdate(update); });
  Observe(&proxy->AppRegistryCache());
}

void AppServiceAppModelBuilder::OnAppUpdate(const apps::AppUpdate& update) {
  ChromeAppListItem* item = GetAppItem(update.AppId());
  bool show = ShouldShowInLauncher(update);
  if (item) {
    if (show) {
      DCHECK(item->GetItemType() == AppServiceAppItem::kItemType);
      static_cast<AppServiceAppItem*>(item)->OnAppUpdate(update);

      // TODO(crbug.com/826982): drop the check for kChromeApp or kWeb, and
      // call UpdateItem unconditionally?
      apps::mojom::AppType app_type = update.AppType();
      if ((app_type == apps::mojom::AppType::kChromeApp) ||
          (app_type == apps::mojom::AppType::kSystemWeb) ||
          (app_type == apps::mojom::AppType::kWeb)) {
        app_list::AppListSyncableService* serv = service();
        if (serv) {
          serv->UpdateItem(item);
        }
      }

    } else {
      bool unsynced_change = false;
      if (update.AppType() == apps::mojom::AppType::kArc) {
        // Don't sync app removal in case it was caused by disabling Google
        // Play Store.
        unsynced_change = !arc::IsArcPlayStoreEnabledForProfile(profile());
      }

      if (update.InstalledInternally() == apps::mojom::OptionalBool::kTrue) {
        // Don't sync default app removal as default installed apps are not
        // synced.
        unsynced_change = true;
      }

      if (update.Readiness() ==
          apps::mojom::Readiness::kUninstalledByMigration) {
        // Don't sync migration uninstallations as it will interfere with other
        // devices doing their own migration.
        unsynced_change = true;
      }

      RemoveApp(update.AppId(), unsynced_change);
    }

  } else if (show) {
    InsertApp(std::make_unique<AppServiceAppItem>(
        profile(), model_updater(),
        GetSyncItem(update.AppId(), sync_pb::AppListSpecifics::TYPE_APP),
        update));
  }
}

void AppServiceAppModelBuilder::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}
