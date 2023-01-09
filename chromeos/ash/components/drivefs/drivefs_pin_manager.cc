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
#include "base/task/sequenced_task_runner.h"
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

std::ostream& operator<<(std::ostream& out, Quoter<mojom::FileChange::Type> q) {
  using Type = mojom::FileChange::Type;
  switch (q.value) {
#define PRINT(s)   \
  case Type::k##s: \
    return out << #s;
    PRINT(Create)
    PRINT(Delete)
    PRINT(Modify)
#undef PRINT
  }

  return out << "FileChange::Type("
             << static_cast<std::underlying_type_t<Type>>(q.value) << ")";
}

std::ostream& operator<<(std::ostream& out, Quoter<mojom::FileMetadata> q) {
  const mojom::FileMetadata& md = q.value;
  return out << "{type: " << Quote(md.type)
             << ", size: " << HumanReadableSize(md.size)
             << ", pinned: " << md.pinned << ", can_pin: "
             << (md.can_pin == mojom::FileMetadata::CanPinStatus::kOk)
             << ", available_offline: " << md.available_offline
             << ", shared: " << md.shared << ", starred: " << md.starred
             << ", stable_id: " << md.stable_id
             << ", item_id = " << Quote(md.item_id) << "}";
}

std::ostream& operator<<(std::ostream& out, Quoter<mojom::ItemEvent> q) {
  const mojom::ItemEvent& e = q.value;
  return out << "{state: " << Quote(e.state) << ", path: " << Quote(e.path)
             << ", stable_id: " << e.stable_id
             << ", bytes_transferred: " << e.bytes_transferred
             << ", bytes_to_transfer: " << e.bytes_to_transfer << "}";
}

std::ostream& operator<<(std::ostream& out, Quoter<mojom::FileChange> q) {
  const mojom::FileChange& change = q.value;
  return out << "{path: " << Quote(change.path)
             << ", type: " << Quote(change.type)
             << ", stable_id: " << change.stable_id << "}";
}

int64_t GetSize(const mojom::FileMetadata& metadata) {
  const int64_t kAverageHostedFileSize = 7800;
  return metadata.type == mojom::FileMetadata::Type::kHosted
             ? kAverageHostedFileSize
             : metadata.size;
}

bool CanPinItem(const mojom::FileMetadata& metadata,
                const base::FilePath& path) {
  using Type = mojom::FileMetadata::Type;

  if (metadata.type == Type::kDirectory) {
    VLOG(2) << "Skipped " << Quote(path) << ": Directory";
    return false;
  }

  // TODO (b/264596214) Drive shortcuts masquerade as empty files. Is there a
  // better way to recognize Drive shortcuts?
  if (metadata.type == Type::kFile && metadata.size == 0) {
    VLOG(2) << "Skipped " << Quote(path) << ": Empty file or shortcut";
    return false;
  }

  if (metadata.pinned) {
    VLOG(2) << "Skipped " << Quote(path) << ": Already pinned";
    return false;
  }

  if (metadata.can_pin != mojom::FileMetadata::CanPinStatus::kOk) {
    VLOG(2) << "Skipped " << Quote(path) << ": Cannot be pinned";
    return false;
  }

  return true;
}

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

DriveFsPinManager::TrackedFiles::~TrackedFiles() = default;
DriveFsPinManager::TrackedFiles::TrackedFiles() = default;

void DriveFsPinManager::TrackedFiles::Add(const std::string& path,
                                          const int64_t expected_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Emplace an item with no progress (yet). The progress values will get
  // updated in the `OnSyncingStatusUpdate`.
  const auto [it, ok] =
      files_.try_emplace(path, Progress{.total = expected_size});
  DCHECK_EQ(path, it->first);
  if (ok) {
    VLOG(3) << "Added " << Quote(path) << " to the tracked files";
    VLOG_IF(1, expected_size <= 0)
        << "Tracked file " << Quote(path) << " has an expected size of "
        << HumanReadableSize(expected_size);
  } else {
    LOG(ERROR) << "Cannot add " << Quote(path)
               << " to the tracked files: Conflicting entry " << it->second;
  }
}

bool DriveFsPinManager::TrackedFiles::Remove(const std::string& path,
                                             int64_t bytes_transferred) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Files::node_type node = files_.extract(path);
  if (!node) {
    return false;
  }

  DCHECK_EQ(node.key(), path);
  const Progress& progress = node.mapped();

  if (bytes_transferred < 0) {
    bytes_transferred = progress.total;
  } else {
    LOG_IF(ERROR, progress.total != bytes_transferred)
        << "Expected final progress " << HumanReadableSize(progress.total)
        << " instead of " << HumanReadableSize(bytes_transferred) << " for "
        << Quote(path);
  }

  LOG_IF(ERROR, progress.transferred > bytes_transferred)
      << "Progress went backwards from "
      << HumanReadableSize(progress.transferred) << " to "
      << HumanReadableSize(bytes_transferred) << " for " << Quote(path);
  total_bytes_transferred_ += bytes_transferred - progress.transferred;

  VLOG(3) << "Stopped tracking " << Quote(path);
  return true;
}

bool DriveFsPinManager::TrackedFiles::Update(const std::string& path,
                                             const int64_t bytes_transferred,
                                             const int64_t bytes_to_transfer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(bytes_to_transfer, 0) << " for " << Quote(path);

  if (bytes_transferred < 0) {
    LOG(ERROR) << "Negative bytes_transferred = "
               << HumanReadableSize(bytes_transferred) << " for "
               << Quote(path);
    return false;
  }

  Progress& progress = files_[path];
  if (bytes_transferred == progress.transferred &&
      bytes_to_transfer == progress.total && progress.in_progress) {
    return false;
  }

  progress.in_progress = true;

  LOG_IF(ERROR, bytes_transferred < progress.transferred)
      << "Progress went backwards from "
      << HumanReadableSize(progress.transferred) << " to "
      << HumanReadableSize(bytes_transferred) << " for " << Quote(path);

  LOG_IF(ERROR, bytes_to_transfer != progress.total)
      << "Changed expected size of " << Quote(path) << " from "
      << HumanReadableSize(progress.total) << " to "
      << HumanReadableSize(bytes_to_transfer);

  total_bytes_transferred_ += bytes_transferred - progress.transferred;
  progress.transferred = bytes_transferred;
  progress.total = bytes_to_transfer;
  return true;
}

bool DriveFsPinManager::TrackedFiles::MarkInProgress(const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Progress& progress = files_[path];
  if (progress.in_progress) {
    return false;
  }

  LOG_IF(ERROR, progress.transferred != 0)
      << "Queued file " << Quote(path) << " already has transferred "
      << HumanReadableSize(progress.transferred);

  progress.in_progress = true;
  return true;
}

size_t DriveFsPinManager::TrackedFiles::GetCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return files_.size();
}

std::vector<std::string> DriveFsPinManager::TrackedFiles::GetUnstartedPaths()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::string> paths;
  paths.reserve(files_.size());
  for (const auto& [path, progress] : files_) {
    if (!progress.in_progress) {
      paths.push_back(path);
    }
  }
  return paths;
}

void DriveFsPinManager::TrackedFiles::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  files_.clear();
  total_bytes_transferred_ = 0;
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
      drivefs_interface_(drivefs_interface) {}

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

  VLOG(1) << "Calculating free space...";
  timer_ = base::ElapsedTimer();
  complete_callback_ = std::move(complete_callback);
  tracked_files_.Reset();
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

  VLOG(1) << "Calculating required space...";
  drivefs_interface_->StartSearchQuery(
      search_query_.BindNewPipeAndPassReceiver(), CreateMyDriveQuery());
  search_query_->GetNextPage(
      base::BindOnce(&DriveFsPinManager::OnSearchResultForSizeCalculation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::OnSearchResultForSizeCalculation(
    const drive::FileError error,
    const absl::optional<std::vector<mojom::QueryItemPtr>> items) {
  if (error != drive::FILE_ERROR_OK || !items) {
    LOG(ERROR) << "Cannot list files for space calculation: " << error;
    return Complete(SetupError::kCannotRetrieveSearchResults);
  }

  if (items->empty()) {
    VLOG(1) << "Calculated required space in "
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
    const base::FilePath& path = item->path;
    DCHECK(item->metadata);
    const mojom::FileMetadata& md = *item->metadata;
    VLOG(1) << "Considering " << Quote(path) << ", metadata: " << Quote(md);

    if (!CanPinItem(md, item->path)) {
      continue;
    }

    DCHECK_GE(md.size, 0) << " for " << Quote(path);
    state_.progress.required_disk_space += GetSize(md);
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

  NotifyProgress();
  DCHECK(search_query_);
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
      base::BindOnce(&DriveFsPinManager::CheckUnstartedFiles,
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
  for (const mojom::QueryItemPtr& item : *items) {
    DCHECK(item);
    const base::FilePath& path = item->path;
    DCHECK(item->metadata);
    const mojom::FileMetadata& md = *item->metadata;
    VLOG(3) << "Considering " << Quote(path) << ", metadata: " << Quote(md);

    if (!CanPinItem(md, item->path)) {
      continue;
    }

    VLOG(2) << "Pinning " << Quote(path) << "...";
    tracked_files_.Add(path.value(), GetSize(md));
    drivefs_interface_->SetPinned(
        path, true,
        base::BindOnce(&DriveFsPinManager::OnFilePinned,
                       weak_ptr_factory_.GetWeakPtr(), path.value()));
  }

  MaybeContinueSearch();
}

void DriveFsPinManager::OnFilePinned(const std::string& path,
                                     const drive::FileError status) {
  if (status != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot pin " << Quote(path) << ": " << status;
    if (tracked_files_.Remove(path, 0)) {
      state_.progress.error_count++;
      ReportTotalBytesTransferred();
    }
    return;
  }

  VLOG(1) << "Pinned " << Quote(path);
}

void DriveFsPinManager::OnSyncingStatusUpdate(
    const mojom::SyncingStatus& status) {
  if (!enabled_ || !state_.SetupInProgress()) {
    VLOG(2) << "Ignored syncing status update";
    return;
  }

  for (const mojom::ItemEventPtr& event : status.item_events) {
    DCHECK(event);
    using State = mojom::ItemEvent::State;
    switch (event->state) {
      case State::kQueued:
        if (tracked_files_.MarkInProgress(event->path)) {
          VLOG(2) << "Queued " << Quote(event->path) << ": " << Quote(*event);
          VLOG_IF(1, !VLOG_IS_ON(2)) << "Queued " << Quote(event->path);
        } else {
          VLOG(3) << "Duplicated event: " << Quote(*event);
        }
        continue;

      case State::kCompleted:
        if (tracked_files_.Remove(event->path)) {
          VLOG(2) << "Synced " << Quote(event->path) << ": " << Quote(*event);
          VLOG_IF(1, !VLOG_IS_ON(2)) << "Synced " << Quote(event->path);
          ReportTotalBytesTransferred();
        } else {
          VLOG(3) << "Duplicated event: " << Quote(*event);
        }
        continue;

      case State::kFailed:
        if (tracked_files_.Remove(event->path, 0)) {
          LOG(ERROR) << "Cannot sync " << Quote(event->path) << ": "
                     << Quote(*event);
          state_.progress.error_count++;
          ReportTotalBytesTransferred();
        } else {
          VLOG(3) << "Duplicated event: " << Quote(*event);
        }
        continue;

      case State::kInProgress:
        if (tracked_files_.Update(event->path, event->bytes_transferred,
                                  event->bytes_to_transfer)) {
          VLOG(2) << "Syncing " << Quote(event->path) << ": " << Quote(*event);
          ReportTotalBytesTransferred();
        } else {
          VLOG(3) << "Duplicated event: " << Quote(*event);
        }
        continue;
    }

    LOG(ERROR) << "Unexpected event type: " << Quote(*event);
  }

  MaybeContinueSearch();
}

void DriveFsPinManager::ReportTotalBytesTransferred() {
  const int64_t total_bytes_transferred =
      tracked_files_.GetTotalBytesTransferred();
  LOG_IF(ERROR, state_.progress.pinned_disk_space > total_bytes_transferred)
      << "Overall progress went backwards from "
      << HumanReadableSize(state_.progress.pinned_disk_space) << " to "
      << HumanReadableSize(total_bytes_transferred);
  state_.progress.pinned_disk_space = total_bytes_transferred;
  NotifyProgress();
}

void DriveFsPinManager::MaybeContinueSearch() {
  if (const size_t n = tracked_files_.GetCount()) {
    VLOG(1) << "Syncing " << n << " files...";
    return;
  }

  VLOG(1) << "Getting next batch of items...";
  DCHECK(search_query_);
  search_query_->GetNextPage(
      base::BindOnce(&DriveFsPinManager::OnSearchResultsForPinning,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::OnUnmounted() {}

void DriveFsPinManager::OnFilesChanged(
    const std::vector<mojom::FileChange>& changes) {
  for (const mojom::FileChange& change : changes) {
    VLOG(2) << "Got FileChange event: " << Quote(change);
  }
}

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

void DriveFsPinManager::CheckUnstartedFiles() {
  if (const std::vector<std::string> paths = tracked_files_.GetUnstartedPaths();
      !paths.empty()) {
    VLOG(1) << "Checking " << paths.size() << " unstarted files...";
    for (const std::string& path : paths) {
      VLOG(2) << "Checking unstarted " << Quote(path) << "...";
      drivefs_interface_->GetMetadata(
          base::FilePath(path),
          base::BindOnce(&DriveFsPinManager::OnMetadataRetrieved,
                         weak_ptr_factory_.GetWeakPtr(), path));
    }
  }

  MaybeContinueSearch();

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DriveFsPinManager::CheckUnstartedFiles,
                     weak_ptr_factory_.GetWeakPtr()),
      kPeriodicRemovalInterval);
}

void DriveFsPinManager::OnMetadataRetrieved(
    const std::string& path,
    const drive::FileError error,
    const mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot get metadata of " << Quote(path) << ": " << error;
    if (tracked_files_.Remove(path)) {
      state_.progress.error_count++;
      ReportTotalBytesTransferred();
    }
    return;
  }

  DCHECK(metadata);
  VLOG(2) << "Got metadata for " << Quote(path) << ": " << Quote(*metadata);

  if (!metadata->pinned || metadata->available_offline) {
    VLOG_IF(1, !metadata->pinned)
        << "Stop tracking " << Quote(path) << ": Not pinned";
    VLOG_IF(1, metadata->available_offline)
        << "Stop tracking " << Quote(path) << ": Already available offline";
    if (tracked_files_.Remove(path, GetSize(*metadata))) {
      ReportTotalBytesTransferred();
    }
  }
}

}  // namespace drivefs::pinning
