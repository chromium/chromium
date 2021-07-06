// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_downloads_delegate.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/containers/cxx20_erase.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace ash {

namespace {

content::DownloadManager* download_manager_for_testing = nullptr;

// Helpers ---------------------------------------------------------------------

// Returns whether the specified `download_item` is complete.
bool IsComplete(const download::DownloadItem* download_item) {
  return download_item->GetState() ==
         download::DownloadItem::DownloadState::COMPLETE;
}

// Returns whether the specified `download_item` is in progress.
bool IsInProgress(const download::DownloadItem* download_item) {
  return download_item->GetState() ==
         download::DownloadItem::DownloadState::IN_PROGRESS;
}

}  // namespace

// HoldingSpaceDownloadsDelegate::InProgressDownload ---------------------------

// A wrapper around an in-progress `download::DownloadItem` which notifies its
// associated delegate of changes in download state. Each in-progress download
// is associated with a single in-progress holding space item once the target
// file path for the in-progress download has been set.
// NOTE: Instances of this class are immediately destroyed when the underlying
// `download::DownloadItem` is no longer in-progress or when the associated
// in-progress holding space item is removed from the model.
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

  // Cancels the underlying `download_item_`. NOTE: This is expected to be
  // invoked in direct response to an explicit user action and will result in
  // the destruction of `this`.
  void Cancel() { download_item_->Cancel(/*from_user=*/true); }

  // Pauses the underlying `download_item_`.
  void Pause() { download_item_->Pause(); }

  // Resumes the underlying `download_item_`. NOTE: This is expected to be
  // invoked in direct response to an explicit user action.
  void Resume() { download_item_->Resume(/*from_user=*/true); }

  // Marks the underlying `download_item_` to be opened when complete.
  void OpenWhenComplete() { download_item_->SetOpenWhenComplete(true); }

  // Returns the number of bytes received for the underlying `download_item_`.
  int64_t GetReceivedBytes() const {
    return download_item_->GetReceivedBytes();
  }

  // Returns the file path associated with the underlying `download_item_`.
  // NOTE: The file path may be empty before a target file path has been picked.
  const base::FilePath& GetFilePath() const {
    return download_item_->GetFullPath();
  }

  // Returns the target file path associated with the underlying
  // `download_item_`. NOTE: The target file path may be empty before a target
  // file path has been picked.
  const base::FilePath& GetTargetFilePath() const {
    return download_item_->GetTargetFilePath();
  }

  // Returns the current progress of the underlying `download_item_`.
  HoldingSpaceProgress GetProgress() const {
    if (IsComplete(download_item_))
      return HoldingSpaceProgress();
    return HoldingSpaceProgress(GetReceivedBytes(), GetTotalBytes(),
                                /*complete=*/false);
  }

  // Returns the number of total bytes for the underlying `download_item`.
  // NOTE: The total number of bytes will be absent if unknown.
  absl::optional<int64_t> GetTotalBytes() const {
    const int64_t total_bytes = download_item_->GetTotalBytes();
    return total_bytes >= 0 ? absl::make_optional(total_bytes) : absl::nullopt;
  }

  // Returns whether the underlying `download_item_` is paused.
  bool IsPaused() const { return download_item_->IsPaused(); }

  // Associates this in-progress download with the specified in-progress
  // `holding_space_item`. NOTE: This association may be performed only once.
  void SetHoldingSpaceItem(const HoldingSpaceItem* holding_space_item) {
    DCHECK(!holding_space_item_);
    holding_space_item_ = holding_space_item;
  }

  // Returns the in-progress `holding_space_item_` associated with this
  // in-progress download. NOTE: This may be `nullptr` if no holding space item
  // has yet been associated with the in-progress download.
  const HoldingSpaceItem* GetHoldingSpaceItem() const {
    return holding_space_item_;
  }

  // Returns a placeholder resolver which creates a placeholder corresponding
  // to the file type of the associated *target* file path, rather than the
  // *backing* file path, when a thumbnail cannot be generated.
  HoldingSpaceImage::PlaceholderImageSkiaResolver
  GetPlaceholderImageSkiaResolver() const {
    return base::BindRepeating(
        [](const base::WeakPtr<InProgressDownload>& in_progress_download,
           const base::FilePath& file_path, const gfx::Size& size,
           const absl::optional<bool>& dark_background,
           const absl::optional<bool>& is_folder) {
          return HoldingSpaceImage::CreateDefaultPlaceholderImageSkiaResolver()
              .Run(in_progress_download &&
                           IsInProgress(in_progress_download->download_item_)
                       ? in_progress_download->GetTargetFilePath()
                       : file_path,
                   size, dark_background, is_folder);
        },
        weak_factory_.GetWeakPtr());
  }

  // Returns the text to display for the underlying `download_item`.
  absl::optional<std::u16string> GetText() const {
    // Only in-progress download items override primary text. In other cases,
    // the primary text will fall back to the lossy display name of the backing
    // file and be automatically updated in response to file system changes.
    if (!IsInProgress(download_item_))
      return absl::nullopt;
    return download_item_->GetTargetFilePath().BaseName().LossyDisplayName();
  }

  // Returns the secondary text to display for the underlying `download_item`.
  absl::optional<std::u16string> GetSecondaryText() const {
    // Only in-progress download items have secondary text.
    if (!IsInProgress(download_item_))
      return absl::nullopt;

    // In-progress download items which are marked to be opened when complete
    // have a special secondary text treatment.
    if (download_item_->GetOpenWhenComplete()) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_HOLDING_SPACE_IN_PROGRESS_DOWNLOAD_OPEN_WHEN_COMPLETE);
    }

    const int64_t received_bytes = GetReceivedBytes();
    const absl::optional<int64_t> total_bytes = GetTotalBytes();

    std::u16string secondary_text;
    if (total_bytes.has_value()) {
      // If `total_bytes` is known, `secondary_text` will be something of the
      // form "10/100 MB", where the first number is the number of received
      // bytes and the second number is the total number of bytes expected.
      const ui::DataUnits units = ui::GetByteDisplayUnits(total_bytes.value());
      secondary_text = l10n_util::GetStringFUTF16(
          IDS_ASH_HOLDING_SPACE_IN_PROGRESS_DOWNLOAD_SIZE_INFO,
          ui::FormatBytesWithUnits(received_bytes, units, /*show_units=*/false),
          ui::FormatBytesWithUnits(total_bytes.value(), units,
                                   /*show_units=*/true));
    } else {
      // If `total_bytes` is not known, `secondary_text` will be something of
      // the form "10 MB", indicating only the number of received bytes.
      secondary_text = ui::FormatBytes(received_bytes);
    }

    if (download_item_->IsPaused()) {
      // If the `item` is paused, prepend "Paused, " to the `secondary_text`
      // such that the string is of the form "Paused, 10/100 MB" or "Paused, 10
      // MB", depending on whether or not `total_bytes` is known.
      secondary_text = l10n_util::GetStringFUTF16(
          IDS_ASH_HOLDING_SPACE_IN_PROGRESS_DOWNLOAD_PAUSED_WITH_SIZE_INFO,
          secondary_text);
    }

    return secondary_text;
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

  // The in-progress holding space item associated with this in-progress
  // download. NOTE: This may be `nullptr` until the target file path for the
  // in-progress download has been set and a holding space item has been created
  // and associated.
  const HoldingSpaceItem* holding_space_item_ = nullptr;

  base::ScopedObservation<download::DownloadItem,
                          download::DownloadItem::Observer>
      download_item_observation_{this};

  base::WeakPtrFactory<InProgressDownload> weak_factory_{this};
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

// TODO(crbug.com/1184438): Handle Lacros downloads.
void HoldingSpaceDownloadsDelegate::Cancel(const HoldingSpaceItem* item) {
  DCHECK(HoldingSpaceItem::IsDownload(item->type()));
  for (const auto& in_progress_download : in_progress_downloads_) {
    if (in_progress_download->GetHoldingSpaceItem() == item) {
      in_progress_download->Cancel();
      return;
    }
  }
}

// TODO(crbug.com/1184438): Handle Lacros downloads.
void HoldingSpaceDownloadsDelegate::Pause(const HoldingSpaceItem* item) {
  DCHECK(HoldingSpaceItem::IsDownload(item->type()));
  for (const auto& in_progress_download : in_progress_downloads_) {
    if (in_progress_download->GetHoldingSpaceItem() == item) {
      in_progress_download->Pause();
      return;
    }
  }
}

// TODO(crbug.com/1184438): Handle Lacros downloads.
void HoldingSpaceDownloadsDelegate::Resume(const HoldingSpaceItem* item) {
  DCHECK(HoldingSpaceItem::IsDownload(item->type()));
  for (const auto& in_progress_download : in_progress_downloads_) {
    if (in_progress_download->GetHoldingSpaceItem() == item) {
      in_progress_download->Resume();
      return;
    }
  }
}

// TODO(crbug.com/1184438): Handle Lacros downloads.
bool HoldingSpaceDownloadsDelegate::OpenWhenComplete(
    const HoldingSpaceItem* item) {
  DCHECK(HoldingSpaceItem::IsDownload(item->type()));
  for (const auto& in_progress_download : in_progress_downloads_) {
    if (in_progress_download->GetHoldingSpaceItem() == item) {
      in_progress_download->OpenWhenComplete();
      return true;
    }
  }
  return false;
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

void HoldingSpaceDownloadsDelegate::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  // If the user removes a holding space item associated with an in-progress
  // download, that in-progress download can be destroyed. The download will
  // continue, but it will no longer be associated with a holding space item.
  base::EraseIf(in_progress_downloads_, [&](const auto& in_progress_download) {
    return base::Contains(items, in_progress_download->GetHoldingSpaceItem());
  });
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

// TODO(crbug.com/1208910): Support incognito.
void HoldingSpaceDownloadsDelegate::OnLacrosDownloadUpdated(
    const crosapi::mojom::DownloadEvent& event) {
  if (event.is_from_incognito_profile)
    return;
  if (event.state == crosapi::mojom::DownloadState::kComplete) {
    service()->AddDownload(ash::HoldingSpaceItem::Type::kLacrosDownload,
                           event.target_file_path);
  }
}

void HoldingSpaceDownloadsDelegate::OnDownloadUpdated(
    InProgressDownload* in_progress_download) {
  // If in-progress downloads integration is disabled, a holding space item will
  // be associated with a download only upon download completion.
  if (!features::IsHoldingSpaceInProgressDownloadsIntegrationEnabled())
    return;

  // Downloads will not have an associated file path until the target file path
  // is set. In some cases, this requires the user to actively select the target
  // file path from a selection dialog. Only once file path information is set
  // should a holding space item be associated with the in-progress download.
  if (!in_progress_download->GetFilePath().empty())
    CreateOrUpdateHoldingSpaceItem(in_progress_download);
}

void HoldingSpaceDownloadsDelegate::OnDownloadCompleted(
    InProgressDownload* in_progress_download) {
  // If in-progress downloads integration is enabled, a holding space item will
  // have already been associated with the download while it was in-progress.
  CreateOrUpdateHoldingSpaceItem(in_progress_download);
  EraseDownload(in_progress_download);
}

void HoldingSpaceDownloadsDelegate::OnDownloadFailed(
    const InProgressDownload* in_progress_download) {
  // If the `in_progress_download` resulted in the creation of a holding space
  // `item`, that `item` should be removed when the underlying download fails.
  const HoldingSpaceItem* item = in_progress_download->GetHoldingSpaceItem();
  if (item) {
    // NOTE: Removing `item` from the `model()` will result in the
    // `in_progress_download` being erased.
    model()->RemoveItem(item->id());
    DCHECK(!base::Contains(in_progress_downloads_, in_progress_download));
    return;
  }
  EraseDownload(in_progress_download);
}

void HoldingSpaceDownloadsDelegate::EraseDownload(
    const InProgressDownload* in_progress_download) {
  auto it = in_progress_downloads_.find(in_progress_download);
  DCHECK(it != in_progress_downloads_.end());
  in_progress_downloads_.erase(it);
}

void HoldingSpaceDownloadsDelegate::CreateOrUpdateHoldingSpaceItem(
    InProgressDownload* in_progress_download) {
  const HoldingSpaceItem* item = in_progress_download->GetHoldingSpaceItem();

  // Create.
  if (!item) {
    service()->AddDownload(
        HoldingSpaceItem::Type::kDownload, in_progress_download->GetFilePath(),
        in_progress_download->GetProgress(),
        in_progress_download->GetPlaceholderImageSkiaResolver());
    in_progress_download->SetHoldingSpaceItem(
        item = model()->GetItem(HoldingSpaceItem::Type::kDownload,
                                in_progress_download->GetFilePath()));
    // NOTE: This code intentionally falls through so as to update metadata for
    // the newly created holding space item.
  }

  // May be `nullptr` in tests.
  if (!item)
    return;

  // Update.
  model()
      ->UpdateItem(item->id())
      ->SetBackingFile(in_progress_download->GetFilePath(),
                       holding_space_util::ResolveFileSystemUrl(
                           profile(), in_progress_download->GetFilePath()))
      .SetText(in_progress_download->GetText())
      .SetSecondaryText(in_progress_download->GetSecondaryText())
      .SetPaused(in_progress_download->IsPaused())
      .SetProgress(in_progress_download->GetProgress());
}

}  // namespace ash
