// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"

#include <locale>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
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

int Percentage(const int64_t a, const int64_t b) {
  DCHECK_GE(a, 0);
  DCHECK_LE(a, b);
  return b ? 100 * a / b : 0;
}

mojom::QueryParametersPtr CreateMyDriveQuery() {
  mojom::QueryParametersPtr query = mojom::QueryParameters::New();
  query->page_size = 1000;
  return query;
}

// Calls `base::SysInfo::AmountOfFreeDiskSpace` on a blocking thread.
void GetFreeSpace(const base::FilePath& path,
                  DriveFsPinManager::SpaceResult callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace, path),
      std::move(callback));
}

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

// Rounds the given size to the next multiple of 4-KB.
int64_t RoundToBlockSize(int64_t size) {
  const int64_t block_size = 4 << 10;  // 4 KB
  const int64_t mask = block_size - 1;
  static_assert((block_size & mask) == 0, "block_size must be a power of 2");
  return (size + mask) & ~mask;
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
    VLOG_IF(3, !metadata.available_offline)
        << "Already pinned but not available offline yet: " << id << " "
        << Quote(path);
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
    return out << "zilch";
  }

  if (i < 0) {
    out << '-';
    i = -i;
  }

  {
    static const base::NoDestructor<std::locale> with_separators(
        std::locale::classic(), new NumPunct);
    std::locale old_locale = out.imbue(*with_separators);
    out << i << " bytes";
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
    PRINT(GettingFreeSpace)
    PRINT(ListingFiles)
    PRINT(Syncing)
    PRINT(Success)
    PRINT(Stopped)
    PRINT(CannotGetFreeSpace)
    PRINT(CannotListFiles)
    PRINT(NotEnoughSpace)
#undef PRINT
  }

  return out << "SetupStage("
             << static_cast<std::underlying_type_t<SetupStage>>(stage) << ")";
}

SetupProgress::SetupProgress() = default;
SetupProgress::SetupProgress(const SetupProgress&) = default;
SetupProgress& SetupProgress::operator=(const SetupProgress&) = default;

// TODO(b/261530666): This was chosen arbitrarily, this should be experimented
// with and potentially made dynamic depending on feedback of the in progress
// queue.
constexpr base::TimeDelta kPeriodicRemovalInterval = base::Seconds(10);

bool DriveFsPinManager::Add(const StableId id,
                            const std::string& path,
                            const int64_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(size, 0) << " for " << id << " " << Quote(path);

  const auto [it, ok] =
      files_to_pin_.try_emplace(id, Progress{.path = path, .total = size});
  DCHECK_EQ(id, it->first);
  if (!ok) {
    LOG_IF(ERROR, !ok) << "Cannot add " << id << " " << Quote(path)
                       << " with size " << HumanReadableSize(size)
                       << " to the files to pin: Conflicting entry "
                       << it->second;
    return false;
  }

  VLOG(3) << "Added " << id << " " << Quote(path) << " with size "
          << HumanReadableSize(size) << " to the files to pin";
  progress_.bytes_to_pin += size;
  progress_.required_space += RoundToBlockSize(size);
  progress_.files_to_pin++;
  DCHECK_EQ(static_cast<size_t>(progress_.files_to_pin), files_to_pin_.size());
  return true;
}

bool DriveFsPinManager::Remove(const StableId id,
                               const std::string& path,
                               int64_t transferred) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Files::iterator it = files_to_track_.find(id);
  if (it == files_to_track_.end()) {
    VLOG(3) << "Not tracked: " << id << " " << path;
    return false;
  }

  if (transferred < 0) {
    Update(*it, path, it->second.total, -1);
  } else {
    Update(*it, path, transferred, transferred);
  }

  files_to_track_.erase(it);
  VLOG(3) << "Stopped tracking " << id << " " << Quote(path);
  return true;
}

bool DriveFsPinManager::Update(const StableId id,
                               const std::string& path,
                               const int64_t transferred,
                               const int64_t total) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Files::iterator it = files_to_track_.find(id);
  if (it == files_to_track_.end()) {
    VLOG(3) << "Not tracked: " << id << " " << path;
    return false;
  }

  DCHECK_EQ(it->first, id);
  return Update(*it, path, transferred, total);
}

bool DriveFsPinManager::Update(Files::value_type& entry,
                               const std::string& path,
                               int64_t transferred,
                               int64_t total) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& [id, progress] = entry;
  bool modified = false;

  if (path != progress.path) {
    VLOG(1) << "Changed path of " << id << " " << Quote(progress.path) << " to "
            << Quote(path);
    progress.path = path;
    modified = true;
  }

  if (!progress.in_progress) {
    LOG_IF(ERROR, progress.transferred > 0)
        << "Queued " << id << " " << Quote(path) << " already has transferred "
        << HumanReadableSize(progress.transferred);

    progress.in_progress = true;
    modified = true;
  }

  if (transferred != progress.transferred && transferred >= 0) {
    LOG_IF(ERROR, transferred < progress.transferred)
        << "Progress went backwards from "
        << HumanReadableSize(progress.transferred) << " to "
        << HumanReadableSize(transferred) << " for " << id << " "
        << Quote(path);
    progress_.pinned_bytes += transferred - progress.transferred;
    progress.transferred = transferred;
    modified = true;
  }

  if (total != progress.total && total >= 0) {
    LOG(ERROR) << "Changed expected size of " << id << " " << Quote(path)
               << " from " << HumanReadableSize(progress.total) << " to "
               << HumanReadableSize(total);
    progress_.bytes_to_pin += total - progress.total;
    progress_.required_space +=
        RoundToBlockSize(total) - RoundToBlockSize(progress.total);
    progress.total = total;
    modified = true;
  }

  return modified;
}

DriveFsPinManager::DriveFsPinManager(base::FilePath profile_path,
                                     mojom::DriveFs* const drivefs)
    : space_getter_(base::BindRepeating(&GetFreeSpace)),
      profile_path_(std::move(profile_path)),
      drivefs_(drivefs) {
  DCHECK(drivefs_);
}

DriveFsPinManager::~DriveFsPinManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!InProgress(progress_.stage)) << "Pin manager is " << progress_.stage;
  for (Observer& observer : observers_) {
    observer.OnDrop();
  }
  observers_.Clear();
}

void DriveFsPinManager::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!InProgress(progress_.stage)) << "Pin manager is " << progress_.stage;

  progress_ = {};
  files_to_pin_.clear();
  files_to_track_.clear();

  VLOG(1) << "Calculating free space...";
  timer_ = base::ElapsedTimer();
  progress_.stage = SetupStage::kGettingFreeSpace;
  NotifyProgress();

  space_getter_.Run(
      profile_path_.AppendASCII("GCache"),
      base::BindOnce(&DriveFsPinManager::OnFreeSpaceRetrieved, GetWeakPtr()));
}

void DriveFsPinManager::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (InProgress(progress_.stage)) {
    VLOG(1) << "Stopping";
    Complete(SetupStage::kStopped);
  }
}

void DriveFsPinManager::Enable(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (enabled == InProgress(progress_.stage)) {
    VLOG(1) << "Pin manager is already " << (enabled ? "enabled" : "disabled");
    return;
  }

  if (enabled) {
    VLOG(1) << "Starting";
    Start();
    VLOG(1) << "Started";
  } else {
    Stop();
  }
}

void DriveFsPinManager::OnFreeSpaceRetrieved(const int64_t free_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (free_space < 0) {
    LOG(ERROR) << "Cannot calculate free space";
    return Complete(SetupStage::kCannotGetFreeSpace);
  }

  progress_.free_space = free_space;
  VLOG(1) << "Calculated free space " << HumanReadableSize(free_space) << " in "
          << timer_.Elapsed().InMilliseconds() << " ms";

  VLOG(1) << "Calculating required space...";
  timer_ = base::ElapsedTimer();
  progress_.stage = SetupStage::kListingFiles;
  NotifyProgress();

  drivefs_->StartSearchQuery(search_query_.BindNewPipeAndPassReceiver(),
                             CreateMyDriveQuery());
  search_query_->GetNextPage(base::BindOnce(
      &DriveFsPinManager::OnSearchResultForSizeCalculation, GetWeakPtr()));
}

void DriveFsPinManager::OnSearchResultForSizeCalculation(
    const drive::FileError error,
    const absl::optional<std::vector<mojom::QueryItemPtr>> items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != drive::FILE_ERROR_OK || !items) {
    LOG(ERROR) << "Cannot list files: " << error;
    return Complete(SetupStage::kCannotListFiles);
  }

  if (items->empty()) {
    search_query_.reset();
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

    VLOG_IF(1, md.available_offline)
        << "Not pinned yet but already available offline: " << id << " "
        << Quote(path) << ": " << Quote(md);

    Add(id, path.value(), GetSize(md));
  }

  NotifyProgress();
  DCHECK(search_query_);
  search_query_->GetNextPage(base::BindOnce(
      &DriveFsPinManager::OnSearchResultForSizeCalculation, GetWeakPtr()));
}

void DriveFsPinManager::Complete(const SetupStage stage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!InProgress(stage));

  progress_.stage = stage;
  switch (stage) {
    case SetupStage::kSuccess:
      LOG_IF(ERROR, progress_.failed_files > 0)
          << "Failed to pin " << progress_.failed_files << " files";
      VLOG(1) << "Pinned " << progress_.pinned_files << " files and downloaded "
              << HumanReadableSize(progress_.pinned_bytes) << " in "
              << timer_.Elapsed().InMilliseconds() << " ms";
      VLOG(2) << "Useful events: " << progress_.useful_events;
      VLOG(2) << "Duplicated events: " << progress_.duplicated_events;
      VLOG(1) << "Finished with success";
      break;

    case SetupStage::kStopped:
      VLOG(1) << "Stopped";
      break;

    default:
      LOG(ERROR) << "Finished with error: " << stage;
  }

  NotifyProgress();
  weak_ptr_factory_.InvalidateWeakPtrs();
  search_query_.reset();
  files_to_pin_.clear();
  files_to_track_.clear();

  if (completion_callback_) {
    std::move(completion_callback_).Run(stage);
  }
}

void DriveFsPinManager::StartPinning() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << "Calculated required space "
          << HumanReadableSize(progress_.required_space) << " in "
          << timer_.Elapsed().InMilliseconds() << " ms";

  VLOG(1) << "Free space: " << HumanReadableSize(progress_.free_space);
  VLOG(1) << "Required space: " << HumanReadableSize(progress_.required_space);
  VLOG(1) << "To download: " << HumanReadableSize(progress_.bytes_to_pin);
  VLOG(1) << "To pin: " << files_to_pin_.size() << " files";
  VLOG(1) << "To track: " << files_to_track_.size() << " files";

  // The free space should not go below this limit.
  const int64_t margin = cryptohome::kMinFreeSpaceInBytes;
  const int64_t required_with_margin = progress_.required_space + margin;

  if (progress_.free_space < required_with_margin) {
    LOG(ERROR) << "Not enough space: Free space "
               << HumanReadableSize(progress_.free_space)
               << " is less than required space "
               << HumanReadableSize(progress_.required_space) << " + margin "
               << HumanReadableSize(margin);
    return Complete(SetupStage::kNotEnoughSpace);
  }

  if (!should_pin_) {
    VLOG(1) << "Should not pin files";
    return Complete(SetupStage::kSuccess);
  }

  if (files_to_track_.empty() && files_to_pin_.empty()) {
    VLOG(1) << "Nothing to pin or track";
    return Complete(SetupStage::kSuccess);
  }

  VLOG(1) << "Pinning and tracking "
          << (files_to_pin_.size() + files_to_track_.size()) << " files...";
  timer_ = base::ElapsedTimer();
  progress_.stage = SetupStage::kSyncing;
  NotifyProgress();

  if (should_check_stalled_files_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DriveFsPinManager::CheckStalledFiles, GetWeakPtr()),
        kPeriodicRemovalInterval);
  }

  PinSomeFiles();
}

void DriveFsPinManager::PinSomeFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (files_to_track_.empty() && files_to_pin_.empty()) {
    VLOG(1) << "Nothing left to pin or track";
    return Complete(SetupStage::kSuccess);
  }

  while (files_to_track_.size() < 50 && !files_to_pin_.empty()) {
    Files::node_type node = files_to_pin_.extract(files_to_pin_.begin());
    DCHECK(node);
    const StableId id = node.key();
    const Progress& progress = node.mapped();
    const std::string& path = progress.path;

    VLOG(2) << "Pinning " << id << " " << Quote(path);
    drivefs_->SetPinnedByStableId(
        static_cast<int64_t>(id), true,
        base::BindOnce(&DriveFsPinManager::OnFilePinned, GetWeakPtr(), id,
                       path));

    const Files::insert_return_type ir =
        files_to_track_.insert(std::move(node));
    DCHECK(ir.inserted) << " for " << id << " " << path;
  }

  VLOG(1) << "Progress "
          << Percentage(progress_.pinned_bytes, progress_.bytes_to_pin)
          << "%: synced " << HumanReadableSize(progress_.pinned_bytes)
          << " and " << progress_.pinned_files << " files, syncing "
          << files_to_track_.size() << " files";
}

void DriveFsPinManager::OnFilePinned(const StableId id,
                                     const std::string& path,
                                     const drive::FileError status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot pin " << id << " " << Quote(path) << ": " << status;
    if (Remove(id, path, 0)) {
      progress_.failed_files++;
      NotifyProgress();
      PinSomeFiles();
    }
    return;
  }

  VLOG(1) << "Pinned " << id << " " << Quote(path);
}

void DriveFsPinManager::OnSyncingStatusUpdate(
    const mojom::SyncingStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (progress_.stage != SetupStage::kSyncing) {
    VLOG(2) << "Ignored syncing status update";
    return;
  }

  for (const mojom::ItemEventPtr& event : status.item_events) {
    DCHECK(event);
    const StableId id = StableId(event->stable_id);
    using State = mojom::ItemEvent::State;
    switch (event->state) {
      case State::kQueued:
        // kQueued events come with a bytes_to_transfer field incorrectly set to
        // zero (b/266462624). So we set it to -1 to ignore it.
        event->bytes_to_transfer = -1;
        [[fallthrough]];

      case State::kInProgress:
        if (Update(id, event->path, event->bytes_transferred,
                   event->bytes_to_transfer)) {
          VLOG(3) << Quote(event->state) << " " << id << " "
                  << Quote(event->path) << ": " << Quote(*event);
          VLOG_IF(2, !VLOG_IS_ON(3))
              << Quote(event->state) << " " << id << " " << Quote(event->path);
          progress_.useful_events++;
          NotifyProgress();
        } else {
          VLOG(3) << "Duplicated event: " << Quote(*event);
          progress_.duplicated_events++;
        }
        continue;

      case State::kCompleted:
        if (Remove(id, event->path)) {
          VLOG(3) << "Synced " << id << " " << Quote(event->path) << ": "
                  << Quote(*event);
          VLOG_IF(2, !VLOG_IS_ON(3))
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
          LOG(ERROR) << Quote(event->state) << " " << id << " "
                     << Quote(event->path) << ": " << Quote(*event);
          progress_.failed_files++;
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

  PinSomeFiles();
}

void DriveFsPinManager::OnUnmounted() {
  LOG(ERROR) << "DriveFS got unmounted";
}

void DriveFsPinManager::OnFilesChanged(
    const std::vector<mojom::FileChange>& changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (progress_.stage != SetupStage::kSyncing) {
    for (const mojom::FileChange& change : changes) {
      VLOG(1) << "Ignored FileChange " << Quote(change);
    }
    return;
  }

  for (const mojom::FileChange& change : changes) {
    const StableId id = StableId(change.stable_id);
    const Files::iterator it = files_to_track_.find(id);
    if (it == files_to_track_.end()) {
      VLOG(1) << "Ignored FileChange " << Quote(change);
      continue;
    }

    VLOG(1) << "Got FileChange " << Quote(change);
    DCHECK_EQ(it->first, id);
    Progress& progress = it->second;

    const std::string& path = change.path.value();
    if (progress.path != path) {
      LOG(ERROR) << "Changed path of " << id << " " << Quote(progress.path)
                 << " to " << Quote(path);
      progress.path = path;
    }

    VLOG(2) << "Checking changed " << id << " " << Quote(path);
    drivefs_->GetMetadataByStableId(
        static_cast<int64_t>(id),
        base::BindOnce(&DriveFsPinManager::OnMetadataRetrieved, GetWeakPtr(),
                       id, path));
  }
}

void DriveFsPinManager::OnError(const mojom::DriveError& error) {
  LOG(ERROR) << "Got DriveError " << Quote(error);
}

void DriveFsPinManager::NotifyProgress() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observers_) {
    observer.OnProgress(progress_);
  }
}

void DriveFsPinManager::CheckStalledFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!should_check_stalled_files_) {
    return;
  }

  for (const auto& [id, progress] : files_to_track_) {
    if (!progress.in_progress) {
      const std::string& path = progress.path;
      VLOG(2) << "Checking unstarted " << id << " " << Quote(path);
      drivefs_->GetMetadataByStableId(
          static_cast<int64_t>(id),
          base::BindOnce(&DriveFsPinManager::OnMetadataRetrieved, GetWeakPtr(),
                         id, path));
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DriveFsPinManager::CheckStalledFiles, GetWeakPtr()),
      kPeriodicRemovalInterval);

  PinSomeFiles();
}

void DriveFsPinManager::OnMetadataRetrieved(
    const StableId id,
    const std::string& path,
    const drive::FileError error,
    const mojom::FileMetadataPtr metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (progress_.stage != SetupStage::kSyncing) {
    VLOG(1) << "Ignored metadata of " << id << " " << Quote(path);
    return;
  }

  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot get metadata of " << id << " " << Quote(path) << ": "
               << error;
    if (!Remove(id, path, 0)) {
      LOG(ERROR) << "Not tracked: " << id << " " << Quote(path);
      return;
    }

    VLOG(1) << "Stopped tracking " << id << " " << Quote(path);
    progress_.failed_files++;
    NotifyProgress();
    PinSomeFiles();
    return;
  }

  DCHECK(metadata);
  DCHECK_EQ(id, StableId(metadata->stable_id));
  VLOG(2) << "Got metadata for " << id << " " << Quote(path) << ": "
          << Quote(*metadata);

  if (!metadata->pinned) {
    if (!Remove(id, path, 0)) {
      LOG(ERROR) << "Not tracked: " << id << " " << Quote(path);
      return;
    }

    LOG(ERROR) << "Got unexpectedly unpinned: " << id << " " << Quote(path);
    progress_.failed_files++;
    NotifyProgress();
    PinSomeFiles();
    return;
  }

  DCHECK(metadata->pinned);

  if (metadata->available_offline) {
    if (!Remove(id, path, GetSize(*metadata))) {
      LOG(ERROR) << "Not tracked: " << id << " " << Quote(path);
      return;
    }

    VLOG(1) << "Synced " << id << " " << Quote(path);
    progress_.pinned_files++;
    NotifyProgress();
    PinSomeFiles();
  }
}

}  // namespace drivefs::pinning
