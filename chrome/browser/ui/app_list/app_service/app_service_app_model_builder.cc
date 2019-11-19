// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_service/app_service_app_model_builder.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/app_list/app_service/app_service_app_item.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool ShouldShowInLauncher(const apps::AppUpdate& update) {
  apps::mojom::Readiness readiness = update.Readiness();
  switch (readiness) {
    case apps::mojom::Readiness::kReady:
    case apps::mojom::Readiness::kDisabledByUser:
    case apps::mojom::Readiness::kDisabledByBlacklist:
    case apps::mojom::Readiness::kDisabledByPolicy:
    case apps::mojom::Readiness::kTerminated:
      return update.ShowInLauncher() == apps::mojom::OptionalBool::kTrue;
    default:
      return false;
  }
}

}  // namespace

// Folder items are created by the Ash process and their existence is
// communicated to chrome via the AppListClient. Therefore, Crostini has an
// observer that listens for the creation of its folder, and updates the
// properties accordingly.
//
// Folders are an App List UI concept, not intrinsic to apps, so this
// Crostini-specific feature is implemented here (chrome/browser/ui/app_list)
// instead of in the App Service per se.
class AppServiceAppModelBuilder::CrostiniFolderObserver
    : public AppListModelUpdaterObserver {
 public:
  explicit CrostiniFolderObserver(AppServiceAppModelBuilder* parent)
      : parent_(parent) {}

  ~CrostiniFolderObserver() override = default;

  void OnAppListItemAdded(ChromeAppListItem* item) override {
    if (item->id() != crostini::kCrostiniFolderId)
      return;
    // Persistence is not recorded by the sync, so we always set it.
    item->SetIsPersistent(true);

    // Either the name and position will be in the sync, or we set them
    // manually.
    if (parent_->GetSyncItem(crostini::kCrostiniFolderId))
      return;
    item->SetName(
        l10n_util::GetStringUTF8(IDS_APP_LIST_CROSTINI_DEFAULT_FOLDER_NAME));
    item->SetDefaultPositionIfApplicable(parent_->model_updater());
  }

 private:
  AppServiceAppModelBuilder* parent_;
};

AppServiceAppModelBuilder::AppServiceAppModelBuilder(
    AppListControllerDelegate* controller)
    : AppListModelBuilder(controller, AppServiceAppItem::kItemType) {}

AppServiceAppModelBuilder::~AppServiceAppModelBuilder() {
  if (model_updater() && crostini_folder_observer_) {
    model_updater()->RemoveObserver(crostini_folder_observer_.get());
  }
}

void AppServiceAppModelBuilder::BuildModel() {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());
  if (proxy) {
    proxy->AppRegistryCache().ForEachApp(
        [this](const apps::AppUpdate& update) { OnAppUpdate(update); });
    Observe(&proxy->AppRegistryCache());
  } else {
    // TODO(crbug.com/826982): do we want apps in incognito mode? See the TODO
    // in AppServiceProxyFactory::GetForProfile about whether
    // apps::AppServiceProxy::Get should return nullptr for incognito profiles.
  }

  if (model_updater()) {
    crostini_folder_observer_ = std::make_unique<CrostiniFolderObserver>(this);
    model_updater()->AddObserver(crostini_folder_observer_.get());
  }
}

void AppServiceAppModelBuilder::OnAppUpdate(const apps::AppUpdate& update) {
  ChromeAppListItem* item = GetAppItem(update.AppId());
  bool show = ShouldShowInLauncher(update);
  if (item) {
    if (show) {
      DCHECK(item->GetItemType() == AppServiceAppItem::kItemType);
      static_cast<AppServiceAppItem*>(item)->OnAppUpdate(update);

      // TODO(crbug.com/826982): drop the check for kExtension or kWeb, and
      // call UpdateItem unconditionally?
      apps::mojom::AppType app_type = update.AppType();
      if ((app_type == apps::mojom::AppType::kExtension) ||
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
      RemoveApp(update.AppId(), unsynced_change);
    }

  } else if (show) {
    InsertApp(std::make_unique<AppServiceAppItem>(
        profile(), model_updater(), GetSyncItem(update.AppId()), update));
  }
}

void AppServiceAppModelBuilder::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}
