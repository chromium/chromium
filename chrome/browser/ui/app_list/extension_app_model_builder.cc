// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_model_builder.h"

#include <algorithm>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/extension_app_item.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/ash/launcher/launcher_extension_app_updater.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

using extensions::Extension;

ExtensionAppModelBuilder::ExtensionAppModelBuilder(
    AppListControllerDelegate* controller)
    : AppListModelBuilder(controller, ExtensionAppItem::kItemType) {
}

ExtensionAppModelBuilder::~ExtensionAppModelBuilder() {
  OnShutdown();
}

void ExtensionAppModelBuilder::InitializePrefChangeRegistrars() {
  profile_pref_change_registrar_.Init(profile()->GetPrefs());
  profile_pref_change_registrar_.Add(
      prefs::kHideWebStoreIcon,
      base::Bind(&ExtensionAppModelBuilder::OnProfilePreferenceChanged,
                 base::Unretained(this)));
}

void ExtensionAppModelBuilder::OnProfilePreferenceChanged() {
  extensions::ExtensionSet extensions;
  controller()->GetApps(profile(), &extensions);

  for (extensions::ExtensionSet::const_iterator app = extensions.begin();
       app != extensions.end(); ++app) {
    bool should_display = app_list::ShouldShowInLauncher(app->get(), profile());
    bool does_display = GetExtensionAppItem((*app)->id()) != nullptr;

    if (should_display == does_display)
      continue;

    if (should_display) {
      InsertApp(CreateAppItem((*app)->id(),
                              "",
                              gfx::ImageSkia(),
                              (*app)->is_platform_app()));
    } else {
      RemoveApp((*app)->id(), false);
    }
  }
}

void ExtensionAppModelBuilder::OnBeginExtensionInstall(
    const ExtensionInstallParams& params) {
  if (!params.is_app)
    return;

  DVLOG(2) << service() << ": OnBeginExtensionInstall: "
           << params.extension_id.substr(0, 8);
  ExtensionAppItem* existing_item = GetExtensionAppItem(params.extension_id);
  if (existing_item) {
    existing_item->SetIsInstalling(true);
    return;
  }

  if (app_list::HideInLauncherById(params.extension_id))
    return;

  // Icons from the webstore can be unusual sizes. Once installed,
  // ExtensionAppItem uses ash::AppListConfig::instance().grid_icon_dimension()
  // to load it, so be consistent with that.
  const int icon_dimension =
      ash::AppListConfig::instance().grid_icon_dimension();
  gfx::Size icon_size(icon_dimension, icon_dimension);
  gfx::ImageSkia resized(gfx::ImageSkiaOperations::CreateResizedImage(
      params.installing_icon, skia::ImageOperations::RESIZE_BEST, icon_size));

  InsertApp(CreateAppItem(params.extension_id,
                          params.extension_name,
                          resized,
                          params.is_platform_app));
}

void ExtensionAppModelBuilder::OnDownloadProgress(
    const std::string& extension_id,
    int percent_downloaded) {
  ExtensionAppItem* item = GetExtensionAppItem(extension_id);
  if (!item)
    return;
  item->SetPercentDownloaded(percent_downloaded);
}

void ExtensionAppModelBuilder::OnInstallFailure(
    const std::string& extension_id) {
  model_updater()->RemoveItem(extension_id);
}

void ExtensionAppModelBuilder::OnAppInstalled(
    content::BrowserContext* browser_context,
    const std::string& app_id) {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile());

  const Extension* extension =
      extension_registry ? extension_registry->GetInstalledExtension(app_id)
                         : nullptr;
  if (!extension) {
    NOTREACHED();
    return;
  }

  if (!app_list::ShouldShowInLauncher(extension, profile()))
    return;

  DVLOG(2) << service() << ": OnAppInstalled: " << app_id.substr(0, 8);
  ExtensionAppItem* existing_item = GetExtensionAppItem(app_id);
  if (existing_item) {
    existing_item->Reload();
    if (service())
      service()->UpdateItem(existing_item);
    return;
  }

  InsertApp(CreateAppItem(app_id, "", gfx::ImageSkia(),
                          extension->is_platform_app()));
}

void ExtensionAppModelBuilder::OnAppUninstalled(
    content::BrowserContext* browser_context,
    const std::string& app_id) {
  const bool unsynced_change = false;
  RemoveApp(app_id, unsynced_change);
}

void ExtensionAppModelBuilder::OnDisabledExtensionUpdated(
    const Extension* extension) {
  if (!app_list::ShouldShowInLauncher(extension, profile()))
    return;

  ExtensionAppItem* existing_item = GetExtensionAppItem(extension->id());
  if (existing_item)
    existing_item->Reload();
}

void ExtensionAppModelBuilder::OnShutdown() {
  if (tracker_) {
    tracker_->RemoveObserver(this);
    tracker_ = nullptr;
  }
  app_updater_.reset();
}

std::unique_ptr<ExtensionAppItem> ExtensionAppModelBuilder::CreateAppItem(
    const std::string& extension_id,
    const std::string& extension_name,
    const gfx::ImageSkia& installing_icon,
    bool is_platform_app) {
  return std::make_unique<ExtensionAppItem>(
      profile(), model_updater(), GetSyncItem(extension_id), extension_id,
      extension_name, installing_icon, is_platform_app);
}

void ExtensionAppModelBuilder::BuildModel() {
  DCHECK(!tracker_);

  InitializePrefChangeRegistrars();

  tracker_ = controller()->GetInstallTrackerFor(profile());

  PopulateApps();

  // Start observing after model is built.
  if (tracker_)
    tracker_->AddObserver(this);

  app_updater_ = std::make_unique<LauncherExtensionAppUpdater>(this, profile());
}

void ExtensionAppModelBuilder::PopulateApps() {
  extensions::ExtensionSet extensions;
  controller()->GetApps(profile(), &extensions);

  for (extensions::ExtensionSet::const_iterator app = extensions.begin();
       app != extensions.end(); ++app) {
    if (!app_list::ShouldShowInLauncher(app->get(), profile()))
      continue;
    InsertApp(CreateAppItem((*app)->id(),
                            "",
                            gfx::ImageSkia(),
                            (*app)->is_platform_app()));
  }
}

ExtensionAppItem* ExtensionAppModelBuilder::GetExtensionAppItem(
    const std::string& extension_id) {
  return static_cast<ExtensionAppItem*>(GetAppItem(extension_id));
}
