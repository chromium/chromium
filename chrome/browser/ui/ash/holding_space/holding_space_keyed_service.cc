// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_file_system_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_persistence_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "components/account_id/account_id.h"
#include "storage/browser/file_system/file_system_url.h"

namespace ash {

namespace {

ProfileManager* GetProfileManager() {
  return g_browser_process->profile_manager();
}

}  // namespace

// TODO(dmblack): Add a delegate for downloads.
HoldingSpaceKeyedService::HoldingSpaceKeyedService(
    content::BrowserContext* context,
    const AccountId& account_id)
    : browser_context_(context),
      account_id_(account_id),
      holding_space_client_(Profile::FromBrowserContext(context)),
      thumbnail_loader_(Profile::FromBrowserContext(context)) {
  // The associated profile may not be ready yet. If it is, we can immediately
  // proceed with profile dependent initialization.
  ProfileManager* const profile_manager = GetProfileManager();
  if (profile_manager->IsValidProfile(Profile::FromBrowserContext(context))) {
    OnProfileReady();
    return;
  }
  // Otherwise we need to wait for the profile to be added.
  profile_manager_observer_.Add(profile_manager);
}

HoldingSpaceKeyedService::~HoldingSpaceKeyedService() = default;

// static
void HoldingSpaceKeyedService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  HoldingSpacePersistenceDelegate::RegisterProfilePrefs(registry);
}

void HoldingSpaceKeyedService::AddPinnedFile(
    const storage::FileSystemURL& file_system_url) {
  AddItem(HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kPinnedFile, file_system_url.path(),
      file_system_url.ToGURL(),
      holding_space_util::ResolveImage(file_system_url.path())));
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
  GURL file_system_url = holding_space_util::ResolveFileSystemUrl(
      Profile::FromBrowserContext(browser_context_), screenshot_file);
  if (file_system_url.is_empty())
    return;

  AddItem(HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kScreenshot, screenshot_file, file_system_url,
      image));
}

void HoldingSpaceKeyedService::AddDownload(
    const base::FilePath& download_file) {
  GURL file_system_url = holding_space_util::ResolveFileSystemUrl(
      Profile::FromBrowserContext(browser_context_), download_file);
  if (file_system_url.is_empty())
    return;

  AddItem(HoldingSpaceItem::CreateFileBackedItem(
      HoldingSpaceItem::Type::kDownload, download_file, file_system_url,
      holding_space_util::ResolveImage(download_file)));
}

void HoldingSpaceKeyedService::AddItem(std::unique_ptr<HoldingSpaceItem> item) {
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

void HoldingSpaceKeyedService::OnProfileAdded(Profile* profile) {
  if (profile == Profile::FromBrowserContext(browser_context_)) {
    profile_manager_observer_.Remove(GetProfileManager());
    OnProfileReady();
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

void HoldingSpaceKeyedService::OnProfileReady() {
  Profile* profile = Profile::FromBrowserContext(browser_context_);

  // The `HoldingSpaceFileSystemDelegate` monitors the file system for changes.
  delegates_.push_back(std::make_unique<HoldingSpaceFileSystemDelegate>(
      profile, &holding_space_model_,
      /*file_removed_callback=*/
      base::BindRepeating(&HoldingSpaceKeyedService::OnFileRemoved,
                          weak_factory_.GetWeakPtr())));

  // The `HoldingSpacePersistenceDelegate` manages holding space persistence.
  delegates_.push_back(std::make_unique<HoldingSpacePersistenceDelegate>(
      profile, &holding_space_model_,
      /*item_restored_callback=*/
      base::BindRepeating(&HoldingSpaceKeyedService::AddItem,
                          weak_factory_.GetWeakPtr()),
      /*model_restored_callback=*/
      base::BindOnce(&HoldingSpaceKeyedService::OnModelRestored,
                     weak_factory_.GetWeakPtr())));

  // Initialize all delegates only after they have been added to our collection.
  // Delegates should not fire their respective callbacks during construction
  // but once they have been initialized they are free to do so.
  for (auto& delegate : delegates_)
    delegate->Init();
}

void HoldingSpaceKeyedService::OnFileRemoved(const base::FilePath& file_path) {
  // When `file_path` is removed, we need to remove any associated items.
  holding_space_model_.RemoveIf(base::BindRepeating(
      [](const base::FilePath& file_path, const HoldingSpaceItem* item) {
        return item->file_path() == file_path;
      },
      std::cref(file_path)));
}

void HoldingSpaceKeyedService::OnModelRestored() {
  for (auto& delegate : delegates_)
    delegate->NotifyHoldingSpaceModelRestored();

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

}  // namespace ash
