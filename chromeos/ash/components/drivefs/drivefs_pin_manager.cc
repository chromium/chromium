// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"

#include <locale>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
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

class NumPunct : public std::numpunct<char> {
 private:
  char do_thousands_sep() const override { return ','; }
  std::string do_grouping() const override { return "\3"; }
};

template <typename T>
struct Quoter {
  const T& value;
};

template <typename T>
Quoter<T> Quote(const T& value) {
  return {value};
}

std::ostream& operator<<(std::ostream& out, Quoter<base::FilePath> q) {
  return out << "'" << q.value << "'";
}

std::ostream& operator<<(std::ostream& out, Quoter<std::string> q) {
  return out << "'" << q.value << "'";
}

template <typename T>
std::ostream& operator<<(std::ostream& out, Quoter<absl::optional<T>> q) {
  if (!q.value.has_value()) {
    return out << "(nullopt)";
  }

  return out << Quote(*q.value);
}

std::ostream& operator<<(std::ostream& out,
                         Quoter<mojom::FileMetadata::Type> q) {
  using Type = mojom::FileMetadata::Type;
  switch (q.value) {
#define PRINT(s)   \
  case Type::k##s: \
    return out << #s;
    PRINT(File)
    PRINT(Hosted)
    PRINT(Directory)
#undef PRINT
  }

  return out << "Type(" << static_cast<std::underlying_type_t<Type>>(q.value)
             << ")";
}

std::ostream& operator<<(std::ostream& out, Quoter<mojom::ItemEvent::State> q) {
  using State = mojom::ItemEvent::State;
  switch (q.value) {
#define PRINT(s)    \
  case State::k##s: \
    return out << #s;
    PRINT(Queued)
    PRINT(InProgress)
    PRINT(Completed)
    PRINT(Failed)
#undef PRINT
  }

  return out << "State(" << static_cast<std::underlying_type_t<State>>(q.value)
             << ")";
}

std::ostream& operator<<(std::ostream& out, Quoter<mojom::FileMetadata> q) {
  const mojom::FileMetadata& md = q.value;
  return out << "{type: " << Quote(md.type)
             << ", size: " << HumanReadableSize(md.size)
             << ", pinned: " << md.pinned << ", can_pin: "
             << (md.can_pin == mojom::FileMetadata::CanPinStatus::kOk)
             << ", available_offline: " << md.available_offline
             << ", shared: " << md.shared << ", starred: " << md.starred
             << ", item_id = " << Quote(md.item_id) << "}";
}

std::ostream& operator<<(std::ostream& out, Quoter<mojom::ItemEvent> q) {
  const mojom::ItemEvent& e = q.value;
  return out << "{state: " << Quote(e.state) << ", path: " << Quote(e.path)
             << ", bytes_transferred: " << e.bytes_transferred
             << ", bytes_to_transfer: " << e.bytes_to_transfer << "}";
}

constexpr int64_t kAverageHostedFileSizeInBytes = 7800;

}  // namespace

std::ostream& operator<<(std::ostream& out, HumanReadableSize size) {
  int64_t i = static_cast<int64_t>(size);
  if (i == 0) {
    return out << '0';
  }

  if (i < 0) {
    out << '-';
    i = -i;
  }

  {
    static const base::NoDestructor<std::locale> with_separators(
        std::locale::classic(), new NumPunct);
    std::locale old_locale = out.imbue(*with_separators);
    out << i;
    out.imbue(std::move(old_locale));
  }

  if (i < 1024) {
    return out;
  }

  double d = static_cast<double>(i) / 1024;
  const char* unit = "KMGT";
  while (d >= 1024 && *unit != '\0') {
    d /= 1024;
    unit++;
  }

  const int precision = d < 10 ? 2 : d < 100 ? 1 : 0;
  return out << base::StringPrintf(" (%.*f %c)", precision, d, *unit);
}

std::ostream& operator<<(std::ostream& out, const SetupError error) {
  switch (error) {
#define PRINT(s)         \
  case SetupError::k##s: \
    return out << #s;
    PRINT(Success)
    PRINT(ManagerDisabled)
    PRINT(ManagerStopped)
    PRINT(CannotCalculateFreeSpace)
    PRINT(CannotRetrieveSearchResults)
    PRINT(CannotPinItem)
    PRINT(NotEnoughSpace)
    PRINT(SearchQueryNotBound)
#undef PRINT
  }

  return out << "SetupError("
             << static_cast<std::underlying_type_t<SetupError>>(error) << ")";
}

std::ostream& operator<<(std::ostream& out, const SetupStage stage) {
  switch (stage) {
#define PRINT(s)         \
  case SetupStage::k##s: \
    return out << #s;
    PRINT(Error)
    PRINT(NotStarted)
    PRINT(Started)
    PRINT(CalculatedFreeSpace)
    PRINT(CalculatedRequiredSpace)
    PRINT(Finished)
#undef PRINT
  }

  return out << "SetupStage("
             << static_cast<std::underlying_type_t<SetupStage>>(stage) << ")";
}

// TODO(b/261530666): This was chosen arbitrarily, this should be experimented
// with and potentially made dynamic depending on feedback of the in progress
// queue.
constexpr base::TimeDelta kPeriodicRemovalInterval = base::Seconds(10);

constexpr char kGCacheFolderName[] = "GCache";

DriveFsPinManager::InProgressSyncingItems::InProgressSyncingItems() = default;

DriveFsPinManager::InProgressSyncingItems::~InProgressSyncingItems() = default;

void DriveFsPinManager::InProgressSyncingItems::AddItem(
    const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Emplace an item with no progress (yet). The progress values will get
  // updated in the `OnSyncingStatusUpdate`.
  const auto [it, ok] = in_progress_items_.try_emplace(path);
  LOG_IF(ERROR, !ok) << "Cannot add item " << Quote(path)
                     << ": There is already an item with progress "
                     << HumanReadableSize(it->second.transferred) << " / "
                     << HumanReadableSize(it->second.total);
  DCHECK_EQ(path, it->first);
}

int64_t DriveFsPinManager::InProgressSyncingItems::RemoveItem(
    const std::string& path,
    const int64_t total_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(total_bytes, 0) << " for " << Quote(path);

  const auto it = in_progress_items_.find(path);
  if (it == in_progress_items_.end()) {
    // TODO(b/261530520): Items can end up in this flow when a removal is
    // attempted on an item that wasn't tracked via an explicit pin operation.
    // In this case, gracefully degrade by responding with the total bytes
    // transferred. This should ideally fail as all syncing operations should be
    // identified as they affect disk space.
    VLOG(2) << "Cannot remove " << Quote(path);
    return total_bytes_transferred_;
  }

  DCHECK_EQ(it->first, path);
  const Progress& progress = it->second;
  LOG_IF(ERROR, progress.transferred > total_bytes)
      << "Progress went backwards from "
      << HumanReadableSize(progress.transferred) << " to "
      << HumanReadableSize(total_bytes) << " for " << Quote(path);
  total_bytes_transferred_ += total_bytes - progress.transferred;
  in_progress_items_.erase(it);
  return total_bytes_transferred_;
}

int64_t DriveFsPinManager::InProgressSyncingItems::UpdateItem(
    const std::string& path,
    const int64_t bytes_transferred,
    const int64_t bytes_to_transfer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(bytes_to_transfer, 0) << " for " << Quote(path);

  if (bytes_transferred < 0) {
    LOG(ERROR) << "Negative bytes_transferred = "
               << HumanReadableSize(bytes_transferred) << " for "
               << Quote(path);
    return total_bytes_transferred_;
  }

  Progress& progress = in_progress_items_[path];
  LOG_IF(ERROR, progress.transferred > bytes_transferred)
      << "Progress went backwards from "
      << HumanReadableSize(progress.transferred) << " to "
      << HumanReadableSize(bytes_transferred) << " for " << Quote(path);
  total_bytes_transferred_ += bytes_transferred - progress.transferred;
  progress.transferred = bytes_transferred;
  progress.total = bytes_to_transfer;
  return total_bytes_transferred_;
}

size_t DriveFsPinManager::InProgressSyncingItems::GetItemCount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Syncing " << in_progress_items_.size() << " files...";
  return in_progress_items_.size();
}

std::vector<std::string>
DriveFsPinManager::InProgressSyncingItems::GetUnstartedItems() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::string> unstarted_items;
  for (const auto& [path, progress] : in_progress_items_) {
    if (progress.total <= 0) {
      unstarted_items.push_back(path);
    }
  }

  VLOG_IF(1, !unstarted_items.empty())
      << "There are " << unstarted_items.size() << " unstarted items";
  return unstarted_items;
}

bool ManagerState::SetupInProgress() const {
  return progress.stage != SetupStage::kFinished &&
         progress.stage != SetupStage::kError &&
         progress.stage != SetupStage::kNotStarted;
}

DriveFsPinManager::DriveFsPinManager(
    bool enabled,
    const base::FilePath& profile_path,
    mojom::DriveFs* drivefs_interface,
    std::unique_ptr<FreeDiskSpaceDelegate> free_disk_space)
    : enabled_(enabled),
      free_disk_space_(free_disk_space ? std::move(free_disk_space)
                                       : std::make_unique<FreeDiskSpaceImpl>()),
      profile_path_(profile_path),  // The GCache directory is located in the
                                    // users profile path.
      drivefs_interface_(drivefs_interface),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})),
      syncing_items_(
          base::SequenceBound<InProgressSyncingItems>{task_runner_}) {}

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

  VLOG(1) << "Calculating free space";
  timer_ = base::ElapsedTimer();
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
  Complete(SetupError::kManagerStopped);
}

void DriveFsPinManager::OnFreeDiskSpaceRetrieved(const int64_t free_space) {
  if (free_space < 0) {
    LOG(ERROR) << "Cannot calculate free space";
    return Complete(SetupError::kCannotCalculateFreeSpace);
  }

  VLOG(2) << "Free space: " << HumanReadableSize(free_space);
  state_.progress.stage = SetupStage::kCalculatedFreeSpace;
  state_.progress.available_disk_space = free_space;
  NotifyProgress();

  VLOG(1) << "Enumerating items to calculate required space...";
  mojom::QueryParametersPtr query = CreateMyDriveQuery();
  drivefs_interface_->StartSearchQuery(
      search_query_.BindNewPipeAndPassReceiver(), std::move(query));
  search_query_->GetNextPage(
      base::BindOnce(&DriveFsPinManager::OnSearchResultForSizeCalculation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::OnSearchResultForSizeCalculation(
    const drive::FileError error,
    const absl::optional<std::vector<mojom::QueryItemPtr>> items) {
  if (error != drive::FILE_ERROR_OK || !items) {
    LOG(ERROR) << "Cannot list files for size calculation: " << error;
    return Complete(SetupError::kCannotRetrieveSearchResults);
  }

  if (items->empty()) {
    VLOG(1) << "Computed required space in "
            << timer_.Elapsed().InMilliseconds() << " ms";
    VLOG(1) << "Required space: "
            << HumanReadableSize(state_.progress.required_disk_space);
    VLOG(1) << "Free space: "
            << HumanReadableSize(state_.progress.available_disk_space);
    return StartBatchPinning();
  }

  VLOG(2) << "Iterating over " << items->size()
          << " items for space calculation";
  for (const mojom::QueryItemPtr& item : *items) {
    DCHECK(item);
    DCHECK(item->metadata);

    const mojom::FileMetadata& md = *item->metadata;
    VLOG(2) << "path: " << Quote(item->path) << ", metadata: " << Quote(md);

    if (md.pinned) {
      VLOG(2) << "Skipped " << Quote(item->path) << ": Already pinned";
      continue;
    }

    if (md.can_pin != mojom::FileMetadata::CanPinStatus::kOk) {
      VLOG(2) << "Skipped " << Quote(item->path) << ": Cannot be pinned";
      continue;
    }

    if (md.type == mojom::FileMetadata::Type::kHosted) {
      state_.progress.required_disk_space += kAverageHostedFileSizeInBytes;
      continue;
    }

    DCHECK_GE(md.size, 0) << " for " << Quote(item->path);
    state_.progress.required_disk_space += md.size;
  }

  // TODO(b/259454320): This should really not use up all free space but instead
  // include a buffer threshold. Update this once the thresholds have been
  // identified.
  if (state_.progress.required_disk_space >=
      state_.progress.available_disk_space) {
    LOG(ERROR) << "Not enough space: Required = "
               << HumanReadableSize(state_.progress.required_disk_space)
               << ", Free = "
               << HumanReadableSize(state_.progress.available_disk_space);
    return Complete(SetupError::kNotEnoughSpace);
  }

  if (!search_query_.is_bound()) {
    return Complete(SetupError::kSearchQueryNotBound);
  }

  NotifyProgress();
  search_query_->GetNextPage(
      base::BindOnce(&DriveFsPinManager::OnSearchResultForSizeCalculation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::Complete(const SetupError error) {
  state_.progress.error = error;
  if (error == SetupError::kSuccess) {
    VLOG(1) << "Finished with success";
    state_.progress.stage = SetupStage::kFinished;
  } else {
    LOG(ERROR) << "Finished with error: " << error;
    state_.progress.stage = SetupStage::kError;
  }

  NotifyProgress();
  weak_ptr_factory_.InvalidateWeakPtrs();
  search_query_.reset();
  if (complete_callback_) {
    std::move(complete_callback_).Run(error);
  }
}

void DriveFsPinManager::StartBatchPinning() {
  // Restart the search query.
  search_query_.reset();

  state_.progress.stage = SetupStage::kCalculatedRequiredSpace;
  NotifyProgress();

  drivefs_interface_->StartSearchQuery(
      search_query_.BindNewPipeAndPassReceiver(), CreateMyDriveQuery());
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
    const drive::FileError error,
    const absl::optional<std::vector<mojom::QueryItemPtr>> items) {
  if (error != drive::FILE_ERROR_OK || !items) {
    LOG(ERROR) << "Cannot list files to pin: " << error;
    return Complete(SetupError::kCannotRetrieveSearchResults);
  }

  if (items->empty()) {
    VLOG(1) << "Pinned all files in " << timer_.Elapsed().InMilliseconds()
            << " ms";
    if (state_.progress.error_count > 0) {
      LOG(ERROR) << "There were " << state_.progress.error_count
                 << " errors while pinning files";
      return Complete(SetupError::kCannotPinItem);
    }

    return Complete(SetupError::kSuccess);
  }

  // TODO(b/259454320): Free disk space should be retrieved here and after the
  // batch of pinning operations has completed to identify if any other
  // operations writing to disk might cause cause the free space to get used
  // faster than anticipated.
  bool processed = false;
  for (const mojom::QueryItemPtr& item : *items) {
    DCHECK(item);
    const base::FilePath& path = item->path;
    DCHECK(item->metadata);
    const mojom::FileMetadata& md = *item->metadata;
    VLOG(2) << "path: " << Quote(path) << ", metadata: " << Quote(md);
    if (md.pinned) {
      VLOG(2) << "Skipped " << Quote(md.type) << " " << Quote(path)
              << ": Already pinned";
      continue;
    }

    VLOG(2) << "Pinning " << Quote(md.type) << " " << Quote(path) << "...";
    syncing_items_.AsyncCall(&InProgressSyncingItems::AddItem)
        .WithArgs(path.value());
    drivefs_interface_->SetPinned(
        path, /*pinned=*/true,
        base::BindOnce(&DriveFsPinManager::OnFilePinned,
                       weak_ptr_factory_.GetWeakPtr(), path.value()));
    processed = true;
  }

  if (processed) {
    return;
  }

  if (!search_query_.is_bound()) {
    return Complete(SetupError::kSearchQueryNotBound);
  }

  VLOG(1) << "All items in current batch are already pinned";
  VLOG(1) << "Getting next batch of items...";
  search_query_->GetNextPage(
      base::BindOnce(&DriveFsPinManager::OnSearchResultsForPinning,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::OnFilePinned(const std::string& path,
                                     const drive::FileError status) {
  if (status != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot pin " << Quote(path) << ": " << status;
    state_.progress.error_count++;
    return;
  }

  VLOG(2) << "Pinned " << Quote(path);
}

void DriveFsPinManager::OnSyncingStatusUpdate(
    const mojom::SyncingStatus& status) {
  if (!enabled_ || !state_.SetupInProgress()) {
    return;
  }

  for (const mojom::ItemEventPtr& event : status.item_events) {
    DCHECK(event);
    VLOG(2) << "Got event: " << Quote(*event);

    using State = mojom::ItemEvent::State;

    switch (event->state) {
      case State::kQueued:
        VLOG(2) << "Queued " << Quote(event->path);
        continue;

      case State::kCompleted:
        VLOG(2) << "Synced " << Quote(event->path);
        GetMetadataForPath(event->path);
        continue;

      case State::kFailed:
        LOG(ERROR) << "Cannot sync " << Quote(event->path);
        state_.progress.error_count++;
        continue;

      case State::kInProgress:
        LOG_IF(ERROR, event->bytes_transferred < 0)
            << "Negative bytes_transferred " << event->bytes_transferred
            << " for " << Quote(event->path);
        LOG_IF(ERROR, event->bytes_to_transfer < 0)
            << "Negative bytes_to_transfer " << event->bytes_to_transfer
            << " for " << Quote(event->path);
        syncing_items_.AsyncCall(&InProgressSyncingItems::UpdateItem)
            .WithArgs(event->path, event->bytes_transferred,
                      event->bytes_to_transfer)
            .Then(
                base::BindOnce(&DriveFsPinManager::ReportTotalBytesTransferred,
                               weak_ptr_factory_.GetWeakPtr()));
        continue;
    }
  }

  syncing_items_.AsyncCall(&InProgressSyncingItems::GetItemCount)
      .Then(base::BindOnce(&DriveFsPinManager::MaybeStartSearch,
                           weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::ReportTotalBytesTransferred(
    int64_t total_bytes_transferred) {
  LOG_IF(ERROR, state_.progress.pinned_disk_space > total_bytes_transferred)
      << "Pinned space went backwards from "
      << HumanReadableSize(state_.progress.pinned_disk_space) << " to "
      << HumanReadableSize(total_bytes_transferred);
  state_.progress.pinned_disk_space = total_bytes_transferred;
  NotifyProgress();
}

void DriveFsPinManager::MaybeStartSearch(size_t remaining_items) {
  if (!search_query_.is_bound()) {
    return Complete(SetupError::kSearchQueryNotBound);
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

void DriveFsPinManager::OnError(const mojom::DriveError& error) {
  LOG(ERROR) << "DriveFS error " << error.type << " with " << Quote(error.path);
}

void DriveFsPinManager::NotifyProgress() {
  if (observers_.empty()) {
    return;
  }

  VLOG(3) << "Notifying observers...";
  for (DriveFsBulkPinObserver& observer : observers_) {
    observer.OnSetupProgress(state_.progress);
  }
  VLOG(3) << "Notified observers";
}

void DriveFsPinManager::AddObserver(DriveFsBulkPinObserver* observer) {
  observers_.AddObserver(observer);
}

void DriveFsPinManager::RemoveObserver(DriveFsBulkPinObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DriveFsPinManager::PeriodicallyRemovePinnedItems() {
  VLOG(1) << "Removing already pinned items from list of items to sync";

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
    const std::vector<std::string>& unstarted_paths) {
  for (const std::string& path : unstarted_paths) {
    drivefs_interface_->GetMetadata(
        base::FilePath(path),
        base::BindOnce(&DriveFsPinManager::OnMetadataRetrieved,
                       weak_ptr_factory_.GetWeakPtr(), path));
  }

  syncing_items_.AsyncCall(&InProgressSyncingItems::GetItemCount)
      .Then(base::BindOnce(&DriveFsPinManager::MaybeStartSearch,
                           weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::GetMetadataForPath(const std::string& path) {
  drivefs_interface_->GetMetadata(
      base::FilePath(path),
      base::BindOnce(&DriveFsPinManager::OnMetadataRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), path));
}

void DriveFsPinManager::OnMetadataRetrieved(
    const std::string& path,
    const drive::FileError error,
    const mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot get metadata of " << Quote(path) << ": " << error;
    return;
  }

  DCHECK(metadata);
  VLOG(2) << "path: " << Quote(path) << ", metadata: " << Quote(*metadata);

  if (metadata->available_offline || metadata->size == 0) {
    const int64_t file_size =
        (metadata->type == mojom::FileMetadata::Type::kHosted)
            ? kAverageHostedFileSizeInBytes
            : metadata->size;

    VLOG_IF(2, metadata->available_offline)
        << "Skipped " << Quote(path) << ": Already available offline";
    VLOG_IF(2, metadata->size == 0)
        << "Skipped " << Quote(path) << ": Empty file";
    syncing_items_.AsyncCall(&InProgressSyncingItems::RemoveItem)
        .WithArgs(std::move(path), file_size)
        .Then(base::BindOnce(&DriveFsPinManager::ReportTotalBytesTransferred,
                             weak_ptr_factory_.GetWeakPtr()));
  }
}

}  // namespace drivefs::pinning
