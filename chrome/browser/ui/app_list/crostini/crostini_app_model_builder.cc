// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_app_model_builder.h"

#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_item.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/l10n/l10n_util.h"

CrostiniAppModelBuilder::CrostiniAppModelBuilder(
    AppListControllerDelegate* controller)
    : AppListModelBuilder(controller, CrostiniAppItem::kItemType) {}

CrostiniAppModelBuilder::~CrostiniAppModelBuilder() {
  // We don't need to remove ourself from the registry's observer list as these
  // are both KeyedServices (this class lives on AppListSyncableService).
}

void CrostiniAppModelBuilder::BuildModel() {
  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile());
  for (const std::string& app_id : registry_service->GetRegisteredAppIds()) {
    InsertCrostiniAppItem(registry_service, app_id);
  }

  registry_service->AddObserver(this);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile()->GetPrefs());
  pref_change_registrar_->Add(
      crostini::prefs::kCrostiniEnabled,
      base::BindRepeating(&CrostiniAppModelBuilder::OnCrostiniEnabledChanged,
                          base::Unretained(this)));
}

void CrostiniAppModelBuilder::InsertCrostiniAppItem(
    const crostini::CrostiniRegistryService* registry_service,
    const std::string& app_id) {
  if (app_id == crostini::kCrostiniTerminalId &&
      !crostini::IsCrostiniEnabled(profile())) {
    // If Crostini isn't enabled, don't show the Terminal item until it
    // becomes enabled.
    return;
  }
  crostini::CrostiniRegistryService::Registration registration =
      *registry_service->GetRegistration(app_id);
  if (registration.NoDisplay())
    return;

  MaybeCreateRootFolder();
  InsertApp(std::make_unique<CrostiniAppItem>(profile(), model_updater(),
                                              GetSyncItem(app_id), app_id,
                                              registration.Name()));
}

void CrostiniAppModelBuilder::OnRegistryUpdated(
    crostini::CrostiniRegistryService* registry_service,
    const std::vector<std::string>& updated_apps,
    const std::vector<std::string>& removed_apps,
    const std::vector<std::string>& inserted_apps) {
  const bool unsynced_change = false;
  for (const std::string& app_id : removed_apps)
    RemoveApp(app_id, unsynced_change);
  for (const std::string& app_id : updated_apps) {
    crostini::CrostiniRegistryService::Registration registration =
        *registry_service->GetRegistration(app_id);
    if (registration.NoDisplay()) {
      RemoveApp(app_id, unsynced_change);
      continue;
    }

    CrostiniAppItem* app_item =
        static_cast<CrostiniAppItem*>(GetAppItem(app_id));
    if (!app_item) {
      InsertCrostiniAppItem(registry_service, app_id);
      continue;
    }
    app_item->SetName(registration.Name());
  }
  for (const std::string& app_id : inserted_apps) {
    // If the app has been installed before and has not been cleaned up
    // correctly, it needs to be removed.
    RemoveApp(app_id, unsynced_change);
    InsertCrostiniAppItem(registry_service, app_id);
  }
}

void CrostiniAppModelBuilder::OnAppIconUpdated(const std::string& app_id,
                                               ui::ScaleFactor scale_factor) {
  CrostiniAppItem* app_item = static_cast<CrostiniAppItem*>(GetAppItem(app_id));
  if (!app_item) {
    VLOG(2) << "Could not update the icon of Crostini app (" << app_id
            << ") because it was not found";
    return;
  }

  // Initiate async icon reloading.
  app_item->crostini_app_icon()->LoadForScaleFactor(scale_factor);
}

void CrostiniAppModelBuilder::OnCrostiniEnabledChanged() {
  const bool unsynced_change = false;
  if (crostini::IsCrostiniEnabled(profile())) {
    // If Terminal has been installed before and has not been cleaned up
    // correctly, it needs to be removed.
    RemoveApp(crostini::kCrostiniTerminalId, unsynced_change);
    crostini::CrostiniRegistryService* registry_service =
        crostini::CrostiniRegistryServiceFactory::GetForProfile(profile());
    InsertCrostiniAppItem(registry_service, crostini::kCrostiniTerminalId);
  } else {
    RemoveApp(crostini::kCrostiniTerminalId, unsynced_change);
  }
}

void CrostiniAppModelBuilder::MaybeCreateRootFolder() {
  if (root_folder_created_)
    return;

  root_folder_created_ = true;
  const app_list::AppListSyncableService::SyncItem* sync_item =
      GetSyncItem(crostini::kCrostiniFolderId);
  if (sync_item)
    return;

  std::unique_ptr<ChromeAppListItem> crositini_folder =
      std::make_unique<ChromeAppListItem>(
          profile(), crostini::kCrostiniFolderId, model_updater());
  crositini_folder->SetChromeIsFolder(true);
  crositini_folder->SetName(
      l10n_util::GetStringUTF8(IDS_APP_LIST_CROSTINI_DEFAULT_FOLDER_NAME));
  crositini_folder->SetDefaultPositionIfApplicable(model_updater());
  InsertApp(std::move(crositini_folder));
}
