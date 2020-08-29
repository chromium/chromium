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
#include "chrome/browser/ui/ash/holding_space/holding_space_file_system_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "storage/browser/file_system/file_system_url.h"

namespace ash {

namespace {

PrefService* GetPrefService(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);
  return profile->GetPrefs();
}

ProfileManager* GetProfileManager() {
  return g_browser_process->profile_manager();
}

}  // namespace

// static
constexpr char HoldingSpaceKeyedService::kPersistencePath[];

HoldingSpaceKeyedService::HoldingSpaceKeyedService(
    content::BrowserContext* context,
    const AccountId& account_id)
    : browser_context_(context),
      account_id_(account_id),
      holding_space_client_(Profile::FromBrowserContext(context)),
      thumbnail_loader_(Profile::FromBrowserContext(context)) {
  // TODO(dmblack): Add delegates for downloads and persistence.
  delegates_.push_back(std::make_unique<HoldingSpaceFileSystemDelegate>(
      &holding_space_model_,
      base::BindRepeating(&HoldingSpaceKeyedService::OnFileRemoved,
                          weak_factory_.GetWeakPtr())));

  // If the service's associated profile is ready, we can proceed to restore the
  // `holding_space_model_` from persistence.
  ProfileManager* const profile_manager = GetProfileManager();
  if (profile_manager->IsValidProfile(Profile::FromBrowserContext(context))) {
    RestoreModelFromPersistence();
    return;
  }
  // Otherwise we need to wait for the profile to be added.
  profile_manager_observer_.Add(profile_manager);
}

HoldingSpaceKeyedService::~HoldingSpaceKeyedService() = default;

// static
void HoldingSpaceKeyedService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kPersistencePath);
}

void HoldingSpaceKeyedService::AddPinnedFile(
    const storage::FileSystemURL& file_system_url) {
  auto item = HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kPinnedFile, file_system_url.path(),
      file_system_url.ToGURL(), gfx::ImageSkia());
  holding_space_model_.AddItem(std::move(item));
}

void HoldingSpaceKeyedService::RemovePinnedFile(
    const storage::FileSystemURL& file_system_url) {
  holding_space_model_.RemoveItem(HoldingSpaceItem::GetFileBackedItemId(
      HoldingSpaceItem::Type::kPinnedFile, file_system_url.path()));
}

bool HoldingSpaceKeyedService::ContainsPinnedFile(
    const storage::FileSystemURL& file_system_url) const {
  return holding_space_model_.GetItem(HoldingSpaceItem::GetFileBackedItemId(
      HoldingSpaceItem::Type::kPinnedFile, file_system_url.path()));
}

std::vector<GURL> HoldingSpaceKeyedService::GetPinnedFiles() const {
  std::vector<GURL> pinned_files;
  for (const auto& item : holding_space_model_.items()) {
    if (item->type() == HoldingSpaceItem::Type::kPinnedFile)
      pinned_files.push_back(item->file_system_url());
  }
  return pinned_files;
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

void HoldingSpaceKeyedService::SetDownloadManagerForTesting(
    content::DownloadManager* manager) {
  RemoveDownloadManagerObservers();
  download_manager_ = manager;
  download_manager_->AddObserver(this);
}

void HoldingSpaceKeyedService::Shutdown() {
  RemoveDownloadManagerObservers();
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

void HoldingSpaceKeyedService::OnProfileAdded(Profile* profile) {
  if (profile == Profile::FromBrowserContext(browser_context_)) {
    profile_manager_observer_.Remove(GetProfileManager());
    RestoreModelFromPersistence();
  }
}

void HoldingSpaceKeyedService::ManagerGoingDown(
    content::DownloadManager* manager) {
  RemoveDownloadManagerObservers();
  download_manager_ = nullptr;
}

void HoldingSpaceKeyedService::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  download_items_observer_.Add(item);
}

void HoldingSpaceKeyedService::OnDownloadUpdated(download::DownloadItem* item) {
  switch (item->GetState()) {
    case download::DownloadItem::COMPLETE:
      AddDownload(item->GetFullPath());
      FALLTHROUGH;
    case download::DownloadItem::CANCELLED:
    case download::DownloadItem::INTERRUPTED:
      download_items_observer_.Remove(item);
      break;
    case download::DownloadItem::IN_PROGRESS:
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      break;
  }
}

void HoldingSpaceKeyedService::RemoveDownloadManagerObservers() {
  if (download_manager_)
    download_manager_->RemoveObserver(this);
  download_items_observer_.RemoveAll();
}

// TODO(dmblack): Restore download holding space items.
void HoldingSpaceKeyedService::RestoreModelFromPersistence() {
  DCHECK(holding_space_model_.items().empty());

  const auto* persisted_holding_space_items =
      GetPrefService(browser_context_)->GetList(kPersistencePath);

  if (persisted_holding_space_items->GetList().empty()) {
    OnModelRestored();
    return;
  }

  std::vector<HoldingSpaceItemPtr> holding_space_items;
  for (const auto& persisted_holding_space_item :
       persisted_holding_space_items->GetList()) {
    holding_space_items.push_back(HoldingSpaceItem::Deserialize(
        base::Value::AsDictionaryValue(persisted_holding_space_item),
        base::BindOnce(&HoldingSpaceKeyedService::ResolveFileSystemUrl,
                       base::Unretained(this)),
        base::BindOnce(&HoldingSpaceKeyedService::ResolveImage,
                       base::Unretained(this))));
  }

  holding_space_util::PartitionItemsByExistence(
      Profile::FromBrowserContext(browser_context_),
      std::move(holding_space_items),
      base::BindOnce(&HoldingSpaceKeyedService::RestoreModelByExistence,
                     weak_factory_.GetWeakPtr()));
}

void HoldingSpaceKeyedService::RestoreModelByExistence(
    std::vector<HoldingSpaceItemPtr> existing_items,
    std::vector<HoldingSpaceItemPtr> non_existing_items) {
  DCHECK(holding_space_model_.items().empty());

  // Restore existing holding space items.
  for (auto& holding_space_item : existing_items)
    holding_space_model_.AddItem(std::move(holding_space_item));

  // Clean up non-existing holding space items from persistence.
  if (!non_existing_items.empty()) {
    ListPrefUpdate update(GetPrefService(browser_context_), kPersistencePath);
    update->EraseListValueIf([&non_existing_items](
                                 const base::Value& persisted_item) {
      const std::string& persisted_item_id = HoldingSpaceItem::DeserializeId(
          base::Value::AsDictionaryValue(persisted_item));
      return std::any_of(
          non_existing_items.begin(), non_existing_items.end(),
          [&persisted_item_id](const HoldingSpaceItemPtr& non_existing_item) {
            return non_existing_item->id() == persisted_item_id;
          });
    });
  }

  OnModelRestored();
}

void HoldingSpaceKeyedService::OnModelRestored() {
  holding_space_model_observer_.Add(&holding_space_model_);
  HoldingSpaceController::Get()->RegisterClientAndModelForUser(
      account_id_, &holding_space_client_, &holding_space_model_);

  // NOTE: `download_manager_` may have already been set in tests.
  if (download_manager_)
    return;

  // Once the `holding_space_model_` has been restored from persistence, we can
  // start to observe the `download_manager_` to track downloaded files.
  download_manager_ =
      content::BrowserContext::GetDownloadManager(browser_context_);
  download_manager_->AddObserver(this);
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

void HoldingSpaceKeyedService::OnFileRemoved(const base::FilePath& file_path) {
  // When `file_path` is removed, we need to remove any associated items.
  holding_space_model_.RemoveIf(base::BindRepeating(
      [](const base::FilePath& file_path, const HoldingSpaceItem* item) {
        return item->file_path() == file_path;
      },
      std::cref(file_path)));
}

}  // namespace ash
