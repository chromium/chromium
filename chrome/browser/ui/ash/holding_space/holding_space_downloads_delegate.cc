// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_downloads_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace ash {

namespace {

content::DownloadManager* download_manager_for_testing = nullptr;

}  // namespace

HoldingSpaceDownloadsDelegate::HoldingSpaceDownloadsDelegate(
    Profile* profile,
    HoldingSpaceModel* model,
    ItemDownloadedCallback item_downloaded_callback)
    : HoldingSpaceKeyedServiceDelegate(profile, model),
      item_downloaded_callback_(item_downloaded_callback) {}

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

void HoldingSpaceDownloadsDelegate::OnHoldingSpaceModelRestored() {
  content::DownloadManager* download_manager =
      download_manager_for_testing
          ? download_manager_for_testing
          : content::BrowserContext::GetDownloadManager(profile());

  download::SimpleDownloadManager::DownloadVector downloads;
  download_manager->GetAllDownloads(&downloads);

  for (auto* download : downloads) {
    switch (download->GetState()) {
      case download::DownloadItem::COMPLETE:
        item_downloaded_callback_.Run(download->GetFullPath());
        break;
      case download::DownloadItem::IN_PROGRESS:
        download_item_observer_.Add(download);
        break;
      case download::DownloadItem::CANCELLED:
      case download::DownloadItem::INTERRUPTED:
      case download::DownloadItem::MAX_DOWNLOAD_STATE:
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
      OnDownloadCompleted(item);
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
    download::DownloadItem* item) {
  if (!is_restoring())
    item_downloaded_callback_.Run(item->GetFullPath());
}

void HoldingSpaceDownloadsDelegate::RemoveObservers() {
  download_manager_observer_.RemoveAll();
  download_item_observer_.RemoveAll();
}

}  // namespace ash
