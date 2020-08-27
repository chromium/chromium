// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"

#include "ash/public/cpp/file_icon_util.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

namespace {

PrefService* GetPrefService(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);
  return profile->GetPrefs();
}

}  // namespace

// static
constexpr char HoldingSpaceKeyedService::kPersistencePath[];

HoldingSpaceKeyedService::HoldingSpaceKeyedService(
    content::BrowserContext* context,
    const AccountId& account_id)
    : browser_context_(context),
      holding_space_client_(Profile::FromBrowserContext(context)) {
  RestoreModel();
  holding_space_model_observer_.Add(&holding_space_model_);
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id, &holding_space_client_, &holding_space_model_);

  // Observe the profile manager to get notified when the profile creation
  // finishes to start handling user downloads. The keyed service is created
  // with the profile, and at this stage the download manager may not be
  // ready to be used.
  if (g_browser_process->profile_manager()->IsValidProfile(
          Profile::FromBrowserContext(browser_context_))) {
    download_manager_ =
        content::BrowserContext::GetDownloadManager(browser_context_);
    download_manager_->AddObserver(this);
  } else {
    observed_profile_manager_.Add(g_browser_process->profile_manager());
  }
}

HoldingSpaceKeyedService::~HoldingSpaceKeyedService() = default;

void HoldingSpaceKeyedService::Shutdown() {
  RemoveDownloadManagerObservers();
}

// static
void HoldingSpaceKeyedService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kPersistencePath);
}

void HoldingSpaceKeyedService::AddScreenshot(
    const base::FilePath& screenshot_file,
    const gfx::ImageSkia& image) {
  GURL file_system_url = ResolveFileSystemUrl(screenshot_file);
  if (file_system_url.is_empty())
    return;

  auto item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kScreenshot, screenshot_file, file_system_url,
      image);
  holding_space_model_.AddItem(std::move(item));
}

void HoldingSpaceKeyedService::AddDownload(
    const base::FilePath& download_file) {
  GURL file_system_url = ResolveFileSystemUrl(download_file);
  if (file_system_url.is_empty())
    return;

  auto item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kDownload, download_file, file_system_url,
      ResolveImage(download_file));
  holding_space_model_.AddItem(std::move(item));
}

void HoldingSpaceKeyedService::OnHoldingSpaceItemAdded(
    const HoldingSpaceItem* item) {
  // `kDownload` type holding space items have their own persistence mechanism.
  if (item->type() == HoldingSpaceItem::Type::kDownload)
    return;

  // Write the new |item| to persistent storage.
  ListPrefUpdate update(GetPrefService(browser_context_), kPersistencePath);
  update->Append(item->Serialize());
}

void HoldingSpaceKeyedService::OnHoldingSpaceItemRemoved(
    const HoldingSpaceItem* item) {
  // `kDownload` type holding space items have their own persistence mechanism.
  if (item->type() == HoldingSpaceItem::Type::kDownload)
    return;

  // Remove the |item| from persistent storage.
  ListPrefUpdate update(GetPrefService(browser_context_), kPersistencePath);
  update->EraseListValueIf([&item](const base::Value& existing_item) {
    return HoldingSpaceItem::DeserializeId(
               base::Value::AsDictionaryValue(existing_item)) == item->id();
  });
}

// TODO(dmblack): Restore download holding space items.
void HoldingSpaceKeyedService::RestoreModel() {
  DCHECK(holding_space_model_.items().empty());
  const auto* holding_space_items =
      GetPrefService(browser_context_)->GetList(kPersistencePath);
  for (const auto& holding_space_item : holding_space_items->GetList()) {
    holding_space_model_.AddItem(HoldingSpaceItem::Deserialize(
        base::Value::AsDictionaryValue(holding_space_item),
        base::BindOnce(&HoldingSpaceKeyedService::ResolveFileSystemUrl,
                       base::Unretained(this)),
        base::BindOnce(&HoldingSpaceKeyedService::ResolveImage,
                       base::Unretained(this))));
  }
}

GURL HoldingSpaceKeyedService::ResolveFileSystemUrl(
    const base::FilePath& file_path) const {
  GURL file_system_url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          Profile::FromBrowserContext(browser_context_), file_path,
          file_manager::kFileManagerAppId, &file_system_url)) {
    VLOG(2) << "Unable to convert file path to File System URL.";
  }
  return file_system_url;
}

// TODO(dmblack): Use thumbnail service to asynchronously replace placeholders.
gfx::ImageSkia HoldingSpaceKeyedService::ResolveImage(
    const base::FilePath& file_path) const {
  return GetIconForPath(file_path);
}

void HoldingSpaceKeyedService::SetDownloadManagerForTesting(
    content::DownloadManager* manager) {
  RemoveDownloadManagerObservers();
  download_manager_ = manager;
  download_manager_->AddObserver(this);
}

void HoldingSpaceKeyedService::OnProfileAdded(Profile* profile) {
  if (!profile->IsSameOrParent(Profile::FromBrowserContext(browser_context_)))
    return;

  observed_profile_manager_.RemoveAll();

  // Download Manager may have been already set in tests.
  if (download_manager_)
    return;

  download_manager_ =
      content::BrowserContext::GetDownloadManager(browser_context_);
  download_manager_->AddObserver(this);
}

void HoldingSpaceKeyedService::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  download_items_observer_.Add(item);
}

void HoldingSpaceKeyedService::OnDownloadDropped(
    content::DownloadManager* manager) {}

void HoldingSpaceKeyedService::OnManagerInitialized() {}

void HoldingSpaceKeyedService::ManagerGoingDown(
    content::DownloadManager* manager) {
  RemoveDownloadManagerObservers();
  download_manager_ = nullptr;
}

void HoldingSpaceKeyedService::OnDownloadUpdated(download::DownloadItem* item) {
  download::DownloadItem::DownloadState state = item->GetState();
  if (state == download::DownloadItem::COMPLETE ||
      state == download::DownloadItem::CANCELLED ||
      state == download::DownloadItem::INTERRUPTED) {
    // Stop observing now to ensure we only send one complete/fail notification.
    download_items_observer_.Remove(item);

    if (state == download::DownloadItem::COMPLETE) {
      const base::FilePath download_path = item->GetFullPath();
      AddDownload(download_path);
    }
  }
}

void HoldingSpaceKeyedService::RemoveDownloadManagerObservers() {
  if (!download_manager_)
    return;

  download_manager_->RemoveObserver(this);
  download_items_observer_.RemoveAll();
}

}  // namespace ash
