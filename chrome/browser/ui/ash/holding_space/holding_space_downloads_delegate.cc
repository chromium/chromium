// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_downloads_delegate.h"

#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

content::DownloadManager* download_manager_for_testing = nullptr;

// Helpers ---------------------------------------------------------------------

// Returns whether the specified `download_item` is complete.
bool IsComplete(download::DownloadItem* download_item) {
  return download_item->GetState() ==
         download::DownloadItem::DownloadState::COMPLETE;
}

// Returns whether the specified `download_item` is in progress.
bool IsInProgress(download::DownloadItem* download_item) {
  return download_item->GetState() ==
         download::DownloadItem::DownloadState::IN_PROGRESS;
}

}  // namespace

// HoldingSpaceDownloadsDelegate::InProgressDownload ---------------------------

// A wrapper around an in-progress `download::DownloadItem` which notifies its
// associated delegate of changes in download state.
// NOTE: Instances of this class are immediately destroyed when the underlying
// `download::DownloadItem` is no longer in-progress.
class HoldingSpaceDownloadsDelegate::InProgressDownload
    : public download::DownloadItem::Observer {
 public:
  InProgressDownload(HoldingSpaceDownloadsDelegate* delegate,
                     download::DownloadItem* download_item)
      : delegate_(delegate), download_item_(download_item) {
    DCHECK(IsInProgress(download_item));
    download_item_observation_.Observe(download_item);
  }

  InProgressDownload(const InProgressDownload&) = delete;
  InProgressDownload& operator=(const InProgressDownload&) = delete;
  ~InProgressDownload() override = default;

  // Returns the file path associated with the underlying `download_item_`.
  // NOTE: The file path may be empty before a target file path has been picked.
  const base::FilePath& GetFilePath() const {
    return download_item_->GetFullPath();
  }

  // Returns the current progress of the underlying `download_item_`.
  // NOTE: If present, the progress is >= `0.f` and <= `1.f`. If absent, the
  // progress is indeterminate.
  absl::optional<float> GetProgress() const {
    if (IsComplete(download_item_))
      return 1.f;

    absl::optional<float> progress;
    if (download_item_->PercentComplete() >= 0) {
      DCHECK_GE(download_item_->PercentComplete(), 0);
      DCHECK_LE(download_item_->PercentComplete(), 100);
      progress = download_item_->PercentComplete() / 100.f;
    }
    return progress;
  }

 private:
  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* download_item) override {
    switch (download_item->GetState()) {
      case download::DownloadItem::DownloadState::IN_PROGRESS:
        delegate_->OnDownloadUpdated(this);
        break;
      case download::DownloadItem::DownloadState::COMPLETE:
        // NOTE: This method invocation will result in destruction.
        delegate_->OnDownloadCompleted(this);
        break;
      case download::DownloadItem::DownloadState::CANCELLED:
      case download::DownloadItem::DownloadState::INTERRUPTED:
        // NOTE: This method invocation will result in destruction.
        delegate_->OnDownloadFailed(this);
        break;
      case download::DownloadItem::DownloadState::MAX_DOWNLOAD_STATE:
        NOTREACHED();
        break;
    }
  }

  void OnDownloadDestroyed(download::DownloadItem* download_item) override {
    // NOTE: This method invocation will result in destruction.
    delegate_->OnDownloadFailed(this);
  }

  HoldingSpaceDownloadsDelegate* const delegate_;  // NOTE: Owns `this`.
  download::DownloadItem* const download_item_;

  base::ScopedObservation<download::DownloadItem,
                          download::DownloadItem::Observer>
      download_item_observation_{this};
};

// HoldingSpaceDownloadsDelegate -----------------------------------------------

HoldingSpaceDownloadsDelegate::HoldingSpaceDownloadsDelegate(
    HoldingSpaceKeyedService* service,
    HoldingSpaceModel* model)
    : HoldingSpaceKeyedServiceDelegate(service, model) {}

HoldingSpaceDownloadsDelegate::~HoldingSpaceDownloadsDelegate() {
  // Lacros Chrome downloads.
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->download_controller_ash()
        ->RemoveObserver(this);
  }
}

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

  // Ash Chrome downloads.
  download_manager_observation_.Observe(download_manager_for_testing
                                            ? download_manager_for_testing
                                            : profile()->GetDownloadManager());

  // Lacros Chrome downloads.
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->download_controller_ash()
        ->AddObserver(this);
  }
}

void HoldingSpaceDownloadsDelegate::OnPersistenceRestored() {
  content::DownloadManager* download_manager =
      download_manager_for_testing ? download_manager_for_testing
                                   : profile()->GetDownloadManager();

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

  service()->AddDownload(HoldingSpaceItem::Type::kArcDownload, path);
}

void HoldingSpaceDownloadsDelegate::OnManagerInitialized() {
  if (is_restoring_persistence())
    return;

  content::DownloadManager* download_manager =
      download_manager_for_testing ? download_manager_for_testing
                                   : profile()->GetDownloadManager();

  DCHECK(download_manager->IsManagerInitialized());

  download::SimpleDownloadManager::DownloadVector downloads;
  download_manager->GetAllDownloads(&downloads);

  for (download::DownloadItem* download_item : downloads) {
    if (IsInProgress(download_item)) {
      in_progress_downloads_.emplace(
          std::make_unique<InProgressDownload>(this, download_item));
    }
  }
}

void HoldingSpaceDownloadsDelegate::ManagerGoingDown(
    content::DownloadManager* manager) {
  download_manager_observation_.Reset();
  in_progress_downloads_.clear();
}

void HoldingSpaceDownloadsDelegate::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* download_item) {
  // Ignore `OnDownloadCreated()` events prior to `manager` initialization. For
  // those events we create any objects necessary in `OnManagerInitialized()`.
  if (is_restoring_persistence() || !manager->IsManagerInitialized())
    return;

  if (IsInProgress(download_item)) {
    in_progress_downloads_.emplace(
        std::make_unique<InProgressDownload>(this, download_item));
  }
}

// TODO(crbug.com/1184438): Support in-progress downloads.
void HoldingSpaceDownloadsDelegate::OnDownloadUpdated(
    const InProgressDownload* in_progress_download) {}

void HoldingSpaceDownloadsDelegate::OnDownloadCompleted(
    const InProgressDownload* in_progress_download) {
  service()->AddDownload(HoldingSpaceItem::Type::kDownload,
                         in_progress_download->GetFilePath(),
                         in_progress_download->GetProgress());
  EraseDownload(in_progress_download);
}

void HoldingSpaceDownloadsDelegate::OnDownloadFailed(
    const InProgressDownload* in_progress_download) {
  EraseDownload(in_progress_download);
}

void HoldingSpaceDownloadsDelegate::EraseDownload(
    const InProgressDownload* in_progress_download) {
  auto it = in_progress_downloads_.find(in_progress_download);
  DCHECK(it != in_progress_downloads_.end());
  in_progress_downloads_.erase(it);
}

void HoldingSpaceDownloadsDelegate::OnLacrosDownloadUpdated(
    const crosapi::mojom::DownloadEvent& event) {
  // For now, we ignore incognito downloads to match current behavior.
  if (event.is_from_incognito_profile)
    return;
  if (event.state == crosapi::mojom::DownloadState::kComplete) {
    service()->AddDownload(ash::HoldingSpaceItem::Type::kLacrosDownload,
                           event.target_file_path);
  }
}
}  // namespace ash
