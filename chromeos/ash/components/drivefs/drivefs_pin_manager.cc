// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"

#include <locale>
#include <type_traits>

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

bool InProgress(const Stage stage) {
  return stage > Stage::kNotStarted && stage < Stage::kSuccess;
}

int Percentage(const int64_t a, const int64_t b) {
  DCHECK_GE(a, 0);
  DCHECK_LE(a, b);
  return b ? 100 * a / b : 100;
}

mojom::QueryParametersPtr CreateMyDriveQuery() {
  mojom::QueryParametersPtr query = mojom::QueryParameters::New();
  query->page_size = 1000;
  return query;
}

// Calls `base::SysInfo::AmountOfFreeDiskSpace` on a blocking thread.
void GetFreeSpace(const base::FilePath& path,
                  PinManager::SpaceResult callback) {
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

std::ostream& operator<<(std::ostream& out,
                         Quoter<mojom::ShortcutDetails::LookupStatus> q) {
  using LookupStatus = mojom::ShortcutDetails::LookupStatus;
  switch (q.value) {
#define PRINT(s)           \
  case LookupStatus::k##s: \
    return out << #s;
    PRINT(Ok)
    PRINT(NotFound)
    PRINT(PermissionDenied)
    PRINT(Unknown)
#undef PRINT
  }

  return out << "ShortcutDetails::LookupStatus("
             << static_cast<std::underlying_type_t<LookupStatus>>(q.value)
             << ")";
}

std::ostream& operator<<(std::ostream& out, Quoter<mojom::ShortcutDetails> q) {
  return out << "{id: " << PinManager::Id(q.value.target_stable_id)
             << ", status: " << Quote(q.value.target_lookup_status) << "}";
}

std::ostream& operator<<(std::ostream& out, Quoter<mojom::FileMetadata> q) {
  const mojom::FileMetadata& md = q.value;
  out << "{" << Quote(md.type) << " " << PinManager::Id(md.stable_id)
      << ", size: " << HumanReadableSize(md.size) << ", pinned: " << md.pinned
      << ", can_pin: " << (md.can_pin == mojom::FileMetadata::CanPinStatus::kOk)
      << ", available_offline: " << md.available_offline;
  if (md.shortcut_details) {
    out << ", shortcut_details: " << Quote(*md.shortcut_details);
  }
  return out << "}";
}

std::ostream& operator<<(std::ostream& out, Quoter<mojom::ItemEvent> q) {
  const mojom::ItemEvent& e = q.value;
  return out << "{" << Quote(e.state) << " " << PinManager::Id(e.stable_id)
             << " " << Quote(e.path) << ", bytes_transferred: "
             << HumanReadableSize(e.bytes_transferred)
             << ", bytes_to_transfer: "
             << HumanReadableSize(e.bytes_to_transfer) << "}";
}

std::ostream& operator<<(std::ostream& out, Quoter<mojom::FileChange> q) {
  const mojom::FileChange& change = q.value;
  return out << "{" << Quote(change.type) << " "
             << PinManager::Id(change.stable_id) << " " << Quote(change.path)
             << "}";
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
  return out << "{" << Quote(e.type) << " " << PinManager::Id(e.stable_id)
             << " " << Quote(e.path) << "}";
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

}  // namespace

std::ostream& operator<<(std::ostream& out, const PinManager::Id id) {
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

std::ostream& operator<<(std::ostream& out, const Stage stage) {
  switch (stage) {
#define PRINT(s)    \
  case Stage::k##s: \
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

  return out << "Stage(" << static_cast<std::underlying_type_t<Stage>>(stage)
             << ")";
}

std::ostream& PinManager::File::PrintTo(std::ostream& out) const {
  return out << "{path: " << Quote(path)
             << ", transferred: " << HumanReadableSize(transferred)
             << ", total: " << HumanReadableSize(total)
             << ", pinned: " << pinned << ", in_progress: " << in_progress
             << "}";
}

Progress::Progress() = default;
Progress::Progress(const Progress&) = default;
Progress& Progress::operator=(const Progress&) = default;

bool Progress::HasEnoughFreeSpace() const {
  // The free space should not go below this limit.
  const int64_t margin = cryptohome::kMinFreeSpaceInBytes;
  const bool enough = required_space + margin <= free_space;
  LOG_IF(ERROR, !enough) << "Not enough space: Free space "
                         << HumanReadableSize(free_space)
                         << " is less than required space "
                         << HumanReadableSize(required_space) << " + margin "
                         << HumanReadableSize(margin);
  return enough;
}

// TODO(b/261530666): This was chosen arbitrarily, this should be experimented
// with and potentially made dynamic depending on feedback of the in progress
// queue.
constexpr base::TimeDelta kStalledFileInterval = base::Seconds(10);
constexpr base::TimeDelta kFreeSpaceInterval = base::Seconds(60);

bool PinManager::CanPin(const mojom::FileMetadata& md, const Path& path) {
  using Type = mojom::FileMetadata::Type;
  const auto id = PinManager::Id(md.stable_id);

  if (md.shortcut_details) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Shortcut to "
            << Id(md.shortcut_details->target_stable_id);
    return false;
  }

  if (md.type == Type::kDirectory) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Directory";
    return false;
  }

  if (md.can_pin != mojom::FileMetadata::CanPinStatus::kOk) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Cannot be pinned";
    return false;
  }

  if (md.pinned && md.available_offline) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Already pinned";
    return false;
  }

  // TODO(b/266037569): Setting root in the query made to DriveFS is currently
  // unsupported.
  if (!Path("/root").IsParent(path)) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Not in my drive";
    return false;
  }

  return true;
}

bool PinManager::Add(const mojom::FileMetadata& md, const Path& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Id id = Id(md.stable_id);
  VLOG(3) << "Considering " << id << " " << Quote(path) << " " << Quote(md);

  if (!CanPin(md, path)) {
    progress_.skipped_files++;
    return false;
  }

  const int64_t size = GetSize(md);
  DCHECK_GE(size, 0) << " for " << id << " " << Quote(path);

  const auto [it, ok] =
      files_to_track_.try_emplace(id, File{.path = path,
                                           .total = size,
                                           .pinned = md.pinned,
                                           .in_progress = true});
  DCHECK_EQ(id, it->first);
  File& file = it->second;
  if (!ok) {
    LOG_IF(ERROR, !ok) << "Cannot add " << id << " " << Quote(path)
                       << " with size " << HumanReadableSize(size)
                       << " to the files to track: Conflicting entry " << file;
    return false;
  }

  VLOG(3) << "Added " << id << " " << Quote(path) << " with size "
          << HumanReadableSize(size) << " to the files to track";

  progress_.files_to_pin++;
  progress_.bytes_to_pin += size;

  if (md.pinned) {
    progress_.syncing_files++;
    DCHECK_EQ(progress_.syncing_files, CountPinnedFiles());
  } else {
    files_to_pin_.insert(id);
    DCHECK_LE(files_to_pin_.size(),
              static_cast<size_t>(progress_.files_to_pin));
  }

  if (md.available_offline) {
    file.transferred = size;
    progress_.pinned_bytes += size;
  } else {
    DCHECK_EQ(file.transferred, 0);
    progress_.required_space += RoundToBlockSize(size);
  }

  VLOG_IF(1, md.pinned && !md.available_offline)
      << "Already pinned but not available offline yet: " << id << " "
      << Quote(path);
  VLOG_IF(1, !md.pinned && md.available_offline)
      << "Not pinned yet but already available offline: " << id << " "
      << Quote(path);

  return true;
}

bool PinManager::Remove(const Id id,
                        const Path& path,
                        const int64_t transferred) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Files::iterator it = files_to_track_.find(id);
  if (it == files_to_track_.end()) {
    VLOG(3) << "Not tracked: " << id << " " << path;
    return false;
  }

  DCHECK_EQ(it->first, id);
  Remove(it, path, transferred);
  return true;
}

void PinManager::Remove(const Files::iterator it,
                        const Path& path,
                        const int64_t transferred) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(it != files_to_track_.end());
  const Id id = it->first;

  {
    const File& file = it->second;

    if (transferred < 0) {
      Update(*it, path, file.total, -1);
    } else {
      Update(*it, path, transferred, transferred);
    }

    if (file.pinned) {
      progress_.syncing_files--;
      DCHECK_EQ(files_to_pin_.count(id), 0u);
    } else {
      const size_t erased = files_to_pin_.erase(id);
      DCHECK_EQ(erased, 1u);
    }
  }

  files_to_track_.erase(it);
  DCHECK_EQ(progress_.syncing_files, CountPinnedFiles());
  VLOG(3) << "Stopped tracking " << id << " " << Quote(path);
}

bool PinManager::Update(const Id id,
                        const Path& path,
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

bool PinManager::Update(Files::value_type& entry,
                        const Path& path,
                        const int64_t transferred,
                        const int64_t total) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Id id = entry.first;
  File& file = entry.second;
  bool modified = false;

  if (path != file.path) {
    VLOG(1) << "Changed path of " << id << " from " << Quote(file.path)
            << " to " << Quote(path);
    file.path = path;
    modified = true;
  }

  if (transferred != file.transferred && transferred >= 0) {
    LOG_IF(ERROR, transferred < file.transferred)
        << "Progress went backwards from "
        << HumanReadableSize(file.transferred) << " to "
        << HumanReadableSize(transferred) << " for " << id << " "
        << Quote(path);
    progress_.pinned_bytes += transferred - file.transferred;
    progress_.required_space -=
        RoundToBlockSize(transferred) - RoundToBlockSize(file.transferred);
    file.transferred = transferred;
    modified = true;
  }

  if (total != file.total && total >= 0) {
    LOG(ERROR) << "Changed expected size of " << id << " " << Quote(path)
               << " from " << HumanReadableSize(file.total) << " to "
               << HumanReadableSize(total);
    progress_.bytes_to_pin += total - file.total;
    progress_.required_space +=
        RoundToBlockSize(total) - RoundToBlockSize(file.total);
    file.total = total;
    modified = true;
  }

  if (modified) {
    file.in_progress = true;
  }

  return modified;
}

PinManager::PinManager(Path profile_path, mojom::DriveFs* const drivefs)
    : profile_path_(std::move(profile_path)),
      drivefs_(drivefs),
      space_getter_(base::BindRepeating(&GetFreeSpace)) {
  DCHECK(drivefs_);
}

PinManager::~PinManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!InProgress(progress_.stage)) << "Pin manager is " << progress_.stage;
  for (Observer& observer : observers_) {
    observer.OnDrop();
  }
  observers_.Clear();
}

void PinManager::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!InProgress(progress_.stage)) << "Pin manager is " << progress_.stage;

  progress_ = {};
  files_to_pin_.clear();
  files_to_track_.clear();
  DCHECK_EQ(progress_.syncing_files, 0);

  VLOG(2) << "Getting free space...";
  timer_ = base::ElapsedTimer();
  progress_.stage = Stage::kGettingFreeSpace;
  NotifyProgress();

  space_getter_.Run(
      profile_path_.AppendASCII("GCache"),
      base::BindOnce(&PinManager::OnFreeSpaceRetrieved1, GetWeakPtr()));
}

void PinManager::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (InProgress(progress_.stage)) {
    VLOG(1) << "Stopping";
    Complete(Stage::kStopped);
  }
}

void PinManager::Enable(bool enabled) {
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

void PinManager::OnFreeSpaceRetrieved1(const int64_t free_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (free_space < 0) {
    LOG(ERROR) << "Cannot get free space";
    return Complete(Stage::kCannotGetFreeSpace);
  }

  progress_.free_space = free_space;
  VLOG(1) << "Free space: " << HumanReadableSize(free_space);

  VLOG(1) << "Listing files...";
  timer_ = base::ElapsedTimer();
  progress_.stage = Stage::kListingFiles;
  NotifyProgress();

  drivefs_->StartSearchQuery(search_query_.BindNewPipeAndPassReceiver(),
                             CreateMyDriveQuery());
  search_query_->GetNextPage(base::BindOnce(
      &PinManager::OnSearchResultForSizeCalculation, GetWeakPtr()));
}

void PinManager::CheckFreeSpace() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(2) << "Getting free space...";
  space_getter_.Run(
      profile_path_.AppendASCII("GCache"),
      base::BindOnce(&PinManager::OnFreeSpaceRetrieved2, GetWeakPtr()));
}

void PinManager::OnFreeSpaceRetrieved2(const int64_t free_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (free_space < 0) {
    LOG(ERROR) << "Cannot get free space";
    return Complete(Stage::kCannotGetFreeSpace);
  }

  progress_.free_space = free_space;
  VLOG(1) << "Free space: " << HumanReadableSize(progress_.free_space);
  NotifyProgress();

  if (!progress_.HasEnoughFreeSpace()) {
    return Complete(Stage::kNotEnoughSpace);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&PinManager::CheckFreeSpace, GetWeakPtr()),
      kFreeSpaceInterval);
}

void PinManager::OnSearchResultForSizeCalculation(
    const drive::FileError error,
    const absl::optional<std::vector<mojom::QueryItemPtr>> items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != drive::FILE_ERROR_OK || !items) {
    LOG(ERROR) << "Cannot list files: " << error;
    return Complete(Stage::kCannotListFiles);
  }

  if (items->empty()) {
    search_query_.reset();
    return StartPinning();
  }

  VLOG(2) << "Iterating over " << items->size()
          << " items for space calculation";
  for (const mojom::QueryItemPtr& item : *items) {
    DCHECK(item);
    DCHECK(item->metadata);
    Add(*item->metadata, item->path);
  }

  VLOG(1) << "Skipped " << progress_.skipped_files << " files, Tracking "
          << files_to_track_.size() << " files";
  NotifyProgress();
  DCHECK(search_query_);
  search_query_->GetNextPage(base::BindOnce(
      &PinManager::OnSearchResultForSizeCalculation, GetWeakPtr()));
}

void PinManager::Complete(const Stage stage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!InProgress(stage));

  progress_.stage = stage;
  switch (stage) {
    case Stage::kSuccess:
      VLOG(1) << "Finished with success";
      break;

    case Stage::kStopped:
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
  progress_.syncing_files = 0;

  if (completion_callback_) {
    std::move(completion_callback_).Run(stage);
  }
}

void PinManager::StartPinning() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << "Listed files in " << timer_.Elapsed().InMilliseconds() << " ms";
  VLOG(1) << "Free space: " << HumanReadableSize(progress_.free_space);
  VLOG(1) << "Required space: " << HumanReadableSize(progress_.required_space);
  VLOG(1) << "Skipped: " << progress_.skipped_files << " files";
  VLOG(1) << "To pin: " << files_to_pin_.size() << " files, "
          << HumanReadableSize(progress_.bytes_to_pin);
  VLOG(1) << "To track: " << files_to_track_.size() << " files";

  if (!progress_.HasEnoughFreeSpace()) {
    return Complete(Stage::kNotEnoughSpace);
  }

  if (!should_pin_) {
    VLOG(1) << "Should not pin files";
    return Complete(Stage::kSuccess);
  }

  timer_ = base::ElapsedTimer();
  progress_.stage = Stage::kSyncing;
  NotifyProgress();

  if (should_check_stalled_files_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&PinManager::CheckStalledFiles, GetWeakPtr()),
        kStalledFileInterval);
  }

  CheckFreeSpace();

  PinSomeFiles();
  NotifyProgress();
}

void PinManager::PinSomeFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (progress_.stage != Stage::kSyncing) {
    return;
  }

  while (progress_.syncing_files < 50 && !files_to_pin_.empty()) {
    const Id id = files_to_pin_.extract(files_to_pin_.begin()).value();

    const Files::iterator it = files_to_track_.find(id);
    if (it == files_to_track_.end()) {
      VLOG(2) << "Not tracked: " << id;
      continue;
    }

    DCHECK_EQ(it->first, id);
    File& file = it->second;
    const Path& path = file.path;

    if (file.pinned) {
      VLOG(2) << "Already pinned: " << id << " " << Quote(path);
      continue;
    }

    VLOG(2) << "Pinning " << id << " " << Quote(path);
    drivefs_->SetPinnedByStableId(
        static_cast<int64_t>(id), true,
        base::BindOnce(&PinManager::OnFilePinned, GetWeakPtr(), id, path));

    file.pinned = true;
    progress_.syncing_files++;
    DCHECK_EQ(progress_.syncing_files, CountPinnedFiles());
  }

  VLOG(1) << "Progress "
          << Percentage(progress_.pinned_bytes, progress_.bytes_to_pin)
          << "%: synced " << HumanReadableSize(progress_.pinned_bytes)
          << " and " << progress_.pinned_files << " files, syncing "
          << progress_.syncing_files << " files";

  if (files_to_track_.empty() && !progress_.emptied_queue) {
    progress_.emptied_queue = true;
    LOG_IF(ERROR, progress_.failed_files > 0)
        << "Failed to pin " << progress_.failed_files << " files";
    VLOG(1) << "Pinned " << progress_.pinned_files << " files and "
            << HumanReadableSize(progress_.pinned_bytes) << " in "
            << timer_.Elapsed().InMilliseconds() << " ms";
    VLOG(2) << "Useful events: " << progress_.useful_events;
    VLOG(2) << "Duplicated events: " << progress_.duplicated_events;
  }
}

void PinManager::OnFilePinned(const Id id,
                              const Path& path,
                              const drive::FileError status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot pin " << id << " " << Quote(path) << ": " << status;
    if (Remove(id, path, 0)) {
      progress_.failed_files++;
      PinSomeFiles();
      NotifyProgress();
    }
    return;
  }

  VLOG(1) << "Pinned " << id << " " << Quote(path);
}

void PinManager::OnSyncingStatusUpdate(const mojom::SyncingStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const mojom::ItemEventPtr& event : status.item_events) {
    DCHECK(event);

    if (!InProgress(progress_.stage)) {
      VLOG(2) << "Ignored " << Quote(*event);
      continue;
    }

    if (OnSyncingEvent(*event)) {
      progress_.useful_events++;
    } else {
      progress_.duplicated_events++;
      VLOG(3) << "Duplicated event: " << Quote(*event);
    }
  }

  PinSomeFiles();
  NotifyProgress();
}

bool PinManager::OnSyncingEvent(mojom::ItemEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Id id = Id(event.stable_id);
  const Path path = Path(event.path);

  using State = mojom::ItemEvent::State;
  switch (event.state) {
    case State::kQueued:
      // kQueued events come with a bytes_to_transfer field incorrectly set to
      // zero (b/266462624). So we set it to -1 to ignore it.
      event.bytes_to_transfer = -1;
      [[fallthrough]];

    case State::kInProgress:
      if (!Update(id, path, event.bytes_transferred, event.bytes_to_transfer)) {
        return false;
      }

      VLOG(3) << Quote(event.state) << " " << id << " " << Quote(path) << ": "
              << Quote(event);
      VLOG_IF(2, !VLOG_IS_ON(3))
          << Quote(event.state) << " " << id << " " << Quote(path);
      return true;

    case State::kCompleted:
      if (!Remove(id, path)) {
        return false;
      }

      VLOG(2) << "Synced " << id << " " << Quote(path) << ": " << Quote(event);
      VLOG_IF(1, !VLOG_IS_ON(2)) << "Synced " << id << " " << Quote(path);
      progress_.pinned_files++;
      return true;

    case State::kFailed:
      if (!Remove(id, path, 0)) {
        return false;
      }

      LOG(ERROR) << Quote(event.state) << " " << id << " " << Quote(path)
                 << ": " << Quote(event);
      progress_.failed_files++;
      return true;
  }

  LOG(ERROR) << "Unexpected event type: " << Quote(event);
  return false;
}

void PinManager::NotifyDelete(const Id id, const Path& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!Remove(id, path, 0)) {
    VLOG(1) << "Not tracked: " << id << " " << Quote(path);
    return;
  }

  VLOG(1) << "Stopped tracking " << id << " " << Quote(path);
  progress_.failed_files++;
  PinSomeFiles();
  NotifyProgress();
}

void PinManager::OnUnmounted() {
  LOG(ERROR) << "DriveFS got unmounted";
}

void PinManager::OnFilesChanged(const std::vector<mojom::FileChange>& changes) {
  for (const mojom::FileChange& event : changes) {
    switch (event.type) {
      using Type = mojom::FileChange::Type;

      case Type::kCreate:
        OnFileCreated(event);
        continue;

      case Type::kDelete:
        OnFileDeleted(event);
        continue;

      case Type::kModify:
        OnFileModified(event);
        continue;
    }

    VLOG(1) << "Unexpected FileChange type " << Quote(event);
  }
}

void PinManager::OnFileCreated(const mojom::FileChange& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(event.type, mojom::FileChange::Type::kCreate);

  if (!InProgress(progress_.stage)) {
    VLOG(2) << "Ignored " << Quote(event) << ": PinManager is currently "
            << progress_.stage;
    return;
  }

  const Id id = Id(event.stable_id);
  const Path& path = event.path;

  if (const Files::iterator it = files_to_track_.find(id);
      it != files_to_track_.end()) {
    DCHECK_EQ(it->first, id);
    LOG(ERROR) << "Ignored " << Quote(event) << ": Existing entry "
               << it->second;
    return;
  }

  VLOG(1) << "Got " << Quote(event);
  drivefs_->GetMetadataByStableId(
      static_cast<int64_t>(id),
      base::BindOnce(&PinManager::OnMetadataForCreatedFile, GetWeakPtr(), id,
                     path));
}

void PinManager::OnFileDeleted(const mojom::FileChange& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(event.type, mojom::FileChange::Type::kDelete);

  VLOG(1) << "Got " << Quote(event);
  const Path& path = event.path;
  const Id id = static_cast<Id>(event.stable_id);

  drivefs_->SetPinnedByStableId(
      event.stable_id, /*pinned=*/false,
      base::BindOnce(
          [](const Id id, const Path& path, const drive::FileError status) {
            if (status != drive::FILE_ERROR_OK) {
              LOG(ERROR) << "Cannot unpin " << id << " " << Quote(path) << ": "
                         << status;
            } else {
              VLOG(1) << "Unpinned " << id << " " << Quote(path);
            }
          },
          id, path));

  NotifyDelete(id, path);
}

void PinManager::OnFileModified(const mojom::FileChange& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(event.type, mojom::FileChange::Type::kModify);

  const Id id = Id(event.stable_id);
  const Path& path = event.path;

  const Files::iterator it = files_to_track_.find(id);
  if (it == files_to_track_.end()) {
    VLOG(1) << "Ignored " << Quote(event) << ": Not tracked";
    return;
  }

  VLOG(1) << "Got " << Quote(event);
  DCHECK_EQ(it->first, id);

  Update(*it, path, -1, -1);

  VLOG(2) << "Checking changed " << id << " " << Quote(path);
  drivefs_->GetMetadataByStableId(
      static_cast<int64_t>(id),
      base::BindOnce(&PinManager::OnMetadataForModifiedFile, GetWeakPtr(), id,
                     path));
}

void PinManager::OnError(const mojom::DriveError& error) {
  LOG(ERROR) << "Got DriveError " << Quote(error);
}

void PinManager::NotifyProgress() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observers_) {
    observer.OnProgress(progress_);
  }
}

void PinManager::CheckStalledFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!should_check_stalled_files_) {
    return;
  }

  for (auto& [id, file] : files_to_track_) {
    if (!file.pinned) {
      DCHECK(files_to_pin_.contains(id));
      continue;
    }

    if (file.in_progress) {
      file.in_progress = false;
      continue;
    }

    const Path& path = file.path;
    VLOG(1) << "Checking stalled " << id << " " << Quote(path);
    drivefs_->GetMetadataByStableId(
        static_cast<int64_t>(id),
        base::BindOnce(&PinManager::OnMetadataForModifiedFile, GetWeakPtr(), id,
                       path));
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&PinManager::CheckStalledFiles, GetWeakPtr()),
      kStalledFileInterval);
}

void PinManager::OnMetadataForCreatedFile(
    const Id id,
    const Path& path,
    const drive::FileError error,
    const mojom::FileMetadataPtr metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot get metadata of created " << id << " " << Quote(path)
               << ": " << error;
    return NotifyDelete(id, path);
  }

  DCHECK(metadata);
  const mojom::FileMetadata& md = *metadata;
  DCHECK_EQ(id, Id(md.stable_id));
  VLOG(2) << "Got metadata of created " << id << " " << Quote(path) << ": "
          << Quote(md);

  if (Add(md, path)) {
    PinSomeFiles();
    NotifyProgress();
  }
}

void PinManager::OnMetadataForModifiedFile(
    const Id id,
    const Path& path,
    const drive::FileError error,
    const mojom::FileMetadataPtr metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot get metadata of modified " << id << " " << Quote(path)
               << ": " << error;
    return NotifyDelete(id, path);
  }

  DCHECK(metadata);
  const mojom::FileMetadata& md = *metadata;
  DCHECK_EQ(id, Id(md.stable_id));

  const Files::iterator it = files_to_track_.find(id);
  if (it == files_to_track_.end()) {
    VLOG(1) << "Ignored metadata of untracked " << id << " " << Quote(path)
            << ": " << Quote(md);
    return;
  }

  DCHECK_EQ(it->first, id);
  const File& file = it->second;
  VLOG(2) << "Got metadata of modified " << id << " " << Quote(path) << ": "
          << Quote(md);

  if (!md.pinned) {
    if (!file.pinned) {
      VLOG(1) << "Modified " << id << " " << Quote(path)
              << " is still scheduled to be pinned";
      DCHECK(files_to_pin_.contains(id));
      return;
    }

    DCHECK(file.pinned);
    LOG(ERROR) << "Got unexpectedly unpinned: " << id << " " << Quote(path);
    Remove(it, path, 0);
    progress_.failed_files++;
    PinSomeFiles();
    NotifyProgress();
    return;
  }

  DCHECK(md.pinned);
  if (md.available_offline) {
    Remove(it, path, GetSize(md));
    VLOG(1) << "Synced " << id << " " << Quote(path);
    progress_.pinned_files++;
    PinSomeFiles();
    NotifyProgress();
  }
}

}  // namespace drivefs::pinning
