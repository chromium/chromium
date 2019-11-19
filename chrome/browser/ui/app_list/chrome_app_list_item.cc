// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/chrome_app_list_item.h"

#include <utility>

#include "ash/public/cpp/tablet_mode.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/chrome_app_list_model_updater.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_system.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_operations.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#endif

namespace {

AppListControllerDelegate* g_controller_for_test = nullptr;

std::unique_ptr<ash::AppListItemMetadata> CreateDefaultMetadata(
    const std::string& app_id) {
  auto metadata = std::make_unique<ash::AppListItemMetadata>();
  metadata->id = app_id;
  return metadata;
}

}  // namespace

// static
void ChromeAppListItem::OverrideAppListControllerDelegateForTesting(
    AppListControllerDelegate* controller) {
  g_controller_for_test = controller;
}

// static
gfx::ImageSkia ChromeAppListItem::CreateDisabledIcon(
    const gfx::ImageSkia& icon) {
  const color_utils::HSL shift = {-1, 0, 0.6};
  return gfx::ImageSkiaOperations::CreateHSLShiftedImage(icon, shift);
}

// ChromeAppListItem::TestApi
ChromeAppListItem::TestApi::TestApi(ChromeAppListItem* item) : item_(item) {}

void ChromeAppListItem::TestApi::SetFolderId(const std::string& folder_id) {
  item_->SetFolderId(folder_id);
}

void ChromeAppListItem::TestApi::SetPosition(
    const syncer::StringOrdinal& position) {
  item_->SetPosition(position);
}

// ChromeAppListItem
ChromeAppListItem::ChromeAppListItem(Profile* profile,
                                     const std::string& app_id)
    : metadata_(CreateDefaultMetadata(app_id)), profile_(profile) {}

ChromeAppListItem::ChromeAppListItem(Profile* profile,
                                     const std::string& app_id,
                                     AppListModelUpdater* model_updater)
    : metadata_(CreateDefaultMetadata(app_id)),
      profile_(profile),
      model_updater_(model_updater) {}

ChromeAppListItem::~ChromeAppListItem() = default;

void ChromeAppListItem::SetIsInstalling(bool is_installing) {
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetItemIsInstalling(id(), is_installing);
}

void ChromeAppListItem::SetPercentDownloaded(int32_t percent_downloaded) {
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetItemPercentDownloaded(id(), percent_downloaded);
}

void ChromeAppListItem::SetMetadata(
    std::unique_ptr<ash::AppListItemMetadata> metadata) {
  metadata_ = std::move(metadata);
}

std::unique_ptr<ash::AppListItemMetadata> ChromeAppListItem::CloneMetadata()
    const {
  return std::make_unique<ash::AppListItemMetadata>(*metadata_);
}

void ChromeAppListItem::PerformActivate(int event_flags) {
#if defined(OS_CHROMEOS)
  // Handle recording app launch source from the AppList in Demo Mode.
  chromeos::DemoSession::RecordAppLaunchSourceIfInDemoMode(
      chromeos::DemoSession::AppLaunchSource::kAppList);
#endif
  Activate(event_flags);
  MaybeDismissAppList();
}

void ChromeAppListItem::Activate(int event_flags) {}

const char* ChromeAppListItem::GetItemType() const {
  return "";
}

void ChromeAppListItem::GetContextMenuModel(GetMenuModelCallback callback) {
  std::move(callback).Run(nullptr);
}

bool ChromeAppListItem::IsBadged() const {
  return false;
}

app_list::AppContextMenu* ChromeAppListItem::GetAppContextMenu() {
  return nullptr;
}

void ChromeAppListItem::MaybeDismissAppList() {
  // Launching apps can take some time. It looks nicer to dismiss the app list.
  // Do not close app list for home launcher.
  if (!ash::TabletMode::Get() || !ash::TabletMode::Get()->InTabletMode()) {
    GetController()->DismissView();
  }
}

extensions::AppSorting* ChromeAppListItem::GetAppSorting() {
  return extensions::ExtensionSystem::Get(profile())->app_sorting();
}

AppListControllerDelegate* ChromeAppListItem::GetController() {
  return g_controller_for_test != nullptr ? g_controller_for_test
                                          : AppListClientImpl::GetInstance();
}

void ChromeAppListItem::UpdateFromSync(
    const app_list::AppListSyncableService::SyncItem* sync_item) {
  DCHECK(sync_item && sync_item->item_ordinal.IsValid());
  // An existing synced position exists, use that.
  SetPosition(sync_item->item_ordinal);
  // Only set the name from the sync item if it is empty.
  if (name().empty())
    SetName(sync_item->item_name);
}

void ChromeAppListItem::SetDefaultPositionIfApplicable(
    AppListModelUpdater* model_updater) {
  syncer::StringOrdinal page_ordinal;
  syncer::StringOrdinal launch_ordinal;
  extensions::AppSorting* app_sorting = GetAppSorting();
  if (app_sorting->GetDefaultOrdinals(id(), &page_ordinal, &launch_ordinal) &&
      page_ordinal.IsValid() && launch_ordinal.IsValid()) {
    // Set the default position if it exists.
    SetPosition(syncer::StringOrdinal(page_ordinal.ToInternalValue() +
                                      launch_ordinal.ToInternalValue()));
    return;
  }

  if (model_updater) {
    // Set the first available position in the app list.
    SetPosition(model_updater->GetFirstAvailablePosition());
    return;
  }

  // Set the natural position.
  app_sorting->EnsureValidOrdinals(id(), syncer::StringOrdinal());
  page_ordinal = app_sorting->GetPageOrdinal(id());
  launch_ordinal = app_sorting->GetAppLaunchOrdinal(id());
  SetPosition(syncer::StringOrdinal(page_ordinal.ToInternalValue() +
                                    launch_ordinal.ToInternalValue()));
}

void ChromeAppListItem::SetIcon(const gfx::ImageSkia& icon) {
  metadata_->icon = icon;
  metadata_->icon.EnsureRepsForSupportedScales();
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetItemIcon(id(), metadata_->icon);
}

void ChromeAppListItem::SetName(const std::string& name) {
  metadata_->name = name;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetItemName(id(), name);
}

void ChromeAppListItem::SetNameAndShortName(const std::string& name,
                                            const std::string& short_name) {
  metadata_->name = name;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetItemNameAndShortName(id(), name, short_name);
}

void ChromeAppListItem::SetFolderId(const std::string& folder_id) {
  metadata_->folder_id = folder_id;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetItemFolderId(id(), folder_id);
}

void ChromeAppListItem::SetPosition(const syncer::StringOrdinal& position) {
  metadata_->position = position;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetItemPosition(id(), position);
}

void ChromeAppListItem::SetIsPersistent(bool is_persistent) {
  metadata_->is_persistent = is_persistent;
  AppListModelUpdater* updater = model_updater();
  if (updater)
    updater->SetItemIsPersistent(id(), is_persistent);
}

void ChromeAppListItem::SetIsPageBreak(bool is_page_break) {
  metadata_->is_page_break = is_page_break;
}

void ChromeAppListItem::SetChromeFolderId(const std::string& folder_id) {
  metadata_->folder_id = folder_id;
}

void ChromeAppListItem::SetChromeIsFolder(bool is_folder) {
  metadata_->is_folder = is_folder;
}

void ChromeAppListItem::SetChromeName(const std::string& name) {
  metadata_->name = name;
}

void ChromeAppListItem::SetChromePosition(
    const syncer::StringOrdinal& position) {
  metadata_->position = position;
}

bool ChromeAppListItem::CompareForTest(const ChromeAppListItem* other) const {
  return id() == other->id() && folder_id() == other->folder_id() &&
         name() == other->name() && GetItemType() == other->GetItemType() &&
         position().Equals(other->position());
}

std::string ChromeAppListItem::ToDebugString() const {
  return id().substr(0, 8) + " '" + name() + "' (" + folder_id() + ") [" +
         position().ToDebugString() + "]";
}
