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
#include "third_party/cros_system_api/constants/cryptohome.h"

namespace drivefs::pinning {
namespace {

bool InProgress(const SetupStage stage) {
  return stage > SetupStage::kNotStarted && stage < SetupStage::kSuccess;
}

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

  return out << "FileMetadata::Type("
             << static_cast<std::underlying_type_t<Type>>(q.value) << ")";
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

  return out << "ItemEvent::State("
             << static_cast<std::underlying_type_t<State>>(q.value) << ")";
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
  return out << "{" << Quote(md.type) << " "
             << DriveFsPinManager::StableId(md.stable_id)
             << ", size: " << HumanReadableSize(md.size)
             << ", pinned: " << md.pinned << ", can_pin: "
             << (md.can_pin == mojom::FileMetadata::CanPinStatus::kOk)
             << ", available_offline: " << md.available_offline
             << ", shared: " << md.shared << ", starred: " << md.starred
             << ", item_id = " << Quote(md.item_id) << "}";
}

std::ostream& operator<<(std::ostream& out, Quoter<mojom::ItemEvent> q) {
  const mojom::ItemEvent& e = q.value;
  return out << "{" << Quote(e.state) << " "
             << DriveFsPinManager::StableId(e.stable_id) << " " << Quote(e.path)
             << ", bytes_transferred: "
             << HumanReadableSize(e.bytes_transferred)
             << ", bytes_to_transfer: "
             << HumanReadableSize(e.bytes_to_transfer) << "}";
}

std::ostream& operator<<(std::ostream& out, Quoter<mojom::FileChange> q) {
  const mojom::FileChange& change = q.value;
  return out << "{" << Quote(change.type) << " "
             << DriveFsPinManager::StableId(change.stable_id) << " "
             << Quote(change.path) << "}";
}

std::ostream& operator<<(std::ostream& out, Quoter<mojom::DriveError::Type> q) {
  using Type = mojom::DriveError::Type;
  switch (q.value) {
#define PRINT(s)   \
  case Type::k##s: \
    return out << #s;
    PRINT(CantUploadStorageFull)
    PRINT(PinningFailedDiskFull)
    PRINT(CantUploadStorageFullOrganization)
    PRINT(CantUploadSharedDriveStorageFull)
#undef PRINT
  }

  return out << "DriveError::Type("
             << static_cast<std::underlying_type_t<Type>>(q.value) << ")";
}

std::ostream& operator<<(std::ostream& out, Quoter<mojom::DriveError> q) {
  const mojom::DriveError& e = q.value;
  return out << "{" << Quote(e.type) << " "
             << DriveFsPinManager::StableId(e.stable_id) << " " << Quote(e.path)
             << "}";
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
  const auto id = DriveFsPinManager::StableId(metadata.stable_id);

  if (metadata.type == Type::kDirectory) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Directory";
    return false;
  }

  // TODO (b/264596214) Drive shortcuts masquerade as empty files. Is there a
  // better way to recognize Drive shortcuts?
  if (metadata.type == Type::kFile && metadata.size == 0) {
    VLOG(2) << "Skipped " << id << " " << Quote(path)
            << ": Empty file or shortcut";
    return false;
  }

  if (metadata.pinned) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Already pinned";
    return false;
  }

  if (metadata.can_pin != mojom::FileMetadata::CanPinStatus::kOk) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Cannot be pinned";
    return false;
  }

  return true;
}

}  // namespace

std::ostream& operator<<(std::ostream& out,
                         const DriveFsPinManager::StableId id) {
  return out << "#" << static_cast<int64_t>(id);
}

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

std::ostream& operator<<(std::ostream& out, const SetupStage stage) {
  switch (stage) {
#define PRINT(s)         \
  case SetupStage::k##s: \
    return out << #s;
    PRINT(NotStarted)
    PRINT(CalculatingFreeSpace)
    PRINT(CalculatingRequiredSpace)
    PRINT(Syncing)
    PRINT(Success)
    PRINT(Disabled)
    PRINT(Stopped)
    PRINT(CannotCalculateFreeSpace)
    PRINT(CannotRetrieveSearchResults)
    PRINT(NotEnoughSpace)
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

void DriveFsPinManager::Add(const StableId id,
                            const std::string& path,
                            const int64_t expected_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Emplace an item with no progress (yet). The progress values will get
  // updated in the `OnSyncingStatusUpdate`.
  const auto [it, ok] =
      files_.try_emplace(id, Progress{.path = path, .total = expected_size});
  DCHECK_EQ(id, it->first);
  if (ok) {
    VLOG(3) << "Added " << id << " " << Quote(path) << " to the tracked files";
    VLOG_IF(1, expected_size <= 0)
        << "Tracked " << id << " " << Quote(path) << " has an expected size of "
        << HumanReadableSize(expected_size);
  } else {
    LOG(ERROR) << "Cannot add " << id << " " << Quote(path)
               << " to the tracked files: Conflicting entry " << it->second;
  }
}

bool DriveFsPinManager::Remove(const StableId id,
                               const std::string& path,
                               int64_t bytes_transferred) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Files::node_type node = files_.extract(id);
  if (!node) {
    return false;
  }

  DCHECK_EQ(node.key(), id);
  const Progress& progress = node.mapped();

  LOG_IF(ERROR, path != progress.path)
      << "Changed path of " << id << " " << Quote(progress.path) << " to "
      << Quote(path);

  if (bytes_transferred < 0) {
    bytes_transferred = progress.total;
  } else {
    LOG_IF(ERROR, progress.total != bytes_transferred)
        << "Expected final progress " << HumanReadableSize(progress.total)
        << " instead of " << HumanReadableSize(bytes_transferred) << " for "
        << id << " " << Quote(path);
  }

  LOG_IF(ERROR, progress.transferred > bytes_transferred)
      << "Progress went backwards from "
      << HumanReadableSize(progress.transferred) << " to "
      << HumanReadableSize(bytes_transferred) << " for " << id << " "
      << Quote(path);
  progress_.transferred_bytes += bytes_transferred - progress.transferred;

  VLOG(3) << "Stopped tracking " << id << " " << Quote(path);
  return true;
}

bool DriveFsPinManager::Update(const StableId id,
                               const std::string& path,
                               const int64_t bytes_transferred,
                               const int64_t bytes_to_transfer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(bytes_to_transfer, 0) << " for " << id << " " << Quote(path);

  if (bytes_transferred < 0) {
    LOG(ERROR) << "Negative bytes_transferred = "
               << HumanReadableSize(bytes_transferred) << " for " << id << " "
               << Quote(path);
    return false;
  }

  Progress& progress = files_[id];

  if (path != progress.path) {
    LOG(ERROR) << "Changed path of " << id << " " << Quote(progress.path)
               << " to " << Quote(path);
    progress.path = path;
  }

  if (bytes_transferred == progress.transferred &&
      bytes_to_transfer == progress.total && progress.in_progress) {
    return false;
  }

  progress.in_progress = true;

  LOG_IF(ERROR, bytes_transferred < progress.transferred)
      << "Progress went backwards from "
      << HumanReadableSize(progress.transferred) << " to "
      << HumanReadableSize(bytes_transferred) << " for " << id << " "
      << Quote(path);

  LOG_IF(ERROR, bytes_to_transfer != progress.total)
      << "Changed expected size of " << id << " " << Quote(path) << " from "
      << HumanReadableSize(progress.total) << " to "
      << HumanReadableSize(bytes_to_transfer);

  progress_.transferred_bytes += bytes_transferred - progress.transferred;
  progress.transferred = bytes_transferred;
  progress.total = bytes_to_transfer;
  return true;
}

bool DriveFsPinManager::MarkInProgress(const StableId id,
                                       const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Progress& progress = files_[id];

  if (path != progress.path) {
    LOG(ERROR) << "Changed path of " << id << " " << Quote(progress.path)
               << " to " << Quote(path);
    progress.path = path;
  }

  if (progress.in_progress) {
    return false;
  }

  LOG_IF(ERROR, progress.transferred != 0)
      << "Queued " << id << " " << Quote(path) << " already has transferred "
      << HumanReadableSize(progress.transferred);

  progress.in_progress = true;
  return true;
}

DriveFsPinManager::DriveFsPinManager(
    bool enabled,
    const base::FilePath& profile_path,
    mojom::DriveFs* drivefs_interface,
    std::unique_ptr<FreeDiskSpaceDelegate> free_space)
    : enabled_(enabled),
      free_space_(free_space ? std::move(free_space)
                             : std::make_unique<FreeDiskSpaceImpl>()),
      profile_path_(profile_path),  // The GCache directory is located in the
                                    // users profile path.
      drivefs_interface_(drivefs_interface) {}

DriveFsPinManager::~DriveFsPinManager() = default;

// TODO(b/259454320): Pass through a `base::RepeatingCallback` here to enable
// the callsite to receive progress updates.
void DriveFsPinManager::Start(CompletionCallback complete_callback,
                              const bool should_pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(complete_callback);

  if (!enabled_) {
    LOG(ERROR) << "The pin manager is not enabled";
    std::move(complete_callback).Run(SetupStage::kDisabled);
    return;
  }

  VLOG(1) << "Calculating free space...";
  should_pin_ = should_pin;
  timer_ = base::ElapsedTimer();
  complete_callback_ = std::move(complete_callback);
  files_.clear();
  progress_ = {.stage = SetupStage::kCalculatingFreeSpace};
  NotifyProgress();

  free_space_->AmountOfFreeDiskSpace(
      profile_path_.AppendASCII(kGCacheFolderName),
      base::BindOnce(&DriveFsPinManager::OnFreeDiskSpaceRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::Stop() {
  Complete(SetupStage::kStopped);
}

void DriveFsPinManager::OnFreeDiskSpaceRetrieved(const int64_t free_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (free_space < 0) {
    LOG(ERROR) << "Cannot calculate free space";
    return Complete(SetupStage::kCannotCalculateFreeSpace);
  }

  VLOG(2) << "Free space: " << HumanReadableSize(free_space);
  progress_.stage = SetupStage::kCalculatingRequiredSpace;
  progress_.free_space = free_space;
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != drive::FILE_ERROR_OK || !items) {
    LOG(ERROR) << "Cannot list files for space calculation: " << error;
    return Complete(SetupStage::kCannotRetrieveSearchResults);
  }

  if (items->empty()) {
    VLOG(1) << "Calculated required space in "
            << timer_.Elapsed().InMilliseconds() << " ms";
    VLOG(1) << "Free space: " << HumanReadableSize(progress_.free_space);
    VLOG(1) << "Required space: "
            << HumanReadableSize(progress_.required_space);
    VLOG(1) << "To download: " << HumanReadableSize(progress_.total_bytes);
    return StartPinning();
  }

  VLOG(2) << "Iterating over " << items->size()
          << " items for space calculation";
  for (const mojom::QueryItemPtr& item : *items) {
    DCHECK(item);
    const base::FilePath& path = item->path;
    DCHECK(item->metadata);
    const mojom::FileMetadata& md = *item->metadata;
    const StableId id = StableId(md.stable_id);
    VLOG(3) << "Considering " << id << " " << Quote(path) << " " << Quote(md);

    if (!CanPinItem(md, item->path)) {
      continue;
    }

    DCHECK_GE(md.size, 0) << " for " << id << " " << Quote(path);
    const int64_t size = GetSize(md);
    progress_.total_bytes += size;

    // Assumes that the underlying filesystem works with 4-KB blocks.
    const int64_t block_size = 4096;
    const int64_t block_count = (size + (block_size - 1)) / block_size;
    progress_.required_space += block_count * block_size;
  }

  // The free space should not go below this limit.
  const int64_t storage_floor = cryptohome::kMinFreeSpaceInBytes;
  const int64_t required_with_margin = progress_.required_space + storage_floor;

  if (progress_.free_space < required_with_margin) {
    LOG(ERROR) << "Not enough space: Required: "
               << HumanReadableSize(progress_.required_space)
               << ", Required plus margin: "
               << HumanReadableSize(required_with_margin)
               << ", Free: " << HumanReadableSize(progress_.free_space);
    return Complete(SetupStage::kNotEnoughSpace);
  }

  NotifyProgress();
  DCHECK(search_query_);
  search_query_->GetNextPage(
      base::BindOnce(&DriveFsPinManager::OnSearchResultForSizeCalculation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::Complete(const SetupStage stage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!InProgress(stage));
  progress_.stage = stage;
  if (stage == SetupStage::kSuccess) {
    VLOG(1) << "Finished with success";
  } else {
    LOG(ERROR) << "Finished with error: " << stage;
  }

  LOG_IF(ERROR, progress_.errors > 0)
      << "Failed to pin " << progress_.errors << " files";
  VLOG(1) << "Pinned " << progress_.pinned_files << " files in "
          << timer_.Elapsed().InMilliseconds() << " ms";
  VLOG(2) << "Useful events: " << progress_.useful_events;
  VLOG(2) << "Duplicated events: " << progress_.duplicated_events;

  NotifyProgress();
  weak_ptr_factory_.InvalidateWeakPtrs();
  search_query_.reset();
  if (complete_callback_) {
    std::move(complete_callback_).Run(stage);
  }
}

void DriveFsPinManager::StartPinning() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!should_pin_) {
    return Complete(SetupStage::kSuccess);
  }

  // Restart the search query.
  search_query_.reset();

  progress_.stage = SetupStage::kSyncing;
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != drive::FILE_ERROR_OK || !items) {
    LOG(ERROR) << "Cannot list files to pin: " << error;
    return Complete(SetupStage::kCannotRetrieveSearchResults);
  }

  if (items->empty()) {
    return Complete(SetupStage::kSuccess);
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
    const StableId id = StableId(md.stable_id);
    VLOG(3) << "Considering " << id << " " << Quote(path) << " " << Quote(md);

    if (!CanPinItem(md, item->path)) {
      continue;
    }

    VLOG(2) << "Pinning " << id << " " << Quote(path);
    Add(id, path.value(), GetSize(md));

    // TODO(b/264932437) Use stable ID instead of path.
    drivefs_interface_->SetPinned(
        path, true,
        base::BindOnce(&DriveFsPinManager::OnFilePinned,
                       weak_ptr_factory_.GetWeakPtr(), id, path.value()));
  }

  MaybeContinueSearch();
}

void DriveFsPinManager::OnFilePinned(const StableId id,
                                     const std::string& path,
                                     const drive::FileError status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot pin " << id << " " << Quote(path) << ": " << status;
    if (Remove(id, path, 0)) {
      progress_.errors++;
      NotifyProgress();
    }
    return;
  }

  VLOG(1) << "Pinned " << id << " " << Quote(path);
}

void DriveFsPinManager::OnSyncingStatusUpdate(
    const mojom::SyncingStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!enabled_ || !InProgress(progress_.stage)) {
    VLOG(2) << "Ignored syncing status update";
    return;
  }

  for (const mojom::ItemEventPtr& event : status.item_events) {
    DCHECK(event);
    const StableId id = StableId(event->stable_id);
    using State = mojom::ItemEvent::State;
    switch (event->state) {
      case State::kQueued:
        if (MarkInProgress(id, event->path)) {
          VLOG(2) << "Queued " << id << " " << Quote(event->path) << ": "
                  << Quote(*event);
          VLOG_IF(1, !VLOG_IS_ON(2))
              << "Queued " << id << " " << Quote(event->path);
          progress_.useful_events++;
        } else {
          VLOG(3) << "Duplicated event: " << Quote(*event);
          progress_.duplicated_events++;
        }
        continue;

      case State::kCompleted:
        if (Remove(id, event->path)) {
          VLOG(2) << "Synced " << id << " " << Quote(event->path) << ": "
                  << Quote(*event);
          VLOG_IF(1, !VLOG_IS_ON(2))
              << "Synced " << id << " " << Quote(event->path);
          progress_.useful_events++;
          progress_.pinned_files++;
          NotifyProgress();
        } else {
          VLOG(3) << "Duplicated event: " << Quote(*event);
          progress_.duplicated_events++;
        }
        continue;

      case State::kFailed:
        if (Remove(id, event->path, 0)) {
          LOG(ERROR) << "Cannot sync " << id << " " << Quote(event->path)
                     << ": " << Quote(*event);
          progress_.errors++;
          progress_.useful_events++;
          NotifyProgress();
        } else {
          VLOG(3) << "Duplicated event: " << Quote(*event);
          progress_.duplicated_events++;
        }
        continue;

      case State::kInProgress:
        if (Update(id, event->path, event->bytes_transferred,
                   event->bytes_to_transfer)) {
          VLOG(2) << "Syncing " << id << " " << Quote(event->path) << " at "
                  << (100 * event->bytes_transferred / event->bytes_to_transfer)
                  << "%: " << Quote(*event);
          progress_.useful_events++;
          NotifyProgress();
        } else {
          VLOG(3) << "Duplicated event: " << Quote(*event);
          progress_.duplicated_events++;
        }
        continue;
    }

    LOG(ERROR) << "Unexpected event type: " << Quote(*event);
  }

  MaybeContinueSearch();
}

void DriveFsPinManager::MaybeContinueSearch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!files_.empty()) {
    VLOG(1) << "Syncing " << files_.size() << " files";
    return;
  }

  VLOG(1) << "Getting next batch of items";
  DCHECK(search_query_);
  search_query_->GetNextPage(
      base::BindOnce(&DriveFsPinManager::OnSearchResultsForPinning,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::OnUnmounted() {}

void DriveFsPinManager::OnFilesChanged(
    const std::vector<mojom::FileChange>& changes) {
  for (const mojom::FileChange& change : changes) {
    VLOG(2) << "Got FileChange " << Quote(change);
  }
}

void DriveFsPinManager::OnError(const mojom::DriveError& error) {
  LOG(ERROR) << "Got DriveError " << Quote(error);
}

void DriveFsPinManager::NotifyProgress() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (observers_.empty()) {
    return;
  }

  VLOG(3) << "Notifying observers";
  for (DriveFsBulkPinObserver& observer : observers_) {
    observer.OnSetupProgress(progress_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& [id, progress] : files_) {
    if (!progress.in_progress) {
      const std::string& path = progress.path;
      VLOG(2) << "Checking unstarted " << id << " " << Quote(path);
      // TODO(b/264932920) Use stable ID instead of path.
      drivefs_interface_->GetMetadata(
          base::FilePath(path),
          base::BindOnce(&DriveFsPinManager::OnMetadataRetrieved,
                         weak_ptr_factory_.GetWeakPtr(), id, path));
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
    const StableId id,
    const std::string& path,
    const drive::FileError error,
    const mojom::FileMetadataPtr metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot get metadata of " << id << " " << Quote(path) << ": "
               << error;
    if (!Remove(id, path)) {
      LOG(ERROR) << "Not tracked: " << id << " " << Quote(path);
      return;
    }

    VLOG(1) << "Stopped tracking " << id << " " << Quote(path);
    progress_.errors++;
    NotifyProgress();
    return;
  }

  DCHECK(metadata);
  DCHECK_EQ(id, StableId(metadata->stable_id));
  VLOG(2) << "Got metadata for " << id << " " << Quote(path) << ": "
          << Quote(*metadata);

  if (metadata->pinned && !metadata->available_offline) {
    return;
  }

  if (!Remove(id, path, GetSize(*metadata))) {
    LOG(ERROR) << "Not tracked: " << id << " " << Quote(path);
  }

  VLOG_IF(1, !metadata->pinned)
      << "Stopped tracking " << id << " " << Quote(path) << ": Not pinned";
  VLOG_IF(1, metadata->available_offline)
      << "Stopped tracking " << id << " " << Quote(path)
      << ": Available offline";

  progress_.pinned_files++;
  NotifyProgress();
}

}  // namespace drivefs::pinning
