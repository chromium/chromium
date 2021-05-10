// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_downloads_delegate.h"

#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_context.h"

namespace ash {

namespace {

content::DownloadManager* download_manager_for_testing = nullptr;

// Helpers ---------------------------------------------------------------------

// Returns whether the specified `download_item` is in progress.
bool IsInProgress(download::DownloadItem* download_item) {
  return download_item->GetState() ==
         download::DownloadItem::DownloadState::IN_PROGRESS;
}

}  // namespace

// HoldingSpaceDownloadsDelegate::InProgressDownload ---------------------------

// A class which observes an in-progress `download::DownloadItem`.
// NOTE: Instances of this class are immediately destroyed when the underlying
// `download::DownloadItem` is no longer in-progress.
class HoldingSpaceDownloadsDelegate::InProgressDownload
    : public download::DownloadItem::Observer {
 public:
  InProgressDownload(HoldingSpaceDownloadsDelegate* delegate,
                     download::DownloadItem* download_item)
      : delegate_(delegate) {
    DCHECK(IsInProgress(download_item));
    download_item_observation_.Observe(download_item);
  }

  InProgressDownload(const InProgressDownload&) = delete;
  InProgressDownload& operator=(const InProgressDownload&) = delete;
  ~InProgressDownload() override = default;

 private:
  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* download_item) override {
    // NOTE: This method invocation may result in destruction.
    delegate_->OnDownloadUpdated(download_item);
  }

  void OnDownloadDestroyed(download::DownloadItem* download_item) override {
    // NOTE: This method invocation will result in destruction.
    delegate_->OnDownloadDestroyed(download_item);
  }

  // NOTE: The `delegate_` owns `this`.
  HoldingSpaceDownloadsDelegate* const delegate_;

  base::ScopedObservation<download::DownloadItem,
                          download::DownloadItem::Observer>
      download_item_observation_{this};
};

// HoldingSpaceDownloadsDelegate -----------------------------------------------

HoldingSpaceDownloadsDelegate::HoldingSpaceDownloadsDelegate(
    HoldingSpaceKeyedService* service,
    HoldingSpaceModel* model)
    : HoldingSpaceKeyedServiceDelegate(service, model) {}

HoldingSpaceDownloadsDelegate::~HoldingSpaceDownloadsDelegate() = default;

// static
void HoldingSpaceDownloadsDelegate::SetDownloadManagerForTesting(
    content::DownloadManager* download_manager) {
  download_manager_for_testing = download_manager;
}

void HoldingSpaceDownloadsDelegate::Init() {
  // ARC downloads.
  if (features::IsHoldingSpaceArcIntegrationEnabled()) {
    // NOTE: The `arc_intent_helper_bridge` may be `nullptr` if the `profile()`
    // is not allowed to use ARC, e.g. if the `profile()` is OTR.
    auto* const arc_intent_helper_bridge =
        arc::ArcIntentHelperBridge::GetForBrowserContext(profile());
    if (arc_intent_helper_bridge)
      arc_intent_helper_observation_.Observe(arc_intent_helper_bridge);
  }

  // Chrome downloads.
  download_manager_observation_.Observe(
      download_manager_for_testing
          ? download_manager_for_testing
          : content::BrowserContext::GetDownloadManager(profile()));
}

void HoldingSpaceDownloadsDelegate::OnPersistenceRestored() {
  content::DownloadManager* download_manager =
      download_manager_for_testing
          ? download_manager_for_testing
          : content::BrowserContext::GetDownloadManager(profile());

  if (download_manager->IsManagerInitialized())
    OnManagerInitialized();
}

void HoldingSpaceDownloadsDelegate::OnArcDownloadAdded(
    const base::FilePath& relative_path,
    const std::string& owner_package_name) {
  DCHECK(features::IsHoldingSpaceArcIntegrationEnabled());
  if (is_restoring_persistence())
    return;

  // It is expected that `owner_package_name` be non-empty. Media files from
  // Chrome are synced to ARC via media scan and have `NULL` owning packages but
  // are expected *not* to have generated `OnArcDownloadAdded()` events.
  if (owner_package_name.empty()) {
    NOTREACHED();
    return;
  }

  // It is expected that `relative_path` always be contained within `Download/`
  // which refers to the public downloads folder for the current `profile()`.
  base::FilePath path(
      file_manager::util::GetDownloadsFolderForProfile(profile()));
  if (!base::FilePath("Download/").AppendRelativePath(relative_path, &path)) {
    NOTREACHED();
    return;
  }

  OnDownloadCompleted(HoldingSpaceItem::Type::kArcDownload, path);
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

  for (download::DownloadItem* download_item : downloads) {
    if (IsInProgress(download_item)) {
      in_progress_downloads_by_id_.emplace(
          std::piecewise_construct,
          /*id=*/std::forward_as_tuple(download_item->GetId()),
          /*in_progress_download=*/std::forward_as_tuple(this, download_item));
    }
  }
}

void HoldingSpaceDownloadsDelegate::ManagerGoingDown(
    content::DownloadManager* manager) {
  download_manager_observation_.Reset();
  in_progress_downloads_by_id_.clear();
}

void HoldingSpaceDownloadsDelegate::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* download_item) {
  // Ignore `OnDownloadCreated()` events prior to `manager` initialization. For
  // those events we create any objects necessary in `OnManagerInitialized()`.
  if (is_restoring_persistence() || !manager->IsManagerInitialized())
    return;

  if (IsInProgress(download_item)) {
    in_progress_downloads_by_id_.emplace(
        std::piecewise_construct,
        /*id=*/std::forward_as_tuple(download_item->GetId()),
        /*in_progress_download=*/std::forward_as_tuple(this, download_item));
  }
}

void HoldingSpaceDownloadsDelegate::OnDownloadUpdated(
    const download::DownloadItem* download_item) {
  switch (download_item->GetState()) {
    case download::DownloadItem::DownloadState::IN_PROGRESS:
      // TODO(crbug.com/1184438): Support in-progress downloads.
      break;
    case download::DownloadItem::DownloadState::COMPLETE:
      OnDownloadCompleted(HoldingSpaceItem::Type::kDownload,
                          download_item->GetFullPath());
      FALLTHROUGH;
    case download::DownloadItem::DownloadState::CANCELLED:
    case download::DownloadItem::DownloadState::INTERRUPTED:
      in_progress_downloads_by_id_.erase(download_item->GetId());
      break;
    case download::DownloadItem::DownloadState::MAX_DOWNLOAD_STATE:
      NOTREACHED();
      break;
  }
}

void HoldingSpaceDownloadsDelegate::OnDownloadDestroyed(
    const download::DownloadItem* download_item) {
  in_progress_downloads_by_id_.erase(download_item->GetId());
}

void HoldingSpaceDownloadsDelegate::OnDownloadCompleted(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  DCHECK(HoldingSpaceItem::IsDownload(type));
  if (!is_restoring_persistence())
    service()->AddDownload(type, file_path, /*progress=*/1.f);
}

}  // namespace ash
