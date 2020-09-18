// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_downloads_delegate.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "base/barrier_closure.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "content/public/browser/browser_context.h"

namespace ash {

namespace {

content::DownloadManager* download_manager_for_testing = nullptr;

}  // namespace

HoldingSpaceDownloadsDelegate::HoldingSpaceDownloadsDelegate(
    Profile* profile,
    HoldingSpaceModel* model,
    ItemDownloadedCallback item_downloaded_callback,
    DownloadsRestoredCallback downloads_restored_callback)
    : HoldingSpaceKeyedServiceDelegate(profile, model),
      item_downloaded_callback_(item_downloaded_callback),
      downloads_restored_callback_(std::move(downloads_restored_callback)) {}

HoldingSpaceDownloadsDelegate::~HoldingSpaceDownloadsDelegate() = default;

// static
void HoldingSpaceDownloadsDelegate::SetDownloadManagerForTesting(
    content::DownloadManager* download_manager) {
  download_manager_for_testing = download_manager;
}

void HoldingSpaceDownloadsDelegate::Init() {
  download_manager_observer_.Add(
      download_manager_for_testing
          ? download_manager_for_testing
          : content::BrowserContext::GetDownloadManager(profile()));
}

void HoldingSpaceDownloadsDelegate::Shutdown() {
  RemoveObservers();
}

void HoldingSpaceDownloadsDelegate::OnPersistenceRestored() {
  content::DownloadManager* download_manager =
      download_manager_for_testing
          ? download_manager_for_testing
          : content::BrowserContext::GetDownloadManager(profile());

  if (download_manager->IsManagerInitialized())
    OnManagerInitialized();
}

void HoldingSpaceDownloadsDelegate::OnManagerInitialized() {
  if (is_restoring_persistence())
    return;

  content::DownloadManager* download_manager =
      download_manager_for_testing
          ? download_manager_for_testing
          : content::BrowserContext::GetDownloadManager(profile());

  DCHECK(download_manager->IsManagerInitialized());

  download::SimpleDownloadManager::DownloadVector downloads;
  download_manager->GetAllDownloads(&downloads);

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      downloads.size(), std::move(downloads_restored_callback_));

  for (auto* download : downloads) {
    switch (download->GetState()) {
      case download::DownloadItem::COMPLETE: {
        holding_space_util::ValidityRequirement requirements;
        requirements.must_be_newer_than = kMaxFileAge;
        holding_space_util::FilePathValid(
            profile(), {download->GetFullPath(), requirements},
            base::BindOnce(
                [](const base::FilePath& path,
                   base::RepeatingClosure barrier_closure,
                   ItemDownloadedCallback callback, bool valid) {
                  if (valid)
                    callback.Run(path);
                  barrier_closure.Run();
                },
                download->GetFullPath(), barrier_closure,
                base::BindRepeating(
                    &HoldingSpaceDownloadsDelegate::OnDownloadCompleted,
                    weak_factory_.GetWeakPtr())));
      } break;
      case download::DownloadItem::IN_PROGRESS:
        download_item_observer_.Add(download);
        FALLTHROUGH;
      case download::DownloadItem::CANCELLED:
      case download::DownloadItem::INTERRUPTED:
      case download::DownloadItem::MAX_DOWNLOAD_STATE:
        barrier_closure.Run();
        break;
    }
  }
}

void HoldingSpaceDownloadsDelegate::ManagerGoingDown(
    content::DownloadManager* manager) {
  RemoveObservers();
}

void HoldingSpaceDownloadsDelegate::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  download_item_observer_.Add(item);
}

void HoldingSpaceDownloadsDelegate::OnDownloadUpdated(
    download::DownloadItem* item) {
  switch (item->GetState()) {
    case download::DownloadItem::COMPLETE:
      OnDownloadCompleted(item->GetFullPath());
      FALLTHROUGH;
    case download::DownloadItem::CANCELLED:
    case download::DownloadItem::INTERRUPTED:
      download_item_observer_.Remove(item);
      break;
    case download::DownloadItem::IN_PROGRESS:
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      break;
  }
}

void HoldingSpaceDownloadsDelegate::OnDownloadCompleted(
    const base::FilePath& file_path) {
  if (!is_restoring_persistence())
    item_downloaded_callback_.Run(file_path);
}

void HoldingSpaceDownloadsDelegate::RemoveObservers() {
  download_manager_observer_.RemoveAll();
  download_item_observer_.RemoveAll();
}

}  // namespace ash
