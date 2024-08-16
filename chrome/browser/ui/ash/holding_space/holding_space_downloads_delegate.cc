// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_downloads_delegate.h"

#include <optional>
#include <set>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/public/cpp/image_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_item_utils.h"
#include "components/download/public/common/simple_download_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {
namespace {

using ItemLaunchFailureReason = holding_space_metrics::ItemLaunchFailureReason;

// Helpers ---------------------------------------------------------------------

// Returns a `gfx::ImageSkia` to show as a placeholder in a holding space image
// to indicate error.
gfx::ImageSkia CreateErrorPlaceholderImageSkia(
    const gfx::Size& size,
    cros_styles::ColorName color_name,
    const std::optional<bool>& dark_background) {
  DCHECK_GE(size.width(), kHoldingSpaceIconSize);
  DCHECK_GE(size.height(), kHoldingSpaceIconSize);
  return gfx::ImageSkiaOperations::CreateSuperimposedImage(
      image_util::CreateEmptyImage(size),
      gfx::CreateVectorIcon(
          vector_icons::kErrorOutlineIcon, kHoldingSpaceIconSize,
          cros_styles::ResolveColor(
              color_name,
              /*is_dark_mode=*/
              dark_background.value_or(
                  DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()))));
}

// Returns whether the specified `download_item` is complete.
bool IsComplete(const download::DownloadItem* download_item) {
  return download_item->GetState() == download::DownloadItem::COMPLETE;
}

// Returns whether the specified `download_item` is in progress.
bool IsInProgress(const download::DownloadItem* download_item) {
  return download_item->GetState() == download::DownloadItem::IN_PROGRESS;
}

// Returns whether the specified `download_item` is being scanned.
bool IsScanning(const download::DownloadItem* download_item) {
  return IsInProgress(download_item) &&
         download_item->GetDangerType() ==
             download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING;
}

}  // namespace

// HoldingSpaceDownloadsDelegate::InProgressDownload ---------------------------

// A wrapper around an in-progress `download::DownloadItem` which notifies
// its associated `delegate_` of changes in download state. Each in-progress
// download is associated with a single in-progress holding space item once the
// target file path for the in-progress download has been set. NOTE: Instances
// of this class are immediately destroyed when the underlying download is no
// longer in-progress or when the associated in-progress holding space item is
// removed from the model.
class HoldingSpaceDownloadsDelegate::InProgressDownload
    : public download::DownloadItem::Observer {
 public:
  InProgressDownload(HoldingSpaceDownloadsDelegate* delegate,
                     content::DownloadManager* manager,
                     download::DownloadItem* download_item)
      : delegate_(delegate),
        manager_(manager),
        download_item_(download_item),
        is_dangerous_(IsDangerous()),
        is_insecure_(IsInsecure()),
        might_be_malicious_(MightBeMalicious()) {
    DCHECK(IsInProgress(download_item_));
    download_item_observation_.Observe(download_item_);
  }

  InProgressDownload(const InProgressDownload&) = delete;
  InProgressDownload& operator=(const InProgressDownload&) = delete;
  ~InProgressDownload() override = default;

  // Returns the download manager that originated this download.
  const content::DownloadManager* manager() const { return manager_; }

  // Returns the download item that this in-progress download wraps.
  const download::DownloadItem* download_item() const { return download_item_; }

  // Cancels the underlying download. NOTE: This is expected to be invoked in
  // direct response to an explicit user action and will result in the
  // destruction of `this`.
  void Cancel() { download_item_->Cancel(/*from_user=*/true); }

  // Pauses the underlying download.
  void Pause() { download_item_->Pause(); }

  // Resumes the underlying download. NOTE: This is expected to be invoked in
  // direct response to an explicit user action.
  void Resume() { download_item_->Resume(/*from_user=*/true); }

  // Marks the underlying download to open when complete. Returns `absl:nullopt`
  // on success or the reason if the attempt was not successful.
  std::optional<ItemLaunchFailureReason> OpenWhenComplete() {
    if (GetOpenWhenComplete()) {
      return ItemLaunchFailureReason::kReattemptToOpenWhenComplete;
    }
    download_item_->SetOpenWhenComplete(true);
    return std::nullopt;
  }

  // Returns the accessible name to use for the underlying download.
  // NOTE: If the underlying download is complete, the return value will be
  // absent so as to fallback to default accessibility behavior.
  std::optional<std::u16string> GetAccessibleName() const {
    if (IsComplete(download_item_)) {
      return std::nullopt;
    }

    int msg_id = IDS_ASH_HOLDING_SPACE_IN_PROGRESS_DOWNLOAD_A11Y_NAME;

    if (IsScanning(download_item_)) {
      msg_id = IDS_ASH_HOLDING_SPACE_IN_PROGRESS_DOWNLOAD_A11Y_NAME_SCANNING;
    } else if (IsDangerous() && !MightBeMalicious()) {
      msg_id = IDS_ASH_HOLDING_SPACE_IN_PROGRESS_DOWNLOAD_A11Y_NAME_CONFIRM;
    } else if (IsDangerous() || IsInsecure()) {
      msg_id = IDS_ASH_HOLDING_SPACE_IN_PROGRESS_DOWNLOAD_A11Y_NAME_DANGEROUS;
    } else if (IsPaused()) {
      msg_id = IDS_ASH_HOLDING_SPACE_IN_PROGRESS_DOWNLOAD_A11Y_NAME_PAUSED;
    }

    const auto filename = GetTargetFilePath().BaseName().LossyDisplayName();
    return l10n_util::GetStringFUTF16(msg_id, filename);
  }

  // Returns the file path associated with the underlying download.
  // NOTE: The file path may be empty before a target file path has been picked.
  const base::FilePath& GetFilePath() const {
    return download_item_->GetFullPath();
  }

  // Returns the GUID which uniquely identifies the underlying download.
  // NOTE: The existence of `this` implies that GUID is present.
  const std::string& GetGuid() const { return download_item_->GetGuid(); }

  // Returns whether the underlying download is marked to open when complete.
  bool GetOpenWhenComplete() const {
    return download_item_->GetOpenWhenComplete();
  }

  // Returns the current progress of the underlying download.
  // NOTE:
  //   * Progress is indeterminate if the download is being scanned.
  //   * Progress is hidden if the download is dangerous or insecure.
  HoldingSpaceProgress GetProgress() const {
    if (IsComplete(download_item_)) {
      return HoldingSpaceProgress();
    }
    if (IsScanning(download_item_)) {
      return HoldingSpaceProgress(/*current_bytes=*/std::nullopt,
                                  /*total_bytes=*/std::nullopt);
    }
    return HoldingSpaceProgress(GetReceivedBytes(), GetTotalBytes(),
                                /*complete=*/false,
                                /*hidden=*/IsDangerous() || IsInsecure());
  }

  // Returns the target file path associated with the underlying download.
  // NOTE: Returned path may be empty before a target file path has been picked.
  const base::FilePath& GetTargetFilePath() const {
    return download_item_->GetTargetFilePath();
  }

  // Returns the number of bytes received for the underlying download.
  int64_t GetReceivedBytes() const {
    return download_item_->GetReceivedBytes();
  }

  // Returns the number of total bytes for the underlying download.
  // NOTE: The total number of bytes will be absent if unknown or indeterminate.
  std::optional<int64_t> GetTotalBytes() const {
    const int64_t total_bytes = download_item_->GetTotalBytes();
    return total_bytes > 0 ? std::make_optional(total_bytes) : std::nullopt;
  }

  // Returns whether the underlying download is dangerous.
  bool IsDangerous() const { return download_item_->IsDangerous(); }

  // Returns whether the underlying download is insecure.
  bool IsInsecure() const { return download_item_->IsInsecure(); }

  // Returns whether the underlying download is paused.
  bool IsPaused() const { return download_item_->IsPaused(); }

  // Returns whether the underlying download might be malicious.
  bool MightBeMalicious() const {
    return IsDangerous() && download_item_->GetDangerType() !=
                                download::DownloadDangerType::
                                    DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE;
  }

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

  // Returns a resolver which creates a `gfx::ImageSkia` placeholder
  // corresponding to the file type of the associated *target* file path, rather
  // than the *backing* file path, when a thumbnail cannot be generated. Note
  // that if the download is dangerous or is insecure, a placeholder
  // indicating error will be returned.
  HoldingSpaceImage::PlaceholderImageSkiaResolver
  GetPlaceholderImageSkiaResolver() const {
    return base::BindRepeating(
        [](const base::WeakPtr<InProgressDownload>& in_progress_download,
           const base::FilePath& file_path, const gfx::Size& size,
           const std::optional<bool>& dark_background,
           const std::optional<bool>& is_folder) {
          if (in_progress_download && (in_progress_download->IsDangerous() ||
                                       in_progress_download->IsInsecure())) {
            return CreateErrorPlaceholderImageSkia(
                size, /*color_name=*/in_progress_download->IsDangerous() &&
                              !in_progress_download->MightBeMalicious()
                          ? cros_styles::ColorName::kIconColorWarning
                          : cros_styles::ColorName::kIconColorAlert,
                dark_background);
          }

          base::FilePath rewritten_file_path(file_path);
          if (in_progress_download &&
              IsInProgress(in_progress_download->download_item())) {
            rewritten_file_path = in_progress_download->GetTargetFilePath();
          }

          return HoldingSpaceImage::CreateDefaultPlaceholderImageSkiaResolver()
              .Run(rewritten_file_path, size, dark_background, is_folder);
        },
        weak_factory_.GetMutableWeakPtr());
  }

  // Returns the text to display for the underlying download.
  std::optional<std::u16string> GetText() const {
    // Only in-progress download items override primary text. In other cases,
    // the primary text will fall back to the lossy display name of the backing
    // file and be automatically updated in response to file system changes.
    if (!IsInProgress(download_item_)) {
      return std::nullopt;
    }
    return GetTargetFilePath().BaseName().LossyDisplayName();
  }

  // Returns the secondary text to display for the underlying download.
  std::optional<std::u16string> GetSecondaryText() const {
    // Only in-progress download items have secondary text.
    if (!IsInProgress(download_item_)) {
      return std::nullopt;
    }

    // In-progress download items which are being scanned have a special
    // secondary text treatment.
    if (IsScanning(download_item_)) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_HOLDING_SPACE_IN_PROGRESS_DOWNLOAD_SCANNING);
    }

    // In-progress download items which are dangerous but not malicious can be
    // kept or discarded by the user via notification. This being the case, such
    // items have a special secondary text treatment.
    if (IsDangerous() && !MightBeMalicious()) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_HOLDING_SPACE_IN_PROGRESS_DOWNLOAD_CONFIRM);
    }

    // In-progress download items which are dangerous or insecure have a special
    // secondary text treatment.
    if (IsDangerous() || IsInsecure()) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_HOLDING_SPACE_IN_PROGRESS_DOWNLOAD_DANGEROUS_FILE);
    }

    // In-progress download items which are marked to be opened when complete
    // and are not paused have a special secondary text treatment.
    if (GetOpenWhenComplete() && !IsPaused()) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_HOLDING_SPACE_IN_PROGRESS_DOWNLOAD_OPEN_WHEN_COMPLETE);
    }

    const int64_t received_bytes = GetReceivedBytes();
    const std::optional<int64_t> total_bytes = GetTotalBytes();

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

    if (IsPaused()) {
      // If the `item` is paused, prepend "Paused, " to the `secondary_text`
      // such that the string is of the form "Paused, 10/100 MB" or "Paused, 10
      // MB", depending on whether or not `total_bytes` is known.
      secondary_text = l10n_util::GetStringFUTF16(
          IDS_ASH_HOLDING_SPACE_IN_PROGRESS_DOWNLOAD_PAUSED_WITH_SIZE_INFO,
          secondary_text);
    }

    return secondary_text;
  }

  // Returns the color for the secondary text to display for the underlying
  // download.
  std::optional<ui::ColorId> GetSecondaryTextColorId() const {
    // Only in-progress download items have secondary text.
    if (!IsInProgress(download_item_)) {
      return std::nullopt;
    }

    // In-progress download items which are being scanned have a special
    // secondary text treatment.
    if (IsScanning(download_item_)) {
      return cros_tokens::kTextColorProminent;
    }

    // In-progress download items which are dangerous but not malicious can be
    // kept or discarded by the user via notification. This being the case, such
    // items have a special secondary text treatment.
    if (IsDangerous() && !MightBeMalicious())
      return cros_tokens::kTextColorWarning;

    // In-progress download items which are dangerous or insecure have a special
    // secondary text treatment.
    if (IsDangerous() || IsInsecure())
      return cros_tokens::kTextColorAlert;

    return std::nullopt;
  }

 protected:
  // Updates cached `download_item_` properties and notifies `delegate_` of the
  // change in state. Note that invoking this method may result in the
  // destruction of `this`.
  void UpdateDownloadItem(bool destroyed) {
    const bool was_dangerous_but_not_malicious =
        is_dangerous_ && !might_be_malicious_;
    const bool was_dangerous_or_insecure = is_dangerous_ || is_insecure_;

    // Update cached properties.
    is_dangerous_ = IsDangerous();
    is_insecure_ = IsInsecure();
    might_be_malicious_ = MightBeMalicious();

    if (destroyed) {
      delegate_->OnDownloadFailed(this);  // NOTE: Destroys `this`.
      return;
    }

    const bool is_dangerous_but_not_malicious =
        is_dangerous_ && !might_be_malicious_;
    const bool is_dangerous_or_insecure = is_dangerous_ || is_insecure_;

    // Explicitly invalidate the image of the associated holding space item if
    // the download is transitioning to/from a state which required an error
    // placeholder image.
    const bool invalidate_image =
        was_dangerous_but_not_malicious != is_dangerous_but_not_malicious ||
        was_dangerous_or_insecure != is_dangerous_or_insecure;

    switch (download_item_->GetState()) {
      case download::DownloadItem::IN_PROGRESS:
        delegate_->OnDownloadUpdated(this, invalidate_image);
        break;
      case download::DownloadItem::COMPLETE:
        delegate_->OnDownloadCompleted(this);  // NOTE: Destroys `this`.
        break;
      case download::DownloadItem::CANCELLED:
      case download::DownloadItem::INTERRUPTED:
        delegate_->OnDownloadFailed(this);  // NOTE: Destroys `this`.
        break;
      case download::DownloadItem::MAX_DOWNLOAD_STATE:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

 private:
  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* download_item) override {
    // NOTE: This method invocation may result in destruction of `this`,
    // depending on the state of the underlying download.
    UpdateDownloadItem(/*destroyed=*/false);
  }

  void OnDownloadDestroyed(download::DownloadItem* download_item) override {
    UpdateDownloadItem(/*destroyed=*/true);  // NOTE: Destroys `this`.
  }

  const raw_ptr<HoldingSpaceDownloadsDelegate> delegate_;  // NOTE: Owns `this`.
  const raw_ptr<content::DownloadManager> manager_;
  const raw_ptr<download::DownloadItem> download_item_;

  // The in-progress holding space item associated with this in-progress
  // download. NOTE: This may be `nullptr` until the target file path for the
  // in-progress download has been set and a holding space item has been created
  // and associated.
  raw_ptr<const HoldingSpaceItem> holding_space_item_ = nullptr;

  // Cached `download_item_` properties used to determine when to invalidate the
  // associated `holding_space_item_` image. The image is only invalidated when
  // these properties change.
  bool is_dangerous_ = false;
  bool is_insecure_ = false;
  bool might_be_malicious_ = false;

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

HoldingSpaceDownloadsDelegate::~HoldingSpaceDownloadsDelegate() = default;

std::optional<holding_space_metrics::ItemLaunchFailureReason>
HoldingSpaceDownloadsDelegate::OpenWhenComplete(const HoldingSpaceItem* item) {
  DCHECK(HoldingSpaceItem::IsDownloadType(item->type()));
  for (const auto& in_progress_download : in_progress_downloads_) {
    if (in_progress_download->GetHoldingSpaceItem() == item)
      return in_progress_download->OpenWhenComplete();
  }
  return ItemLaunchFailureReason::kDownloadNotFound;
}

void HoldingSpaceDownloadsDelegate::OnPersistenceRestored() {
  // ARC downloads.
  // NOTE: The `arc_file_system_bridge` may be `nullptr` if the `profile()`
  // is not allowed to use ARC, e.g. if the `profile()` is OTR.
  auto* const arc_file_system_bridge =
      arc::ArcFileSystemBridge::GetForBrowserContext(profile());
  if (arc_file_system_bridge)
    arc_file_system_bridge_observation_.Observe(arc_file_system_bridge);

  // Ash Chrome downloads.
  download_notifier_.AddProfile(profile());
}

void HoldingSpaceDownloadsDelegate::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  // If the user removes a holding space item associated with an in-progress
  // download, that in-progress download can be destroyed. The download will
  // continue, but it will no longer be associated with a holding space item.
  std::erase_if(in_progress_downloads_, [&](const auto& in_progress_download) {
    return base::Contains(items, in_progress_download->GetHoldingSpaceItem());
  });
}

void HoldingSpaceDownloadsDelegate::OnManagerInitialized(
    content::DownloadManager* manager) {
  DCHECK(!is_restoring_persistence());
  download::SimpleDownloadManager::DownloadVector downloads;
  manager->GetAllDownloads(&downloads);
  for (download::DownloadItem* download : downloads) {
    OnDownloadCreated(manager, download);
  }
}

void HoldingSpaceDownloadsDelegate::OnManagerGoingDown(
    content::DownloadManager* manager) {
  // Collect all downloads associated with `manager`. These downloads will be
  // removed from `in_progress_downloads_` on failure, so they cannot be failed
  // in a loop that iterates over `in_progress_downloads_`.
  std::set<InProgressDownload*> downloads_to_remove;
  for (const auto& in_progress_download : in_progress_downloads_) {
    if (in_progress_download->manager() == manager) {
      downloads_to_remove.insert(in_progress_download.get());
    }
  }

  // Fail all of `manager`'s downloads.
  for (InProgressDownload* in_progress_download : downloads_to_remove)
    OnDownloadFailed(in_progress_download);
}

void HoldingSpaceDownloadsDelegate::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* download_item) {
  DCHECK(!is_restoring_persistence());
  if (IsInProgress(download_item)) {
    in_progress_downloads_.emplace(
        std::make_unique<InProgressDownload>(this, manager, download_item));
  }
}

void HoldingSpaceDownloadsDelegate::OnDownloadUpdated(
    content::DownloadManager* manager,
    download::DownloadItem* download_item) {
  if (!IsInProgress(download_item))
    return;

  // In the common case, we already created an `InProgressDownload` for
  // `download_item` in `OnDownloadCreated()`.
  for (const auto& in_progress_download : in_progress_downloads_) {
    if (in_progress_download->download_item() == download_item) {
      return;
    }
  }

  // If `download_item` does not have an existing `InProgressDownload` (e.g.
  // because it was interrupted and then resumed), create one.
  in_progress_downloads_.emplace(
      std::make_unique<InProgressDownload>(this, manager, download_item));
}

void HoldingSpaceDownloadsDelegate::OnMediaStoreUriAdded(
    const GURL& uri,
    const arc::mojom::MediaStoreMetadata& metadata) {
  if (is_restoring_persistence())
    return;

  // Holding space is only interested in download added events.
  if (!metadata.is_download())
    return;

  const auto& download = metadata.get_download();
  const base::FilePath& relative_path = download->relative_path;
  const std::string& display_name = download->display_name;

  // It is expected that `relative_path` always be contained within `Download/`
  // which refers to the public downloads folder for the associated `profile()`.
  base::FilePath path(
      file_manager::util::GetDownloadsFolderForProfile(profile()));
  if (!base::FilePath("Download/")
           .AppendRelativePath(relative_path.Append(display_name), &path)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  service()->AddItemOfType(HoldingSpaceItem::Type::kArcDownload, path);
}

void HoldingSpaceDownloadsDelegate::OnDownloadUpdated(
    InProgressDownload* in_progress_download,
    bool invalidate_image) {
  // Downloads will not have an associated file path until the target file path
  // is set. In some cases, this requires the user to actively select the target
  // file path from a selection dialog. Only once file path information is set
  // should a holding space item be associated with the in-progress download.
  if (!in_progress_download->GetFilePath().empty())
    CreateOrUpdateHoldingSpaceItem(in_progress_download, invalidate_image);
}

void HoldingSpaceDownloadsDelegate::OnDownloadCompleted(
    InProgressDownload* in_progress_download) {
  // If in-progress downloads integration is enabled, a holding space item will
  // have already been associated with the download while it was in-progress.
  CreateOrUpdateHoldingSpaceItem(in_progress_download, true);
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
    InProgressDownload* in_progress_download,
    bool invalidate_image) {
  const HoldingSpaceItem* item = in_progress_download->GetHoldingSpaceItem();

  // Create.
  if (!item) {
    const std::string& id = service()->AddItemOfType(
        HoldingSpaceItem::Type::kDownload, in_progress_download->GetFilePath(),
        in_progress_download->GetProgress(),
        in_progress_download->GetPlaceholderImageSkiaResolver());
    in_progress_download->SetHoldingSpaceItem(item = model()->GetItem(id));
    // NOTE: This code intentionally falls through so as to update metadata for
    // the newly created holding space item.
  }

  // May be `nullptr` in tests.
  if (!item)
    return;

  // Commands.
  std::vector<HoldingSpaceItem::InProgressCommand> in_progress_commands;
  if (!in_progress_download->GetProgress().IsComplete()) {
    if (!(in_progress_download->IsDangerous() ||
          in_progress_download->IsInsecure())) {
      in_progress_commands.push_back(
          in_progress_download->IsPaused()
              ? HoldingSpaceItem::InProgressCommand(
                    HoldingSpaceCommandId::kResumeItem,
                    IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_RESUME, &kResumeIcon,
                    base::BindRepeating(&HoldingSpaceDownloadsDelegate::Resume,
                                        weak_factory_.GetWeakPtr()))
              : HoldingSpaceItem::InProgressCommand(
                    HoldingSpaceCommandId::kPauseItem,
                    IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_PAUSE, &kPauseIcon,
                    base::BindRepeating(&HoldingSpaceDownloadsDelegate::Pause,
                                        weak_factory_.GetWeakPtr())));
    }
    in_progress_commands.emplace_back(
        HoldingSpaceCommandId::kCancelItem,
        IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_CANCEL, &kCancelIcon,
        base::BindRepeating(&HoldingSpaceDownloadsDelegate::Cancel,
                            weak_factory_.GetWeakPtr()));
  }

  // File.
  const base::FilePath file_path = in_progress_download->GetFilePath();
  const GURL file_system_url =
      holding_space_util::ResolveFileSystemUrl(profile(), file_path);
  const HoldingSpaceFile::FileSystemType file_system_type =
      holding_space_util::ResolveFileSystemType(profile(), file_system_url);

  // Update.
  service()
      ->UpdateItem(item->id())
      ->SetAccessibleName(in_progress_download->GetAccessibleName())
      .SetBackingFile(
          HoldingSpaceFile(file_path, file_system_type, file_system_url))
      .SetInProgressCommands(std::move(in_progress_commands))
      .SetInvalidateImage(invalidate_image)
      .SetText(in_progress_download->GetText())
      .SetSecondaryText(in_progress_download->GetSecondaryText())
      .SetSecondaryTextColorVariant(
          in_progress_download->GetSecondaryTextColorId())
      .SetProgress(in_progress_download->GetProgress());
}

void HoldingSpaceDownloadsDelegate::Cancel(
    const HoldingSpaceItem* item,
    HoldingSpaceCommandId command_id,
    holding_space_metrics::EventSource event_source) {
  DCHECK(HoldingSpaceItem::IsDownloadType(item->type()));
  DCHECK_EQ(HoldingSpaceCommandId::kCancelItem, command_id);
  for (const auto& in_progress_download : in_progress_downloads_) {
    if (in_progress_download->GetHoldingSpaceItem() == item) {
      holding_space_metrics::RecordItemAction(
          {item}, holding_space_metrics::ItemAction::kCancel, event_source);
      in_progress_download->Cancel();
      return;
    }
  }
}

void HoldingSpaceDownloadsDelegate::Pause(
    const HoldingSpaceItem* item,
    HoldingSpaceCommandId command_id,
    holding_space_metrics::EventSource event_source) {
  DCHECK(HoldingSpaceItem::IsDownloadType(item->type()));
  DCHECK_EQ(HoldingSpaceCommandId::kPauseItem, command_id);
  for (const auto& in_progress_download : in_progress_downloads_) {
    if (in_progress_download->GetHoldingSpaceItem() == item) {
      holding_space_metrics::RecordItemAction(
          {item}, holding_space_metrics::ItemAction::kPause, event_source);
      in_progress_download->Pause();
      return;
    }
  }
}

void HoldingSpaceDownloadsDelegate::Resume(
    const HoldingSpaceItem* item,
    HoldingSpaceCommandId command_id,
    holding_space_metrics::EventSource event_source) {
  DCHECK(HoldingSpaceItem::IsDownloadType(item->type()));
  DCHECK_EQ(HoldingSpaceCommandId::kResumeItem, command_id);
  for (const auto& in_progress_download : in_progress_downloads_) {
    if (in_progress_download->GetHoldingSpaceItem() == item) {
      holding_space_metrics::RecordItemAction(
          {item}, holding_space_metrics::ItemAction::kResume, event_source);
      in_progress_download->Resume();
      return;
    }
  }
}

}  // namespace ash
