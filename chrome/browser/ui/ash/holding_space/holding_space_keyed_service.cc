// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"

#include <set>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
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

// TODO(crbug.com/40150129): Track alternative type in `HoldingSpaceItem`.
// Returns a holding space item other than the one provided which is backed by
// the same file path in the specified `model`.
std::optional<const HoldingSpaceItem*> GetAlternativeHoldingSpaceItem(
    const HoldingSpaceModel& model,
    const HoldingSpaceItem* item) {
  for (const auto& candidate_item : model.items()) {
    if (candidate_item.get() == item)
      continue;
    if (candidate_item->file().file_path == item->file().file_path) {
      return candidate_item.get();
    }
  }
  return std::nullopt;
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
  // TODO(crbug.com/40150129): Move to `ash::holding_space_prefs`.
  HoldingSpacePersistenceDelegate::RegisterProfilePrefs(registry);
}

void HoldingSpaceKeyedService::AddPinnedFiles(
    const std::vector<storage::FileSystemURL>& file_system_urls,
    holding_space_metrics::EventSource event_source) {
  if (!IsInitialized()) {
    return;
  }

  std::vector<std::unique_ptr<HoldingSpaceItem>> items;
  std::vector<const HoldingSpaceItem*> items_to_record;
  for (const storage::FileSystemURL& file_system_url : file_system_urls) {
    if (ContainsPinnedFile(file_system_url))
      continue;

    items.push_back(HoldingSpaceItem::CreateFileBackedItem(
        HoldingSpaceItem::Type::kPinnedFile,
        HoldingSpaceFile(file_system_url.path(),
                         holding_space_util::ResolveFileSystemType(
                             profile_, file_system_url.ToGURL()),
                         file_system_url.ToGURL()),
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
      items_to_record, holding_space_metrics::ItemAction::kPin, event_source);

  AddItems(std::move(items), /*allow_duplicates=*/false);
}

void HoldingSpaceKeyedService::RemovePinnedFiles(
    const std::vector<storage::FileSystemURL>& file_system_urls,
    holding_space_metrics::EventSource event_source) {
  if (!IsInitialized()) {
    return;
  }

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
      items_to_record, holding_space_metrics::ItemAction::kUnpin, event_source);

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
      pinned_files.push_back(item->file().file_system_url);
  }
  return pinned_files;
}

void HoldingSpaceKeyedService::RefreshSuggestions() {
  if (suggestions_delegate_) {
    suggestions_delegate_->RefreshSuggestions();
  }
}

void HoldingSpaceKeyedService::RemoveSuggestions(
    const std::vector<base::FilePath>& absolute_file_paths) {
  if (suggestions_delegate_) {
    suggestions_delegate_->RemoveSuggestions(absolute_file_paths);
  }
}

void HoldingSpaceKeyedService::SetSuggestions(
    const std::vector<std::pair<HoldingSpaceItem::Type, base::FilePath>>&
        suggestions) {
  if (!IsInitialized()) {
    return;
  }

  std::set<std::string> item_ids_to_remove;

  // Gather `existing_suggestions`. Note that suggestions are reversed in the
  // holding space model to account for the fact that items are presented in
  // reverse-chronological order.
  std::vector<const HoldingSpaceItem*> existing_suggestions;
  for (const auto& item : base::Reversed(holding_space_model_.items())) {
    if (HoldingSpaceItem::IsSuggestionType(item->type())) {
      existing_suggestions.emplace_back(item.get());
      item_ids_to_remove.insert(item->id());
    }
  }

  // No-op if `existing_suggestions` are unchanged.
  if (base::ranges::equal(existing_suggestions, suggestions, /*pred=*/{},
                          [](const HoldingSpaceItem* item) {
                            return std::make_pair(item->type(),
                                                  item->file().file_path);
                          })) {
    return;
  }

  // Construct `items_to_add` from `suggestions`. Note that any pre-existing
  // items which would ideally be recycled are replaced due to the fact that the
  // holding space model doesn't currently support reordering.
  std::vector<std::unique_ptr<HoldingSpaceItem>> items_to_add;
  for (const auto& [type, file_path] : base::Reversed(suggestions)) {
    std::unique_ptr<HoldingSpaceItem> item;
    if (auto existing_item =
            base::ranges::find_if(existing_suggestions,
                                  [&](const HoldingSpaceItem* item) {
                                    return item->type() == type &&
                                           item->file().file_path == file_path;
                                  });
        existing_item != existing_suggestions.end() &&
        !(*existing_item)->IsInitialized()) {
      // Reuse the existing uninitialized file suggestion item to avoid
      // resolving the suggested file's URL. Because `*existing_item` is
      // uninitialized, its removal does not incur visual changes.
      item = holding_space_model_.TakeItem((*existing_item)->id());
      item_ids_to_remove.erase(item->id());
    } else {
      item = CreateItemOfType(
          type, file_path,
          /*progress=*/HoldingSpaceProgress(),
          /*placeholder_image_skia_resolver=*/base::NullCallback());
    }

    if (item)
      items_to_add.push_back(std::move(item));
  }

  // Add new items before removing old items to prevent UI from transitioning to
  // an empty state if the model is only temporarily becoming empty.
  AddItems(std::move(items_to_add), /*allow_duplicates=*/true);
  holding_space_model_.RemoveItems(item_ids_to_remove);
}

const std::string& HoldingSpaceKeyedService::AddItem(
    std::unique_ptr<HoldingSpaceItem> item) {
  if (!IsInitialized()) {
    return base::EmptyString();
  }

  std::vector<std::unique_ptr<HoldingSpaceItem>> items;
  items.push_back(std::move(item));
  return AddItems(std::move(items), /*allow_duplicates=*/false).at(0);
}

const std::string& HoldingSpaceKeyedService::AddItemOfType(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path,
    const HoldingSpaceProgress& progress,
    HoldingSpaceImage::PlaceholderImageSkiaResolver
        placeholder_image_skia_resolver) {
  if (!IsInitialized()) {
    return base::EmptyString();
  }

  std::unique_ptr<HoldingSpaceItem> item = CreateItemOfType(
      type, file_path, progress, placeholder_image_skia_resolver);
  if (!item)
    return base::EmptyString();

  return AddItem(std::move(item));
}

bool HoldingSpaceKeyedService::ContainsItem(const std::string& id) const {
  return holding_space_model_.GetItem(id) != nullptr;
}

std::unique_ptr<HoldingSpaceModel::ScopedItemUpdate>
HoldingSpaceKeyedService::UpdateItem(const std::string& id) {
  return IsInitialized() ? holding_space_model_.UpdateItem(id) : nullptr;
}

void HoldingSpaceKeyedService::RemoveAll() {
  if (IsInitialized()) {
    holding_space_model_.RemoveAll();
  }
}

void HoldingSpaceKeyedService::RemoveItem(const std::string& id) {
  if (IsInitialized()) {
    holding_space_model_.RemoveItem(id);
  }
}

std::optional<holding_space_metrics::ItemLaunchFailureReason>
HoldingSpaceKeyedService::OpenItemWhenComplete(const HoldingSpaceItem* item) {
  // Currently it is only possible to open download type items when complete.
  if (HoldingSpaceItem::IsDownloadType(item->type()) && downloads_delegate_) {
    return downloads_delegate_->OpenWhenComplete(item);
  }
  return holding_space_metrics::ItemLaunchFailureReason::kNoHandlerForItemType;
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

std::vector<std::reference_wrapper<const std::string>>
HoldingSpaceKeyedService::AddItems(
    std::vector<std::unique_ptr<HoldingSpaceItem>> items,
    bool allow_duplicates) {
  std::vector<std::reference_wrapper<const std::string>> result;
  std::vector<std::unique_ptr<HoldingSpaceItem>> items_to_add;

  for (auto& item : items) {
    // Ignore any `items` that already exist in the `holding_space_model_` if
    // `allow_duplicates` is false.
    if (!allow_duplicates && holding_space_model_.ContainsItem(
                                 item->type(), item->file().file_path)) {
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
    if (holding_space_prefs::MarkTimeOfFirstAdd(profile_->GetPrefs())) {
      RecordTimeFromFirstAvailabilityToFirstAdd(profile_);
    }
    holding_space_model_.AddItems(std::move(items_to_add));
  }

  return result;
}

void HoldingSpaceKeyedService::InitializeDelegates() {
  // Bail out if delegates have already been initialized - delegates are
  // shutdown on suspend, and re-initialized once suspend completes. If
  // holding space keyed service starts observing suspend state after
  // `SuspendImminent()` is sent out, original delegates may still be around.
  if (!delegates_.empty()) {
    return;
  }

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
    auto suggestions_delegate =
        std::make_unique<HoldingSpaceSuggestionsDelegate>(
            this, &holding_space_model_);
    suggestions_delegate_ = suggestions_delegate.get();
    delegates_.push_back(std::move(suggestions_delegate));
  }

  // Initialize all delegates only after they have been added to our collection.
  // Delegates should not fire their respective callbacks during construction
  // but once they have been initialized they are free to do so.
  for (auto& delegate : delegates_)
    delegate->Init();
}

void HoldingSpaceKeyedService::ShutdownDelegates() {
  downloads_delegate_ = nullptr;
  suggestions_delegate_ = nullptr;
  delegates_.clear();
}

void HoldingSpaceKeyedService::OnPersistenceRestored(
    std::vector<std::unique_ptr<HoldingSpaceItem>> restored_items) {
  AddItems(std::move(restored_items), /*allow_duplicates=*/false);
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

bool HoldingSpaceKeyedService::IsInitialized() const {
  return delegates_.size() &&
         base::ranges::none_of(
             delegates_,
             &HoldingSpaceKeyedServiceDelegate::is_restoring_persistence);
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
      type,
      HoldingSpaceFile(
          file_path,
          holding_space_util::ResolveFileSystemType(profile_, file_system_url),
          file_system_url),
      progress,
      base::BindOnce(
          &holding_space_util::ResolveImageWithPlaceholderImageSkiaResolver,
          &thumbnail_loader_, placeholder_image_skia_resolver));
}

}  // namespace ash
