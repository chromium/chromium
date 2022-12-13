// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/drive/file_errors.h"

namespace drivefs::pinning {

namespace {

mojom::QueryParametersPtr CreateMyDriveQuery() {
  mojom::QueryParametersPtr query = mojom::QueryParameters::New();
  // TODO(b/259454320): 50 is chosen arbitrarily, this needs to be updated as
  // different batch sizes are experimented with.
  query->page_size = 50;
  query->query_kind = mojom::QueryKind::kRegular;
  query->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  // TODO(b/259454320): The query.proto for this says the C++ clients don't
  // handle `false` for this boolean, need to investigate if that is true or
  // not.
  query->available_offline = false;
  query->shared_with_me = false;
  return query;
}

class FreeDiskSpaceImpl : public FreeDiskSpaceDelegate {
 public:
  FreeDiskSpaceImpl() = default;

  FreeDiskSpaceImpl(const FreeDiskSpaceImpl&) = delete;
  FreeDiskSpaceImpl& operator=(const FreeDiskSpaceImpl&) = delete;

  ~FreeDiskSpaceImpl() override = default;

  void AmountOfFreeDiskSpace(
      const base::FilePath& path,
      base::OnceCallback<void(int64_t)> callback) override {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace, path),
        std::move(callback));
  }
};

}  // namespace

// TODO(b/261530666): This was chosen arbitrarily, this should be experimented
// with and potentially made dynamic depending on feedback of the in progress
// queue.
constexpr base::TimeDelta kPeriodicRemovalInterval = base::Seconds(10);

constexpr char kGCacheFolderName[] = "GCache";

DriveFsPinManager::InProgressSyncingItems::InProgressSyncingItems() = default;

DriveFsPinManager::InProgressSyncingItems::~InProgressSyncingItems() = default;

void DriveFsPinManager::InProgressSyncingItems::AddItem(
    const std::string path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Emplace an item with no progress, these values (i.e. 0,0) will get updated
  // in the `OnSyncingStatusUpdate`.
  in_progress_items_.try_emplace(path, /*bytes_transferred=*/0,
                                 /*bytes_to_transfer=*/0);
}

int64_t DriveFsPinManager::InProgressSyncingItems::RemoveItem(
    const std::string path,
    int64_t total_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = in_progress_items_.find(path);
  if (it == in_progress_items_.end()) {
    // TODO(b/261530520): Items can end up in this flow when a removal is
    // attempted on an item that wasn't tracked via an explicit pin operation.
    // In this case, gracefully degrade by responding with the total bytes
    // transferred. This should ideally fail as all syncing operations should be
    // identified as they affect disk space.
    return total_bytes_transferred_;
  }
  total_bytes_transferred_ += total_bytes - it->second.first;
  in_progress_items_.erase(it);
  return total_bytes_transferred_;
}

int64_t DriveFsPinManager::InProgressSyncingItems::UpdateItem(
    const std::string path,
    int64_t bytes_transferred,
    int64_t bytes_to_transfer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = in_progress_items_.find(path);
  if (it == in_progress_items_.end()) {
    // TODO(b/261530520): Items can end up in this flow when an update is
    // attempted on an item that wasn't tracked via an explicit pin operation.
    // In this case, gracefully degrade by responding with the total bytes
    // transferred. This should ideally fail as all syncing operations should be
    // identified as they affect disk space.
    return total_bytes_transferred_;
  }
  total_bytes_transferred_ += bytes_transferred - it->second.first;
  it->second.first = bytes_transferred;
  it->second.second = bytes_to_transfer;
  return total_bytes_transferred_;
}

size_t DriveFsPinManager::InProgressSyncingItems::GetItemCount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Remaining syncing items: " << in_progress_items_.size();
  return in_progress_items_.size();
}

std::vector<std::string>
DriveFsPinManager::InProgressSyncingItems::GetUnstartedItems() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::string> unstarted_items;
  for (const auto& item : in_progress_items_) {
    if (item.second.second > 0) {
      continue;
    }
    unstarted_items.emplace_back(item.first);
  }
  return unstarted_items;
}

void SetupProgress::Reset() {
  required_disk_space = 0;
  available_disk_space = 0;
  pinned_disk_space = 0;
  stage = SetupStage::kNotStarted;
}

bool ManagerState::SetupInProgress() {
  return progress.stage != SetupStage::kFinishedSetup &&
         progress.stage != SetupStage::kFinishedSetupWithError &&
         progress.stage != SetupStage::kNotStarted;
}

DriveFsPinManager::DriveFsPinManager(bool enabled,
                                     const base::FilePath& profile_path,
                                     mojom::DriveFs* drivefs_interface)
    : enabled_(enabled),
      free_disk_space_(std::make_unique<FreeDiskSpaceImpl>()),
      profile_path_(profile_path),  // The GCache directory is located in the
                                    // users profile path.
      drivefs_interface_(drivefs_interface),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})),
      syncing_items_(
          base::SequenceBound<InProgressSyncingItems>{task_runner_}) {}

DriveFsPinManager::DriveFsPinManager(
    bool enabled,
    const base::FilePath& profile_path,
    mojom::DriveFs* drivefs_interface,
    std::unique_ptr<FreeDiskSpaceDelegate> free_disk_space)
    : DriveFsPinManager(enabled, profile_path, drivefs_interface) {
  free_disk_space_ = std::move(free_disk_space);
}

DriveFsPinManager::~DriveFsPinManager() = default;

// TODO(b/259454320): Pass through a `base::RepeatingCallback` here to enable
// the callsite to receive progress updates.
void DriveFsPinManager::Start(
    base::OnceCallback<void(SetupError)> complete_callback) {
  if (!enabled_) {
    LOG(ERROR) << "The pin manager is not enabled";
    std::move(complete_callback).Run(SetupError::kManagerDisabled);
    return;
  }

  VLOG(1) << "Caculating free disk space";
  timer_.Begin();
  complete_callback_ = std::move(complete_callback);
  state_.progress.Reset();
  state_.progress.stage = SetupStage::kStarted;
  NotifyProgress();

  base::FilePath gcache_path(profile_path_.AppendASCII(kGCacheFolderName));

  free_disk_space_->AmountOfFreeDiskSpace(
      gcache_path, base::BindOnce(&DriveFsPinManager::OnFreeDiskSpaceRetrieved,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::Stop() {
  Complete(SetupError::kErrorManagerStopped);
}

void DriveFsPinManager::OnFreeDiskSpaceRetrieved(int64_t free_space) {
  if (free_space == -1) {
    LOG(ERROR) << "Error calculating free disk space";
    std::move(complete_callback_)
        .Run(SetupError::kErrorCalculatingFreeDiskSpace);
    return;
  }

  state_.progress.stage = SetupStage::kCalculatedFreeLocalDiskSpace;
  state_.progress.available_disk_space = free_space;
  NotifyProgress();

  VLOG(1) << "Starting to search for items to calculate required space";
  VLOG(2) << "Free disk space in bytes: "
          << state_.progress.available_disk_space;
  mojom::QueryParametersPtr query = CreateMyDriveQuery();
  drivefs_interface_->StartSearchQuery(
      search_query_.BindNewPipeAndPassReceiver(), std::move(query));
  search_query_->GetNextPage(
      base::BindOnce(&DriveFsPinManager::OnSearchResultForSizeCalculation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::OnSearchResultForSizeCalculation(
    drive::FileError error,
    absl::optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Error retrieving search results for size calculation: "
               << error;
    Complete(SetupError::kErrorRetrievingSearchResults);
    return;
  }

  if (!items.has_value()) {
    LOG(ERROR) << "Items returned are invalid";
    Complete(SetupError::kErrorResultsReturnedInvalid);
    return;
  }

  if (items.value().size() == 0) {
    VLOG(1) << "Iterated all files and calculated "
            << state_.progress.required_disk_space << " bytes required with "
            << state_.progress.available_disk_space << " bytes available in "
            << timer_.Elapsed().InMilliseconds() << "ms";
    StartBatchPinning();
    return;
  }

  VLOG(2) << "Iterating over " << items.value().size()
          << " for space calculation";
  for (const auto& item : items.value()) {
    if (item->metadata->pinned) {
      VLOG(2) << "Item is already pinned, ignoring in space calculation";
      continue;
    }
    state_.progress.required_disk_space += item->metadata->size;
  }

  // TODO(b/259454320): This should really not use up all free space but instead
  // include a buffer threshold. Update this once the thresholds have been
  // identified.
  if (state_.progress.required_disk_space >=
      state_.progress.available_disk_space) {
    LOG(ERROR) << "The required size (" << state_.progress.required_disk_space
               << " bytes) exceeds the available free space ("
               << state_.progress.available_disk_space << "bytes)";
    Complete(SetupError::kErrorNotEnoughFreeSpace);
    return;
  }

  if (!search_query_.is_bound()) {
    Complete(SetupError::kErrorSearchQueryNotBound);
    return;
  }

  NotifyProgress();
  search_query_->GetNextPage(
      base::BindOnce(&DriveFsPinManager::OnSearchResultForSizeCalculation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::Complete(SetupError status) {
  state_.progress.stage = (status == SetupError::kSuccess)
                              ? SetupStage::kFinishedSetup
                              : SetupStage::kFinishedSetupWithError;
  NotifyProgress();
  weak_ptr_factory_.InvalidateWeakPtrs();
  search_query_.reset();
  if (complete_callback_) {
    std::move(complete_callback_).Run(status);
  }
}

void DriveFsPinManager::StartBatchPinning() {
  // Restart the search query.
  search_query_.reset();

  state_.progress.stage = SetupStage::kCalculatedRequiredDiskSpace;
  NotifyProgress();

  mojom::QueryParametersPtr query = CreateMyDriveQuery();
  drivefs_interface_->StartSearchQuery(
      search_query_.BindNewPipeAndPassReceiver(), std::move(query));
  search_query_->GetNextPage(
      base::BindOnce(&DriveFsPinManager::OnSearchResultsForPinning,
                     weak_ptr_factory_.GetWeakPtr()));

  // Start a periodic task that removes any files that are already available
  // offline from the `in_progress_items_` map.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DriveFsPinManager::PeriodicallyRemovePinnedItems,
                     weak_ptr_factory_.GetWeakPtr()),
      kPeriodicRemovalInterval);
}

void DriveFsPinManager::OnSearchResultsForPinning(
    drive::FileError error,
    absl::optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Error retrieving search results to pin: " << error;
    Complete(SetupError::kErrorRetrievingSearchResultsForPinning);
    return;
  }

  if (!items.has_value()) {
    LOG(ERROR) << "Items returned are invalid";
    Complete(SetupError::kErrorResultsReturnedInvalidForPinning);
    return;
  }

  if (items.value().size() == 0) {
    VLOG(1) << "Finished pinning all files in "
            << timer_.Elapsed().InMilliseconds() << "ms";
    Complete(SetupError::kSuccess);
    return;
  }

  // TODO(b/259454320): Free disk space should be retrieved here and after the
  // batch of pinning operations has completed to identify if any other
  // operations writing to disk might cause cause the free space to get used
  // faster than anticipated.
  auto unpinned_items =
      base::ranges::count_if(items.value().begin(), items.value().end(),
                             [](const drivefs::mojom::QueryItemPtr& item) {
                               return !item->metadata->pinned;
                             });

  if (unpinned_items == 0) {
    if (!search_query_.is_bound()) {
      Complete(SetupError::kErrorSearchQueryNotBound);
      return;
    }
    VLOG(1) << "All items in current batch are already pinned";
    search_query_->GetNextPage(
        base::BindOnce(&DriveFsPinManager::OnSearchResultsForPinning,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  for (const auto& item : items.value()) {
    if (item->metadata->pinned) {
      VLOG(2) << "Item is already pinned, ignoring when batch pinning";
      continue;
    }
    base::FilePath path(item->path);
    drivefs_interface_->SetPinned(
        path, /*pinned=*/true,
        base::BindOnce(&DriveFsPinManager::OnFilePinned,
                       weak_ptr_factory_.GetWeakPtr(), path.value()));
  }
}

void DriveFsPinManager::OnFilePinned(const std::string& path,
                                     drive::FileError status) {
  if (status != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Failed pinning an item: " << status;
    VLOG(1) << "Path that failed to pin: " << path << " with error "
            << drive::FileErrorToString(status);
    Complete(SetupError::kErrorFailedToPinItem);
    return;
  }

  syncing_items_.AsyncCall(&InProgressSyncingItems::AddItem).WithArgs(path);
}

void DriveFsPinManager::OnSyncingStatusUpdate(
    const mojom::SyncingStatus& status) {
  if (!enabled_ || !state_.SetupInProgress()) {
    return;
  }

  for (const auto& item : status.item_events) {
    auto cloned_item = item.Clone();
    // TODO(b/259454320): Hosted files (e.g. gdoc) do not send an update via the
    // `OnSyncingStatusUpdate` method. Need to add a method to cleanse the
    // `in_progress_items_` map to ensure any values that are small enough or
    // optimistically pinned get removed.
    if (cloned_item->state == mojom::ItemEvent::State::kCompleted) {
      VLOG(2) << "Finished syncing " << cloned_item->path;
      GetMetadataForPath(base::FilePath(cloned_item->path));
      continue;
    }
    syncing_items_.AsyncCall(&InProgressSyncingItems::UpdateItem)
        .WithArgs(cloned_item->path, cloned_item->bytes_transferred,
                  cloned_item->bytes_to_transfer)
        .Then(base::BindOnce(&DriveFsPinManager::ReportTotalBytesTransferred,
                             weak_ptr_factory_.GetWeakPtr()));
  }

  syncing_items_.AsyncCall(&InProgressSyncingItems::GetItemCount)
      .Then(base::BindOnce(&DriveFsPinManager::MaybeStartSearch,
                           weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::ReportTotalBytesTransferred(
    int64_t total_bytes_transferred) {
  state_.progress.pinned_disk_space = total_bytes_transferred;
  NotifyProgress();
}

void DriveFsPinManager::MaybeStartSearch(size_t remaining_items) {
  if (!search_query_.is_bound()) {
    Complete(SetupError::kErrorSearchQueryNotBound);
    return;
  }

  if (remaining_items == 0) {
    search_query_->GetNextPage(
        base::BindOnce(&DriveFsPinManager::OnSearchResultsForPinning,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void DriveFsPinManager::OnUnmounted() {}
void DriveFsPinManager::OnFilesChanged(
    const std::vector<mojom::FileChange>& changes) {}
void DriveFsPinManager::OnError(const mojom::DriveError& error) {}

void DriveFsPinManager::NotifyProgress() {
  VLOG_IF(2, !observers_.empty()) << "Notifying progress to list of observers";
  for (auto& observer : observers_) {
    observer.OnSetupProgress(state_.progress);
  }
}

void DriveFsPinManager::AddObserver(DriveFsBulkPinObserver* observer) {
  observers_.AddObserver(observer);
}

void DriveFsPinManager::RemoveObserver(DriveFsBulkPinObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DriveFsPinManager::PeriodicallyRemovePinnedItems() {
  VLOG(1) << "Periodically removing pinned items";

  syncing_items_.AsyncCall(&InProgressSyncingItems::GetUnstartedItems)
      .Then(base::BindOnce(&DriveFsPinManager::GetMetadata,
                           weak_ptr_factory_.GetWeakPtr()));

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DriveFsPinManager::PeriodicallyRemovePinnedItems,
                     weak_ptr_factory_.GetWeakPtr()),
      kPeriodicRemovalInterval);
}

void DriveFsPinManager::GetMetadata(
    const std::vector<std::string> unstarted_paths) {
  for (const auto& path : unstarted_paths) {
    base::FilePath file_path(path);
    drivefs_interface_->GetMetadata(
        file_path,
        base::BindOnce(&DriveFsPinManager::OnMetadataRetrieved,
                       weak_ptr_factory_.GetWeakPtr(), file_path.value()));
  }

  syncing_items_.AsyncCall(&InProgressSyncingItems::GetItemCount)
      .Then(base::BindOnce(&DriveFsPinManager::MaybeStartSearch,
                           weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::GetMetadataForPath(const base::FilePath& path) {
  drivefs_interface_->GetMetadata(
      path, base::BindOnce(&DriveFsPinManager::OnMetadataRetrieved,
                           weak_ptr_factory_.GetWeakPtr(), path.value()));
}

void DriveFsPinManager::OnMetadataRetrieved(const std::string path,
                                            drive::FileError error,
                                            mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Failed to retrieve metadata: " << error;
    return;
  }

  if (metadata->available_offline || metadata->size == 0) {
    VLOG(2) << "File " << path
            << " has already been pinned or is a 0 byte file, removing from in "
               "progress items";
    syncing_items_.AsyncCall(&InProgressSyncingItems::RemoveItem)
        .WithArgs(std::move(path), metadata->size)
        .Then(base::BindOnce(&DriveFsPinManager::ReportTotalBytesTransferred,
                             weak_ptr_factory_.GetWeakPtr()));
  }
}

}  // namespace drivefs::pinning
