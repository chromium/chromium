// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"

#include <set>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "base/callback_helpers.h"
#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_downloads_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_file_system_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_metrics_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_persistence_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_suggestions_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "components/account_id/account_id.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// TODO(crbug.com/1131266): Track alternative type in `HoldingSpaceItem`.
// Returns a holding space item other than the one provided which is backed by
// the same file path in the specified `model`.
absl::optional<const HoldingSpaceItem*> GetAlternativeHoldingSpaceItem(
    const HoldingSpaceModel& model,
    const HoldingSpaceItem* item) {
  for (const auto& candidate_item : model.items()) {
    if (candidate_item.get() == item)
      continue;
    if (candidate_item->file_path() == item->file_path())
      return candidate_item.get();
  }
  return absl::nullopt;
}

// Returns the singleton profile manager for the browser process.
ProfileManager* GetProfileManager() {
  return g_browser_process->profile_manager();
}

// Records the time from the first availability of the holding space feature
// to the time of the first item being added into holding space.
void RecordTimeFromFirstAvailabilityToFirstAdd(Profile* profile) {
  base::Time time_of_first_availability =
      holding_space_prefs::GetTimeOfFirstAvailability(profile->GetPrefs())
          .value();
  base::Time time_of_first_add =
      holding_space_prefs::GetTimeOfFirstAdd(profile->GetPrefs()).value();
  holding_space_metrics::RecordTimeFromFirstAvailabilityToFirstAdd(
      time_of_first_add - time_of_first_availability);
}

// Records the time from the first entry to the first pin into holding space.
// Note that this time may be zero if the user pinned their first file before
// having ever entered holding space.
void RecordTimeFromFirstEntryToFirstPin(Profile* profile) {
  base::Time time_of_first_pin =
      holding_space_prefs::GetTimeOfFirstPin(profile->GetPrefs()).value();
  base::Time time_of_first_entry =
      holding_space_prefs::GetTimeOfFirstEntry(profile->GetPrefs())
          .value_or(time_of_first_pin);
  holding_space_metrics::RecordTimeFromFirstEntryToFirstPin(
      time_of_first_pin - time_of_first_entry);
}

}  // namespace

// HoldingSpaceKeyedService ----------------------------------------------------

HoldingSpaceKeyedService::HoldingSpaceKeyedService(Profile* profile,
                                                   const AccountId& account_id)
    : profile_(profile),
      account_id_(account_id),
      holding_space_client_(profile),
      thumbnail_loader_(profile) {
  // Mark when the holding space feature first became available. If this is not
  // the first time that holding space became available, this will no-op.
  holding_space_prefs::MarkTimeOfFirstAvailability(profile_->GetPrefs());

  ProfileManager* const profile_manager = GetProfileManager();
  if (!profile_manager)  // May be `nullptr` in tests.
    return;

  // The associated profile may not be ready yet. If it is, we can immediately
  // proceed with profile dependent initialization.
  if (profile_manager->IsValidProfile(profile)) {
    OnProfileReady();
    return;
  }

  // Otherwise we need to wait for the profile to be added.
  profile_manager_observer_.Observe(profile_manager);
}

HoldingSpaceKeyedService::~HoldingSpaceKeyedService() {
  if (chromeos::PowerManagerClient::Get())
    chromeos::PowerManagerClient::Get()->RemoveObserver(this);

  if (HoldingSpaceController::Get()) {  // May be `nullptr` in tests.
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(
        account_id_, /*client=*/nullptr, /*model=*/nullptr);
  }
}

// static
void HoldingSpaceKeyedService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // TODO(crbug.com/1131266): Move to `ash::holding_space_prefs`.
  HoldingSpacePersistenceDelegate::RegisterProfilePrefs(registry);
}

void HoldingSpaceKeyedService::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::HoldingSpaceService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void HoldingSpaceKeyedService::AddPrintedPdf(
    const base::FilePath& printed_pdf_path,
    bool from_incognito_profile) {
  AddItemOfType(HoldingSpaceItem::Type::kPrintedPdf, printed_pdf_path);
}

void HoldingSpaceKeyedService::AddPinnedFiles(
    const std::vector<storage::FileSystemURL>& file_system_urls) {
  std::vector<std::unique_ptr<HoldingSpaceItem>> items;
  std::vector<const HoldingSpaceItem*> items_to_record;
  for (const storage::FileSystemURL& file_system_url : file_system_urls) {
    if (ContainsPinnedFile(file_system_url))
      continue;

    items.push_back(HoldingSpaceItem::CreateFileBackedItem(
        HoldingSpaceItem::Type::kPinnedFile, file_system_url.path(),
        file_system_url.ToGURL(),
        base::BindOnce(&holding_space_util::ResolveImage, &thumbnail_loader_)));

    // When pinning an item which already exists in holding space, the pin
    // action should be recorded on the alternative item backed by the same file
    // path if such an item exists. Otherwise the only type of holding space
    // item pinned will be thought to be `kPinnedFile`.
    items_to_record.push_back(
        GetAlternativeHoldingSpaceItem(holding_space_model_, items.back().get())
            .value_or(items.back().get()));

    if (file_system_url.type() == storage::kFileSystemTypeDriveFs)
      MakeDriveItemAvailableOffline(file_system_url);
  }

  DCHECK_EQ(items.size(), items_to_record.size());
  if (items.empty())
    return;

  // Mark when the first pin to holding space occurred. If this is not the first
  // pin to holding space, this will no-op. If this is the first pin, record the
  // amount of time from first entry to first pin into holding space.
  if (holding_space_prefs::MarkTimeOfFirstPin(profile_->GetPrefs()))
    RecordTimeFromFirstEntryToFirstPin(profile_);

  holding_space_metrics::RecordItemAction(
      items_to_record, holding_space_metrics::ItemAction::kPin);

  AddItems(std::move(items), /*allow_duplicates=*/false);
}

void HoldingSpaceKeyedService::RemovePinnedFiles(
    const std::vector<storage::FileSystemURL>& file_system_urls) {
  std::set<std::string> items;
  std::vector<const HoldingSpaceItem*> items_to_record;
  for (const storage::FileSystemURL& file_system_url : file_system_urls) {
    const HoldingSpaceItem* item = holding_space_model_.GetItem(
        HoldingSpaceItem::Type::kPinnedFile, file_system_url.path());
    if (!item)
      continue;

    items.emplace(item->id());

    // When removing a pinned item, the unpin action should be recorded on the
    // alternative item backed by the same file path if such an item exists.
    // This will give more insight as to what types of items are being unpinned
    // than would otherwise be known if only `kPinnedFile` was recorded.
    items_to_record.push_back(
        GetAlternativeHoldingSpaceItem(holding_space_model_, item)
            .value_or(item));
  }

  DCHECK_EQ(items.size(), items_to_record.size());
  if (items.empty())
    return;

  holding_space_metrics::RecordItemAction(
      items_to_record, holding_space_metrics::ItemAction::kUnpin);

  holding_space_model_.RemoveItems(items);
}

bool HoldingSpaceKeyedService::ContainsPinnedFile(
    const storage::FileSystemURL& file_system_url) const {
  return holding_space_model_.ContainsItem(HoldingSpaceItem::Type::kPinnedFile,
                                           file_system_url.path());
}

std::vector<GURL> HoldingSpaceKeyedService::GetPinnedFiles() const {
  std::vector<GURL> pinned_files;
  for (const auto& item : holding_space_model_.items()) {
    if (item->type() == HoldingSpaceItem::Type::kPinnedFile)
      pinned_files.push_back(item->file_system_url());
  }
  return pinned_files;
}

void HoldingSpaceKeyedService::AddDiagnosticsLog(
    const base::FilePath& diagnostics_log_path) {
  AddItemOfType(HoldingSpaceItem::Type::kDiagnosticsLog, diagnostics_log_path);
}

const std::string& HoldingSpaceKeyedService::AddDownload(
    HoldingSpaceItem::Type type,
    const base::FilePath& download_file,
    const HoldingSpaceProgress& progress,
    HoldingSpaceImage::PlaceholderImageSkiaResolver
        placeholder_image_skia_resolver) {
  DCHECK(HoldingSpaceItem::IsDownload(type));
  return AddItemOfType(type, download_file, progress,
                       placeholder_image_skia_resolver);
}

void HoldingSpaceKeyedService::AddNearbyShare(
    const base::FilePath& nearby_share_path) {
  AddItemOfType(HoldingSpaceItem::Type::kNearbyShare, nearby_share_path);
}

const std::string& HoldingSpaceKeyedService::AddPhoneHubCameraRollItem(
    const base::FilePath& item_path,
    const HoldingSpaceProgress& progress) {
  return AddItemOfType(HoldingSpaceItem::Type::kPhoneHubCameraRoll, item_path,
                       progress);
}

void HoldingSpaceKeyedService::AddScan(const base::FilePath& file_path) {
  AddItemOfType(HoldingSpaceItem::Type::kScan, file_path);
}

void HoldingSpaceKeyedService::AddScreenRecording(
    const base::FilePath& screen_recording_file) {
  AddItemOfType(HoldingSpaceItem::Type::kScreenRecording,
                screen_recording_file);
}

void HoldingSpaceKeyedService::AddScreenshot(
    const base::FilePath& screenshot_file) {
  AddItemOfType(HoldingSpaceItem::Type::kScreenshot, screenshot_file);
}

void HoldingSpaceKeyedService::SetSuggestions(
    const std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>&
        suggestions) {
  // Construct `items` from `suggestions` in the reverse order so that
  // suggestion views follow the order of `suggestions`. `suggestions` could
  // have duplicates in the holding space model. In this case, the existing
  // suggestions are still replaced by newly generated ones because the
  // suggestion order could change.
  std::vector<std::unique_ptr<HoldingSpaceItem>> items_to_add;
  for (const auto& [type, file_path] : base::Reversed(suggestions)) {
    std::unique_ptr<HoldingSpaceItem> item;
    if (const HoldingSpaceItem* existing_item =
            holding_space_model_.GetItem(type, file_path);
        existing_item && !existing_item->IsInitialized()) {
      // Reuse the existing uninitialized file suggestion item to avoid
      // resolving the suggested file's URL. Because `existing_item` is
      // uninitialized, its removal does not incur visual changes.
      item = holding_space_model_.TakeItem(existing_item->id());
    } else {
      item = CreateItemOfType(
          type, file_path,
          /*progress=*/HoldingSpaceProgress(),
          /*placeholder_image_skia_resolver=*/base::NullCallback());
    }

    if (item)
      items_to_add.push_back(std::move(item));
  }

  std::set<std::string> item_ids_to_remove;
  for (const auto& item : holding_space_model_.items()) {
    if (HoldingSpaceItem::IsSuggestion(item->type()))
      item_ids_to_remove.insert(item->id());
  }

  // Allow the duplicate suggestions to be added because the order among
  // `suggestions` should be respected.
  AddItems(std::move(items_to_add), /*allow_duplicates=*/true);

  // Remove old suggestions after adding new suggestions. Otherwise,
  // `holding_space_model_` could be empty after removing old suggestions and
  // before adding new suggestions, which could close the holding space view.
  holding_space_model_.RemoveItems(item_ids_to_remove);
}

const std::string& HoldingSpaceKeyedService::AddItem(
    std::unique_ptr<HoldingSpaceItem> item) {
  std::vector<std::unique_ptr<HoldingSpaceItem>> items;
  items.push_back(std::move(item));
  return AddItems(std::move(items), /*allow_duplicates=*/false).at(0);
}

std::vector<std::reference_wrapper<const std::string>>
HoldingSpaceKeyedService::AddItems(
    std::vector<std::unique_ptr<HoldingSpaceItem>> items,
    bool allow_duplicates) {
  std::vector<std::reference_wrapper<const std::string>> result;
  std::vector<std::unique_ptr<HoldingSpaceItem>> items_to_add;

  for (auto& item : items) {
    if (!allow_duplicates &&
        holding_space_model_.ContainsItem(item->type(), item->file_path())) {
      // Ignore any `items` that already exist in the `holding_space_model_`
      // if `allow_duplicates` is false.
      result.push_back(std::cref(base::EmptyString()));
      continue;
    }
    result.push_back(std::cref(item->id()));
    items_to_add.push_back(std::move(item));
  }

  if (!items_to_add.empty()) {
    // Mark the time when the user's first item was added to holding space. Note
    // that true is returned iff this is in fact the user's first add and, if
    // so, the time it took for the user to add their first item should be
    // recorded.
    if (holding_space_prefs::MarkTimeOfFirstAdd(profile_->GetPrefs()))
      RecordTimeFromFirstAvailabilityToFirstAdd(profile_);
    holding_space_model_.AddItems(std::move(items_to_add));
  }

  return result;
}

const std::string& HoldingSpaceKeyedService::AddItemOfType(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path,
    const HoldingSpaceProgress& progress,
    HoldingSpaceImage::PlaceholderImageSkiaResolver
        placeholder_image_skia_resolver) {
  std::unique_ptr<HoldingSpaceItem> item = CreateItemOfType(
      type, file_path, progress, placeholder_image_skia_resolver);
  if (!item)
    return base::EmptyString();

  return AddItem(std::move(item));
}

std::unique_ptr<HoldingSpaceModel::ScopedItemUpdate>
HoldingSpaceKeyedService::UpdateItem(const std::string& id) {
  return holding_space_model_.UpdateItem(id);
}

void HoldingSpaceKeyedService::RemoveAll() {
  holding_space_model_.RemoveAll();
}

void HoldingSpaceKeyedService::RemoveItem(const std::string& id) {
  holding_space_model_.RemoveItem(id);
}

absl::optional<holding_space_metrics::ItemFailureToLaunchReason>
HoldingSpaceKeyedService::OpenItemWhenComplete(const HoldingSpaceItem* item) {
  // Currently it is only possible to open download type items when complete.
  if (HoldingSpaceItem::IsDownload(item->type()) && downloads_delegate_)
    return downloads_delegate_->OpenWhenComplete(item);
  return holding_space_metrics::ItemFailureToLaunchReason::
      kNoHandlerForItemType;
}

void HoldingSpaceKeyedService::Shutdown() {
  ShutdownDelegates();
}

void HoldingSpaceKeyedService::OnProfileAdded(Profile* profile) {
  if (profile == profile_) {
    DCHECK(profile_manager_observer_.IsObserving());
    profile_manager_observer_.Reset();
    OnProfileReady();
  }
}

void HoldingSpaceKeyedService::OnProfileReady() {
  // Record user preferences at start up.
  PrefService* const prefs = profile_->GetPrefs();
  holding_space_metrics::RecordUserPreferences({
      .previews_enabled = holding_space_prefs::IsPreviewsEnabled(prefs),
      .suggestions_expanded = holding_space_prefs::IsSuggestionsExpanded(prefs),
  });

  // Observe suspend status - the delegates will be shutdown during suspend.
  if (chromeos::PowerManagerClient::Get())
    chromeos::PowerManagerClient::Get()->AddObserver(this);

  InitializeDelegates();

  if (HoldingSpaceController::Get()) {  // May be `nullptr` in tests.
    HoldingSpaceController::Get()->RegisterClientAndModelForUser(
        account_id_, &holding_space_client_, &holding_space_model_);
  }
}

void HoldingSpaceKeyedService::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  // Shutdown all delegates and clear the model when device suspends - some
  // volumes may get unmounted during suspend, and may thus incorrectly get
  // detected as deleted when device suspends - shutting down delegates during
  // suspend avoids this issue, as it also disables file removal detection.
  ShutdownDelegates();

  // Clear the model as it will get restored from persistence when
  // delegates are re-initialized after suspend.
  holding_space_model_.RemoveAll();
}

void HoldingSpaceKeyedService::SuspendDone(base::TimeDelta sleep_duration) {
  InitializeDelegates();
}

void HoldingSpaceKeyedService::InitializeDelegates() {
  // Bail out if delegates have already been initialized - delegates are
  // shutdown on suspend, and re-initialized once suspend completes. If
  // holding space keyed service starts observing suspend state after
  // `SuspendImminent()` is sent out, original delegates may still be around.
  if (!delegates_.empty())
    return;

  // The `HoldingSpaceDownloadsDelegate` monitors the status of downloads.
  auto downloads_delegate = std::make_unique<HoldingSpaceDownloadsDelegate>(
      this, &holding_space_model_);
  downloads_delegate_ = downloads_delegate.get();
  delegates_.push_back(std::move(downloads_delegate));

  // The `HoldingSpaceFileSystemDelegate` monitors the file system for changes.
  delegates_.push_back(std::make_unique<HoldingSpaceFileSystemDelegate>(
      this, &holding_space_model_));

  // The `HoldingSpaceMetricsDelegate` records metrics.
  delegates_.push_back(std::make_unique<HoldingSpaceMetricsDelegate>(
      this, &holding_space_model_));

  // The `HoldingSpacePersistenceDelegate` manages holding space persistence.
  delegates_.push_back(std::make_unique<HoldingSpacePersistenceDelegate>(
      this, &holding_space_model_, &thumbnail_loader_,
      /*persistence_restored_callback=*/
      base::BindOnce(&HoldingSpaceKeyedService::OnPersistenceRestored,
                     weak_factory_.GetWeakPtr())));

  // The `HoldingSpaceSuggestionsDelegate` manages file suggestions (i.e. the
  // files predicted to be used).
  if (features::IsHoldingSpaceSuggestionsEnabled()) {
    delegates_.push_back(std::make_unique<HoldingSpaceSuggestionsDelegate>(
        this, &holding_space_model_));
  }

  // Initialize all delegates only after they have been added to our collection.
  // Delegates should not fire their respective callbacks during construction
  // but once they have been initialized they are free to do so.
  for (auto& delegate : delegates_)
    delegate->Init();
}

void HoldingSpaceKeyedService::ShutdownDelegates() {
  downloads_delegate_ = nullptr;
  delegates_.clear();
}

void HoldingSpaceKeyedService::OnPersistenceRestored() {
  for (auto& delegate : delegates_)
    delegate->NotifyPersistenceRestored();
}

void HoldingSpaceKeyedService::MakeDriveItemAvailableOffline(
    const storage::FileSystemURL& file_system_url) {
  auto* drive_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);

  bool drive_fs_mounted = drive_service && drive_service->IsMounted();
  if (!drive_fs_mounted)
    return;

  if (!drive_service->GetDriveFsInterface())
    return;

  base::FilePath path;
  if (drive_service->GetRelativeDrivePath(file_system_url.path(), &path)) {
    drive_service->GetDriveFsInterface()->SetPinned(path, true,
                                                    base::DoNothing());
  }
}

std::unique_ptr<HoldingSpaceItem> HoldingSpaceKeyedService::CreateItemOfType(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path,
    const HoldingSpaceProgress& progress,
    HoldingSpaceImage::PlaceholderImageSkiaResolver
        placeholder_image_skia_resolver) {
  const GURL file_system_url =
      holding_space_util::ResolveFileSystemUrl(profile_, file_path);
  if (file_system_url.is_empty())
    return nullptr;

  return HoldingSpaceItem::CreateFileBackedItem(
      type, file_path, file_system_url, progress,
      base::BindOnce(
          &holding_space_util::ResolveImageWithPlaceholderImageSkiaResolver,
          &thumbnail_loader_, placeholder_image_skia_resolver));
}

}  // namespace ash
