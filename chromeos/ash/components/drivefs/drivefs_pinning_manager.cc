// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/drivefs/drivefs_pinning_manager.h"

#include <iomanip>
#include <locale>
#include <sstream>
#include <string_view>
#include <type_traits>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/drive/file_errors.h"

namespace drivefs::pinning {
namespace {

using ash::SpacedClient;
using base::Seconds;
using base::SequencedTaskRunner;
using base::TimeDelta;
using base::UmaHistogramBoolean;
using mojom::DocsOfflineEnableStatus;
using mojom::FileMetadata;
using mojom::FileMetadataPtr;
using mojom::QueryItem;
using mojom::QueryItemPtr;
using mojom::ShortcutDetails;
using std::ostream;
using Path = PinningManager::Path;
using LookupStatus = ShortcutDetails::LookupStatus;

int Percentage(const int64_t a, const int64_t b) {
  DCHECK_GE(a, 0);
  DCHECK_LE(a, b);
  return b ? 100 * a / b : 100;
}

// Calls the spaced daemon.
void GetFreeSpace(const Path& path, PinningManager::SpaceResult callback) {
  SpacedClient* const spaced = SpacedClient::Get();
  DCHECK(spaced);
  spaced->GetFreeDiskSpace(path.value(),
                           base::BindOnce(
                               [](PinningManager::SpaceResult callback,
                                  const std::optional<int64_t> space) {
                                 std::move(callback).Run(space.value_or(-1));
                               },
                               std::move(callback)));
}

class NumPunct : public std::numpunct<char> {
 private:
  char do_thousands_sep() const override { return ','; }
  std::string do_grouping() const override { return "\3"; }
};

// Returns a locale that prints numbers with thousands separators.
std::locale NiceNumLocale() {
  static const base::NoDestructor<std::locale> with_separators(
      std::locale::classic(), new NumPunct);
  return *with_separators;
}

template <typename T>
struct Quoter {
  const raw_ref<const T> value;
};

template <typename T>
Quoter<T> Quote(const T& value) {
  return {ToRawRef(value)};
}

template <typename T>
  requires std::is_enum_v<T>
ostream& operator<<(ostream& out, Quoter<T> q) {
  // Convert enum value to string.
  const std::string s = (std::ostringstream() << (*q.value)).str();

  // Does the string start with 'k'?
  if (!s.empty() && s.front() == 'k') {
    // Skip the 'k' prefix.
    return out << std::string_view(s).substr(1);
  }

  // No 'k' prefix. Print between parentheses.
  return out << '(' << s << ')';
}

ostream& operator<<(ostream& out, Quoter<TimeDelta> q) {
  if (q.value->is_inf()) {
    return out << "🤔";
  }

  const double ms = q.value->InMillisecondsF();
  if (ms < 1000) {
    return out << base::StringPrintf("%.0f ms", ms);
  }

  const double seconds = ms / 1000;
  if (seconds < 60) {
    return out << base::StringPrintf("%.1f seconds", seconds);
  }

  const double minutes = seconds / 60;
  if (minutes < 60) {
    return out << base::StringPrintf("%.1f minutes", minutes);
  }

  const double hours = minutes / 60;
  return out << base::StringPrintf("%.1f hours", hours);
}

ostream& operator<<(ostream& out, Quoter<Path> q) {
  const std::string& s = q.value->value();
  if (VLOG_IS_ON(1)) {
    return out << "'" << s << "'";
  }

  for (const std::string_view prefix :
       {"/root", "/.files-by-id", "/.shortcuts-by-id"}) {
    if (s.starts_with(prefix)) {
      if (s.size() == prefix.size()) {
        return out << "'" << prefix << "'";
      }
      DCHECK_GT(s.size(), prefix.size());
      if (s[prefix.size()] == '/') {
        return out << "'" << prefix << "/***'";
      }
    }
  }

  return out << "'***'";
}

template <typename T>
ostream& operator<<(ostream& out, Quoter<std::optional<T>> q) {
  const std::optional<T>& v = *q.value;
  if (!v.has_value()) {
    return out << "(nullopt)";
  }

  return out << Quote(*v);
}

ostream& operator<<(ostream& out, Quoter<ShortcutDetails> q) {
  const ShortcutDetails& s = *q.value;
  out << "{" << PinningManager::Id(s.target_stable_id);

  if (s.target_lookup_status != LookupStatus::kOk) {
    out << " " << Quote(s.target_lookup_status);
  }

  return out << "}";
}

ostream& operator<<(ostream& out, Quoter<FileMetadata> q) {
  const FileMetadata& md = *q.value;

  out << "{" << Quote(md.type) << " " << PinningManager::Id(md.stable_id);

  if (md.size != 0) {
    out << " of " << HumanReadableSize(md.size);
  }

  if (md.trashed) {
    out << ", trashed";
  }

  if (md.can_pin != FileMetadata::CanPinStatus::kOk) {
    out << ", not pinnable";
  }

  if (VLOG_IS_ON(2)) {
    out << ", pinned: " << md.pinned;
  } else if (md.pinned) {
    out << ", pinned";
  }

  if (VLOG_IS_ON(2)) {
    out << ", available_offline: " << md.available_offline;
  } else if (md.available_offline) {
    out << ", available offline";
  }

  if (md.shortcut_details) {
    out << ", shortcut to " << Quote(*md.shortcut_details);
  }

  return out << "}";
}

ostream& operator<<(ostream& out, Quoter<mojom::ProgressEvent> q) {
  const mojom::ProgressEvent& e = *q.value;
  out << "{" << PinningManager::Id(e.stable_id) << " "
      << Quote(e.file_path ? *e.file_path : Path(e.path))
      << ", progress: " << base::StringPrintf("%hhu", e.progress) << "%}";
  return out;
}

ostream& operator<<(ostream& out, Quoter<mojom::FileChange> q) {
  const mojom::FileChange& change = *q.value;
  return out << "{" << Quote(change.type) << " "
             << PinningManager::Id(change.stable_id) << " "
             << Quote(change.path) << "}";
}

ostream& operator<<(ostream& out, Quoter<mojom::DriveError> q) {
  const mojom::DriveError& e = *q.value;
  return out << "{" << Quote(e.type) << " " << PinningManager::Id(e.stable_id)
             << " " << Quote(e.path) << "}";
}

// Rounds the given size to the next multiple of 4-KB.
int64_t RoundToBlockSize(int64_t size) {
  const int64_t block_size = 4 << 10;  // 4 KB
  const int64_t mask = block_size - 1;
  static_assert((block_size & mask) == 0, "block_size must be a power of 2");
  return (size + mask) & ~mask;
}

int64_t GetSize(const FileMetadata& metadata) {
  const int64_t kAverageHostedFileSize = 7800;
  return metadata.type == FileMetadata::Type::kHosted ? kAverageHostedFileSize
                                                      : metadata.size;
}

}  // namespace

void RecordBulkPinningEnabledSource(BulkPinningEnabledSource source) {
  base::UmaHistogramEnumeration(
      "FileBrowser.GoogleDrive.BulkPinning.Enabled.Source", source);
}

ostream& NiceNum(ostream& out) {
  out.imbue(NiceNumLocale());
  return out;
}

ostream& operator<<(ostream& out, const PinningManager::Id id) {
  return out << "#" << static_cast<int64_t>(id);
}

ostream& operator<<(ostream& out, HumanReadableSize size) {
  int64_t i = static_cast<int64_t>(size);
  if (i == 0) {
    return out << "zilch";
  }

  if (i < 0) {
    out << '-';
    i = -i;
  }

  {
    std::locale old_locale = out.imbue(NiceNumLocale());
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

  return out << " (" << std::setprecision(4) << d << " " << *unit << ")";
}

ostream& PinningManager::File::PrintTo(ostream& out) const {
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
  const int64_t margin = int64_t(2) << 30;
  const bool enough = required_space + margin <= free_space;
  VLOG_IF(1, !enough) << "Not enough space: Free space "
                      << HumanReadableSize(free_space)
                      << " is less than required space "
                      << HumanReadableSize(required_space) << " + margin "
                      << HumanReadableSize(margin);
  return enough;
}

bool Progress::IsError() const {
  switch (stage) {
    case Stage::kNotEnoughSpace:
    case Stage::kCannotGetFreeSpace:
    case Stage::kCannotListFiles:
    case Stage::kCannotEnableDocsOffline:
      return true;

    case Stage::kGettingFreeSpace:
    case Stage::kListingFiles:
    case Stage::kPausedOffline:
    case Stage::kPausedBatterySaver:
    case Stage::kSuccess:
    case Stage::kSyncing:
    case Stage::kStopped:
      return false;
  }

  NOTREACHED() << "Unexpected Stage " << Quote(stage);
}

bool InProgress(const Stage stage) {
  switch (stage) {
    case Stage::kGettingFreeSpace:
    case Stage::kListingFiles:
    case Stage::kSyncing:
      return true;

    case Stage::kStopped:
    case Stage::kPausedOffline:
    case Stage::kPausedBatterySaver:
    case Stage::kSuccess:
    case Stage::kNotEnoughSpace:
    case Stage::kCannotGetFreeSpace:
    case Stage::kCannotListFiles:
    case Stage::kCannotEnableDocsOffline:
      return false;
  }

  NOTREACHED() << "Unexpected Stage " << Quote(stage);
}

bool IsPaused(const Stage stage) {
  switch (stage) {
    case Stage::kPausedOffline:
    case Stage::kPausedBatterySaver:
      return true;

    case Stage::kGettingFreeSpace:
    case Stage::kListingFiles:
    case Stage::kSyncing:
    case Stage::kStopped:
    case Stage::kSuccess:
    case Stage::kNotEnoughSpace:
    case Stage::kCannotGetFreeSpace:
    case Stage::kCannotListFiles:
    case Stage::kCannotEnableDocsOffline:
      return false;
  }

  NOTREACHED() << "Unexpected Stage " << Quote(stage);
}

bool IsPausedOrInProgress(const Stage stage) {
  switch (stage) {
    case Stage::kGettingFreeSpace:
    case Stage::kListingFiles:
    case Stage::kSyncing:
    case Stage::kPausedOffline:
    case Stage::kPausedBatterySaver:
      return true;

    case Stage::kStopped:
    case Stage::kSuccess:
    case Stage::kNotEnoughSpace:
    case Stage::kCannotGetFreeSpace:
    case Stage::kCannotListFiles:
    case Stage::kCannotEnableDocsOffline:
      return false;
  }

  NOTREACHED() << "Unexpected Stage " << Quote(stage);
}

bool IsSuccessfulDocsOfflineEnablement(DocsOfflineEnableStatus status) {
  switch (status) {
    case DocsOfflineEnableStatus::kSuccess:
    case DocsOfflineEnableStatus::kAlreadyEnabled:
    case DocsOfflineEnableStatus::kOfflineEligible:
      return true;

    case DocsOfflineEnableStatus::kUnknownError:
    case DocsOfflineEnableStatus::kDisableUnsupported:
    case DocsOfflineEnableStatus::kOfflineIneligibleUnknown:
    case DocsOfflineEnableStatus::kOfflineIneligibleOtherUser:
    case DocsOfflineEnableStatus::kOfflineIneligibleDbInInvalidState:
    case DocsOfflineEnableStatus::kOfflineIneligiblePolicyDisallow:
    case DocsOfflineEnableStatus::kOfflineIneligibleNoExtension:
    case DocsOfflineEnableStatus::kOfflineIneligibleInsufficientDiskSpace:
    case DocsOfflineEnableStatus::kNativeMessageHostError:
    case DocsOfflineEnableStatus::kNativeMessageClientError:
    case DocsOfflineEnableStatus::kSystemError:
    case DocsOfflineEnableStatus::kUnknown:
      return false;
  }
}

std::string ToString(Stage stage) {
  return (std::ostringstream() << Quote(stage)).str();
}

std::string ToString(TimeDelta time_delta) {
  return (std::ostringstream() << Quote(time_delta)).str();
}

constexpr TimeDelta kStalledFileInterval = base::Seconds(10);

bool PinningManager::CanPin(const FileMetadata& md, const Path& path) {
  using Type = FileMetadata::Type;
  const auto id = PinningManager::Id(md.stable_id);

  if (md.shortcut_details) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Shortcut to "
            << Id(md.shortcut_details->target_stable_id);
    return false;
  }

  if (md.type == Type::kDirectory) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Directory";
    return false;
  }

  // Hosted docs are heuristically cached via the Docs offline extension.
  // Ignore explicitly pinning them and prefer caching.
  if (md.type == Type::kHosted) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Hosted doc";
    return false;
  }

  if (md.can_pin != FileMetadata::CanPinStatus::kOk) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Cannot be pinned";
    return false;
  }

  if (md.pinned && md.available_offline) {
    VLOG(2) << "Skipped " << id << " " << Quote(path) << ": Already pinned";
    return false;
  }

  if (md.trashed) {
    VLOG(1) << "Skipped " << id << " " << Quote(path) << ": Trashed";
    return false;
  }

  return true;
}

bool PinningManager::Add(const FileMetadata& md, const Path& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Id id = Id(md.stable_id);
  VLOG(3) << "Considering " << id << " " << Quote(path) << " " << Quote(md);

  if (!CanPin(md, path)) {
    progress_.skipped_items++;
    return false;
  }

  const int64_t size = GetSize(md);
  DCHECK_GE(size, 0) << " for " << id << " " << Quote(path);

  if (files_to_track_.empty() && progress_.emptied_queue) {
    progress_.files_to_pin = 0;
    progress_.bytes_to_pin = 0;
    progress_.pinned_files = 0;
    progress_.pinned_bytes = 0;
    progress_.remaining_time = TimeDelta();
    speedometer_.SetTotalBytes(0);
  }

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

bool PinningManager::Remove(const Id id,
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

void PinningManager::Remove(const Files::iterator it,
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

bool PinningManager::Update(const Id id,
                            const Path& path,
                            const int8_t progress_percent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Files::iterator it = files_to_track_.find(id);
  if (it == files_to_track_.end()) {
    VLOG(3) << "Not tracked: " << id << " " << path;
    return false;
  }

  DCHECK_EQ(it->first, id);
  const int64_t transferred = it->second.total * progress_percent / 100;
  return Update(*it, path, transferred, it->second.total);
}

bool PinningManager::Update(const Id id,
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

bool PinningManager::Update(Files::value_type& entry,
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

PinningManager::PinningManager(Path profile_path,
                               Path mount_path,
                               mojom::DriveFs* const drivefs,
                               int64_t queue_size)
    : profile_path_(std::move(profile_path)),
      mount_path_(std::move(mount_path)),
      drivefs_(drivefs),
      queue_size_(queue_size),
      space_getter_(base::BindRepeating(&GetFreeSpace)) {
  DCHECK(drivefs_);
  VLOG(1) << "Creating bulk-pinning manager";
  chromeos::PowerManagerClient* const p = chromeos::PowerManagerClient::Get();
  power_manager_.Observe(p);
  p->GetBatterySaverModeState(base::BindOnce(
      &PinningManager::OnGotBatterySaverState, weak_ptr_factory_.GetWeakPtr()));
  user_data_auth_client_.Observe(ash::UserDataAuthClient::Get());
}

PinningManager::~PinningManager() {
  Stop();
  VLOG(1) << "Deleting bulk-pinning manager";
}

void PinningManager::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (InProgress(progress_.stage)) {
    LOG(ERROR) << "Bulk-pinning manager is already started: It is in stage "
               << Quote(progress_.stage);
    return;
  }

  VLOG(1) << "Starting";
  const bool should_pin = progress_.should_pin;
  progress_ = {};
  progress_.should_pin = should_pin;
  listed_items_.clear();
  files_to_pin_.clear();
  files_to_track_.clear();
  DCHECK_EQ(progress_.syncing_files, 0);

  if (!is_online_) {
    LOG(WARNING) << "Device is currently offline";
    return Complete(Stage::kPausedOffline);
  }

  if (!is_battery_ok_) {
    LOG(WARNING) << "Device is currently in battery saver mode";
    return Complete(Stage::kPausedBatterySaver);
  }

  VLOG(2) << "Getting free space";
  timer_ = base::ElapsedTimer();
  progress_.stage = Stage::kGettingFreeSpace;
  NotifyProgress();

  space_getter_.Run(
      profile_path_.AppendASCII("GCache"),
      base::BindOnce(&PinningManager::OnFreeSpaceRetrieved1, GetWeakPtr()));
}

void PinningManager::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (progress_.stage != Stage::kStopped && !progress_.IsError()) {
    VLOG(1) << "Stopping";
    Complete(Stage::kStopped);
  }
}

bool PinningManager::CalculateRequiredSpace() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsPausedOrInProgress(progress_.stage) && progress_.should_pin) {
    LOG(ERROR) << "Cannot calculate required space: "
               << "Bulk-pinning manager is in stage " << progress_.stage;
    return false;
  }

  progress_.should_pin = false;
  Start();
  return true;
}

void PinningManager::OnFreeSpaceRetrieved1(const int64_t free_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(progress_.stage, Stage::kGettingFreeSpace);

  if (free_space < 0) {
    LOG(ERROR) << "Cannot get free space: Got negative number " << free_space;
    return Complete(Stage::kCannotGetFreeSpace);
  }

  progress_.free_space = free_space;
  VLOG(1) << "Free space: " << HumanReadableSize(free_space);
  VLOG(1) << "Listing files";
  timer_ = base::ElapsedTimer();
  progress_.stage = Stage::kListingFiles;
  NotifyProgress();

  DCHECK_EQ(progress_.total_queries, 0);
  DCHECK_EQ(progress_.active_queries, 0);
  DCHECK_EQ(progress_.max_active_queries, 0);
  ListItems(Id::kNone, Path("/root"));
}

void PinningManager::CheckFreeSpace() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(2) << "Getting free space";
  space_getter_.Run(
      profile_path_.AppendASCII("GCache"),
      base::BindOnce(&PinningManager::OnFreeSpaceRetrieved2, GetWeakPtr()));
}

void PinningManager::LowDiskSpace(const user_data_auth::LowDiskSpace& event) {
  LOG(ERROR) << "LowDiskSpace: " << HumanReadableSize(event.disk_free_bytes());
  OnFreeSpaceRetrieved2(event.disk_free_bytes());
}

void PinningManager::OnSpaceUpdate(const SpaceEvent& event) {
  VLOG(1) << "OnSpaceUpdate: " << HumanReadableSize(event.free_space_bytes());
  OnFreeSpaceRetrieved2(event.free_space_bytes());
}

void PinningManager::OnFreeSpaceRetrieved2(const int64_t free_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (free_space < 0) {
    LOG(ERROR) << "Cannot get free space: Got negative number " << free_space;
    return Complete(Stage::kCannotGetFreeSpace);
  }

  progress_.free_space = free_space;
  VLOG(1) << "Free space: " << HumanReadableSize(progress_.free_space);

  if (progress_.HasEnoughFreeSpace()) {
    if (progress_.stage == Stage::kNotEnoughSpace) {
      // Transition from kNotEnoughSpace to kSuccess.
      Complete(Stage::kSuccess);
    } else {
      NotifyProgress();
    }
  } else if (progress_.stage != Stage::kNotEnoughSpace) {
    Complete(Stage::kNotEnoughSpace);
  }
}

void PinningManager::OnGotBatterySaverState(
    std::optional<power_manager::BatterySaverModeState> state) {
  if (state) {
    BatterySaverModeStateChanged(*state);
  }
}

void PinningManager::BatterySaverModeStateChanged(
    const power_manager::BatterySaverModeState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_battery_ok_ = !state.enabled();
  VLOG(2) << "Battery saver mode changed, online=" << is_online_
          << ", battery=" << is_battery_ok_;
  if (!is_battery_ok_ && InProgress(progress_.stage)) {
    VLOG(1) << "Pausing for battery saver";
    return Complete(Stage::kPausedBatterySaver);
  }

  if (is_battery_ok_ && IsPaused(progress_.stage)) {
    VLOG(1) << "Restarting from battery saver";
    Start();
  }
}

bool PinningManager::IsTrackedAndUnpinned(Id id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const Files::const_iterator it = files_to_track_.find(id);
  if (it == files_to_track_.end()) {
    return false;
  }
  return !it->second.pinned;
}

void PinningManager::ListItems(const Id dir_id, Path dir_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Visiting " << dir_id << " " << Quote(dir_path);

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->page_size = 1000;
  params->my_drive_results_only = true;
  params->parent_stable_id = static_cast<int64_t>(dir_id);
  params->query_source = mojom::QueryParameters::QuerySource::kLocalAndCloud;

  Query query;
  drivefs_->StartSearchQuery(query.BindNewPipeAndPassReceiver(),
                             std::move(params));

  progress_.total_queries++;
  progress_.active_queries++;
  VLOG(2) << NiceNum << "Active queries: " << progress_.active_queries;

  if (progress_.max_active_queries < progress_.active_queries) {
    progress_.max_active_queries = progress_.active_queries;
  }

  DCHECK_GE(progress_.total_queries, progress_.active_queries);

  GetNextPage(dir_id, std::move(dir_path), std::move(query));
}

void PinningManager::GetNextPage(const Id dir_id, Path dir_path, Query query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(progress_.stage, Stage::kListingFiles);
  DCHECK(query);
  // Get the underlying pointer because we're going to move `query`.
  mojom::SearchQuery* const q = query.get();
  VLOG(2) << "Getting next batch of items from " << dir_id << " "
          << Quote(dir_path);
  DCHECK(q);
  q->GetNextPage(base::BindOnce(
      [](const base::WeakPtr<PinningManager> pinning_manager, Id dir_id,
         Path dir_path, Query query, const drive::FileError error,
         const std::optional<std::vector<QueryItemPtr>> items) {
        if (pinning_manager) {
          pinning_manager->OnSearchResult(
              dir_id, std::move(dir_path), std::move(query), error,
              items ? *items : base::span<const QueryItemPtr>{});
        } else {
          VLOG(1) << "Dropped query for " << dir_id << " " << Quote(dir_path);
        }
      },
      GetWeakPtr(), dir_id, std::move(dir_path), std::move(query)));
}

void PinningManager::OnSearchResult(
    const Id dir_id,
    Path dir_path,
    Query query,
    const drive::FileError error,
    const base::span<const QueryItemPtr> items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(progress_.stage, Stage::kListingFiles);

  if (!drive::IsFileErrorOk(error)) {
    LOG(ERROR) << "Cannot visit " << dir_id << " " << Quote(dir_path) << ": "
               << error;
    switch (error) {
      default:
        return Complete(Stage::kCannotListFiles);

      case drive::FILE_ERROR_NO_CONNECTION:
      case drive::FILE_ERROR_SERVICE_UNAVAILABLE:
        const TimeDelta delay = base::Seconds(5);
        LOG(ERROR) << "Will retry in " << Quote(delay);
        SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&PinningManager::GetNextPage, GetWeakPtr(), dir_id,
                           std::move(dir_path), std::move(query)),
            delay);
        return;
    }
  }

  progress_.time_spent_listing_items = timer_.Elapsed();

  // Output a warning if the time spent listing files is taking longer than 30s
  // but only log this every 30s after that.
  if (progress_.time_spent_listing_items > Seconds(30) &&
      (base::Time::Now() - Seconds(30) >
       last_long_listing_files_warning_time_)) {
    LOG(WARNING) << NiceNum << "Listing files is taking a long time, found "
                 << progress_.listed_items << " items in "
                 << Quote(progress_.time_spent_listing_items);
    last_long_listing_files_warning_time_ = base::Time::Now();
  }

  if (items.empty() && error != drive::FILE_ERROR_OK_WITH_MORE_RESULTS) {
    VLOG(1) << "Visited " << dir_id << " " << Quote(dir_path);

    DCHECK_LE(progress_.active_queries, progress_.max_active_queries);
    DCHECK_GT(progress_.active_queries, 0);
    if (--progress_.active_queries != 0) {
      VLOG(2) << NiceNum << "Active queries: " << progress_.active_queries;
      return;
    }

    LOG_IF(WARNING, progress_.time_spent_listing_items > Seconds(30))
        << "Listing files took a long time, found" << progress_.listed_items
        << " items in " << Quote(progress_.time_spent_listing_items);
    base::UmaHistogramLongTimes(
        "FileBrowser.GoogleDrive.BulkPinning.TimeSpentListing",
        progress_.time_spent_listing_items);
    VLOG(1) << "Finished listing files in "
            << Quote(progress_.time_spent_listing_items);
    VLOG(1) << NiceNum << "Total queries: " << progress_.total_queries;
    VLOG(1) << NiceNum
            << "Max active queries: " << progress_.max_active_queries;
    VLOG(1) << NiceNum << "Found " << progress_.listed_items
            << " items: " << progress_.listed_dirs << " dirs, "
            << progress_.listed_files << " files, " << progress_.listed_docs
            << " docs, " << progress_.listed_shortcuts << " shortcuts";
    VLOG(1) << NiceNum << "Tracking " << files_to_track_.size() << " files";
    listed_items_.clear();
    return StartPinning();
  }

  if (error == drive::FILE_ERROR_OK_WITH_MORE_RESULTS) {
    VLOG(2) << "Potentially more than " << items.size() << " items from"
            << dir_id << " " << Quote(dir_path)
            << ": Need to make a cloud query";
  } else {
    VLOG(2) << "Got " << items.size() << " items from " << dir_id << " "
            << Quote(dir_path);
  }

  progress_.listed_items += items.size();
  for (const QueryItemPtr& item : items) {
    DCHECK(item);
    HandleQueryItem(dir_id, dir_path, *item);
  }

  VLOG(1) << NiceNum << "Listed " << progress_.listed_items << " items in "
          << Quote(progress_.time_spent_listing_items) << ", Skipped "
          << progress_.skipped_items << " items, Tracking "
          << files_to_track_.size() << " files";
  NotifyProgress();
  GetNextPage(dir_id, std::move(dir_path), std::move(query));
}

void PinningManager::HandleQueryItem(Id dir_id,
                                     const Path& dir_path,
                                     const QueryItem& item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(item.metadata);
  using Type = FileMetadata::Type;
  FileMetadata& md = *item.metadata;
  Id id = Id(md.stable_id);
  const Path& path = item.path;

  VLOG(2) << "Listed " << id << " " << Quote(path) << ": " << Quote(md);

  if (!dir_path.IsParent(path)) {
    // This can happen when the parent folder was found by following a shortcut.
    VLOG(2) << Quote(md.type) << " " << id << " " << Quote(path)
            << " is not in Directory " << dir_id << " " << Quote(dir_path);
  }

  // Is this item a shortcut?
  if (md.shortcut_details) {
    progress_.listed_shortcuts++;

    // Is the shortcut pointing to a directory?
    if (md.type == Type::kDirectory) {
      // The shortcut's target is a directory.
      progress_.skipped_items++;
      VLOG(1) << "Skipped shortcut " << id << " " << Quote(path) << " to "
              << Quote(md.type) << " "
              << Id(md.shortcut_details->target_stable_id);
      return;
    }

    // Is the shortcut's target accessible?
    if (md.shortcut_details->target_lookup_status != LookupStatus::kOk) {
      // The shortcut target is not accessible.
      progress_.skipped_items++;
      progress_.broken_shortcuts++;
      VLOG(1) << "Broken shortcut " << id << " " << Quote(path) << ": "
              << "Target " << Quote(md.type) << " "
              << Id(md.shortcut_details->target_stable_id)
              << " has lookup error "
              << Quote(md.shortcut_details->target_lookup_status) << ": "
              << Quote(md);
      return;
    }

    // Is the shortcut's target in the trash bin?
    if (md.trashed) {
      // The shortcut target is in the trash bin.
      progress_.skipped_items++;
      progress_.broken_shortcuts++;
      VLOG(1) << "Broken shortcut " << id << " " << Quote(path) << ": "
              << "Target " << Quote(md.type) << " "
              << Id(md.shortcut_details->target_stable_id)
              << " is trashed: " << Quote(md);
      return;
    }

    // The shortcut's target is accessible and it is not a directory.
    VLOG(1) << "Following shortcut " << id << " " << Quote(path) << " to "
            << Quote(md.type) << " "
            << Id(md.shortcut_details->target_stable_id) << ": " << Quote(md);

    // Follow this shortcut.
    md.stable_id = md.shortcut_details->target_stable_id;
    id = Id(md.stable_id);
    md.shortcut_details.reset();
  }

  // Deduplicate items.
  if (const auto [it, ok] = listed_items_.try_emplace(id, dir_id); !ok) {
    DCHECK_EQ(it->first, id);
    progress_.skipped_items++;
    VLOG(1) << "Skipped " << Quote(md.type) << " " << id << " " << Quote(path)
            << " " << Quote(md) << " seen in Directory " << dir_id << " "
            << Quote(dir_path) << ": Previously seen in Directory "
            << it->second;
    return;
  }

  switch (md.type) {
    case Type::kFile:
      progress_.listed_files++;
      Add(md, path);
      return;

    case Type::kHosted:
      progress_.listed_docs++;
      Add(md, path);
      return;

    case Type::kDirectory:
      progress_.listed_dirs++;
      ListItems(id, path);
      return;
  }

  progress_.skipped_items++;
  LOG(ERROR) << "Unexpected item type " << Quote(md.type) << " for " << id
             << " " << path << ": " << Quote(md);
}

void PinningManager::Complete(const Stage stage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!InProgress(stage));

  switch (stage) {
    case Stage::kSuccess:
      VLOG(1) << "Finished with success";
      break;

    case Stage::kPausedOffline:
      VLOG(1) << "Paused because offline";
      break;

    case Stage::kPausedBatterySaver:
      VLOG(1) << "Paused because of battery saver";
      break;

    case Stage::kStopped:
      VLOG(1) << "Stopped";
      break;

    default:
      LOG_IF(ERROR, progress_.stage == Stage::kNotEnoughSpace)
          << "Not enough space: Free space "
          << HumanReadableSize(progress_.free_space)
          << " is less than required space "
          << HumanReadableSize(progress_.required_space) << " + margin";

      LOG(ERROR) << "Finished with error: " << Quote(stage);

      switch (progress_.stage) {
        case Stage::kListingFiles:
          base::UmaHistogramEnumeration(
              "FileBrowser.GoogleDrive.BulkPinning.Listing.Error", stage);
          break;

        case Stage::kSyncing:
          base::UmaHistogramEnumeration(
              "FileBrowser.GoogleDrive.BulkPinning.Syncing.Error", stage);
          break;

        default:
          break;
      }
  }

  progress_.stage = stage;

  if (progress_.stage == Stage::kNotEnoughSpace) {
    StartMonitoringSpace();
  } else {
    StopMonitoringSpace();
  }

  weak_ptr_factory_.InvalidateWeakPtrs();
  listed_items_.clear();
  files_to_pin_.clear();
  files_to_track_.clear();
  progress_.syncing_files = 0;
  progress_.active_queries = 0;
  NotifyProgress();

  if (completion_callback_) {
    std::move(completion_callback_).Run(stage);
  }
}

void PinningManager::StartPinning() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(progress_.stage, Stage::kListingFiles);

  VLOG(1) << NiceNum << "To pin: " << files_to_pin_.size() << " files, "
          << HumanReadableSize(progress_.bytes_to_pin);
  VLOG(1) << "Required space: " << HumanReadableSize(progress_.required_space);

  if (!progress_.HasEnoughFreeSpace()) {
    return Complete(Stage::kNotEnoughSpace);
  }

  if (!progress_.should_pin) {
    VLOG(1) << "Should not pin files";
    is_first_sync_ = true;
    return Complete(Stage::kSuccess);
  }

  if (is_first_sync_) {
    base::UmaHistogramSparse(
        "FileBrowser.GoogleDrive.BulkPinning.ToDownloadMiB",
        progress_.bytes_to_pin >> 20);
    is_first_sync_ = false;
  }

  base::UmaHistogramSparse("FileBrowser.GoogleDrive.BulkPinning.QueueSize",
                           queue_size_);

  timer_ = base::ElapsedTimer();
  progress_.stage = Stage::kSyncing;
  NotifyProgress();

  EnableDocsOffline();

  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PinningManager::CheckStalledFiles, GetWeakPtr()),
      kStalledFileInterval);

  CheckFreeSpace();
  StartMonitoringSpace();
  PinSomeFiles();
}

bool PinningManager::StartMonitoringSpace() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (spaced_client_.IsObserving()) {
    VLOG(1) << "SpacedClient::Observer is already registered";
    return true;
  }

  SpacedClient* const spaced = SpacedClient::Get();
  DCHECK(spaced);
  if (!spaced->IsConnected()) {
    LOG(ERROR) << "SpacedClient is not connected";
    return false;
  }

  spaced_client_.Observe(spaced);
  VLOG(1) << "Added SpacedClient::Observer";
  return true;
}

void PinningManager::StopMonitoringSpace() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  spaced_client_.Reset();
}

void PinningManager::EnableDocsOffline() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(drivefs_);
  drivefs_->SetDocsOfflineEnabled(
      true, base::BindOnce([](drive::FileError error,
                              DocsOfflineEnableStatus status) {
        LOG_IF(ERROR, error != drive::FILE_ERROR_OK)
            << "Failed to enable Docs offline: " << error << " with status "
            << Quote(status);
        VLOG(1) << "Docs offline enablement status: " << Quote(status);
        base::UmaHistogramEnumeration(
            "FileBrowser.GoogleDrive.BulkPinning.EnableDocsOffline", status);
      }));
}

void PinningManager::PinSomeFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (progress_.stage != Stage::kSyncing) {
    return NotifyProgress();
  }

  if (!should_pin_files_for_testing_) {
    return NotifyProgress();
  }

  while (progress_.syncing_files < queue_size_ && !files_to_pin_.empty()) {
    const Id id = files_to_pin_.extract(files_to_pin_.begin()).value();
    const Files::iterator it = files_to_track_.find(id);
    DCHECK(it != files_to_track_.end()) << "Not tracked: " << id;
    DCHECK_EQ(it->first, id);
    File& file = it->second;
    const Path& path = file.path;

    DCHECK(!file.pinned) << "Already pinned: " << id << " " << Quote(path);
    VLOG(2) << "Pinning " << id << " " << Quote(path);
    drivefs_->SetPinnedByStableId(
        static_cast<int64_t>(id), true,
        base::BindOnce(&PinningManager::OnFilePinned, GetWeakPtr(), id, path));

    file.pinned = true;
    progress_.syncing_files++;
    DCHECK_EQ(progress_.syncing_files, CountPinnedFiles());
  }

  if (!progress_.emptied_queue) {
    progress_.time_spent_pinning_files = timer_.Elapsed();
  }

  bool notify_progress = false;
  if (progress_timer_.Elapsed() >= base::Seconds(1)) {
    notify_progress = true;
    VLOG(1) << NiceNum << "Progress "
            << Percentage(progress_.pinned_bytes, progress_.bytes_to_pin)
            << "%: Synced " << HumanReadableSize(progress_.pinned_bytes)
            << " and " << progress_.pinned_files << " files in "
            << Quote(progress_.time_spent_pinning_files) << ", Syncing "
            << progress_.syncing_files << " files";
    progress_timer_ = {};
  }

  if (files_to_track_.empty() && !progress_.emptied_queue) {
    notify_progress = true;
    progress_.emptied_queue = true;
    LOG_IF(ERROR, progress_.failed_files > 0)
        << NiceNum << "Failed to pin " << progress_.failed_files << " files";
    VLOG(1) << NiceNum << "Pinned " << progress_.pinned_files << " files and "
            << HumanReadableSize(progress_.pinned_bytes) << " in "
            << Quote(progress_.time_spent_pinning_files);
    VLOG(2) << NiceNum << "Useful events: " << progress_.useful_events;
    VLOG(2) << NiceNum << "Duplicated events: " << progress_.duplicated_events;
  }

  if (notify_progress) {
    NotifyProgress();
  }
}

void PinningManager::OnFilePinned(const Id id,
                                  const Path& path,
                                  const drive::FileError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Records error in a UMA histogram. The `1 - error` expression converts the
  // negative drive::FileError enum values 0, -1, -2, -3...  (defined in
  // components/drive/file_errors.h) to the positive UMA DriveFileError enum
  // values +1, +2, +3, +4... (defined in tools/metrics/histograms/enums.xml).
  // The `2 - drive::FILE_ERROR_MAX` expression does likewise, but also
  // converting from an inclusive to exclusive bound.
  base::UmaHistogramExactLinear(
      "FileBrowser.GoogleDrive.BulkPinning.Pinning.Error", 1 - error,
      2 - drive::FILE_ERROR_MAX);

  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot pin " << id << " " << Quote(path) << ": " << error;
    if (Remove(id, path, 0)) {
      progress_.failed_files++;
      UmaHistogramBoolean("FileBrowser.GoogleDrive.BulkPinning.PinnedFiles",
                          false);
      PinSomeFiles();
    }
    return;
  }

  VLOG(1) << "Pinned " << id << " " << Quote(path);

  const auto it = files_to_track_.find(id);
  if (it == files_to_track_.end()) {
    LOG(ERROR) << "Got unexpected notification that " << id << " "
               << Quote(path) << " was pinned: The item is not tracked";
    DCHECK(!files_to_pin_.contains(id));
    return;
  }

  DCHECK_EQ(it->first, id);
  File& file = it->second;
  if (!file.pinned) {
    LOG(ERROR)
        << "Got unexpected notification that " << id << " " << Quote(path)
        << " was pinned: The item is not remembered as having been pinned";
    file.pinned = true;
    const size_t erased = files_to_pin_.erase(id);
    DCHECK_EQ(erased, 1u);
    return;
  }

  DCHECK(!files_to_pin_.contains(id));
}

void PinningManager::OnItemProgress(const mojom::ProgressEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!InProgress(progress_.stage)) {
    VLOG(2) << "Ignored " << Quote(event);
    return;
  }
  VLOG(3) << "Received " << Quote(event);

  Path relative_path("/");
  if (!mount_path_.AppendRelativePath(
          event.file_path ? *event.file_path : Path(event.path),
          &relative_path)) {
    LOG(ERROR) << "Path not relative to drive mount";
    return;
  }
  const Id id = Id(event.stable_id);

  if (event.progress >= 0 && event.progress < 100) {
    Update(id, relative_path, event.progress);
  } else if (event.progress == 100) {
    if (!Remove(id, relative_path)) {
      // Item is not being tracked.
      return;
    }
    VLOG(2) << "Synced " << id << " " << Quote(relative_path);
    VLOG_IF(1, !VLOG_IS_ON(2))
        << "Synced " << id << " " << Quote(relative_path);
    progress_.pinned_files++;
    UmaHistogramBoolean("FileBrowser.GoogleDrive.BulkPinning.PinnedFiles",
                        true);
  } else if (!Remove(id, relative_path)) {
    LOG(ERROR) << "Invalid event " << Quote(event);
  }

  PinSomeFiles();
}

void PinningManager::NotifyDelete(const Id id, const Path& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!Remove(id, path, 0)) {
    VLOG(1) << "Not tracked: " << id << " " << Quote(path);
    return;
  }

  VLOG(1) << "Stopped tracking " << id << " " << Quote(path);
  progress_.failed_files++;
  UmaHistogramBoolean("FileBrowser.GoogleDrive.BulkPinning.PinnedFiles", false);
  PinSomeFiles();
}

void PinningManager::OnUnmounted() {
  VLOG(1) << "Unmounted DriveFS";
}

void PinningManager::OnFilesChanged(
    const std::vector<mojom::FileChange>& changes) {
  using Type = mojom::FileChange::Type;
  for (const mojom::FileChange& event : changes) {
    switch (event.type) {
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

void PinningManager::OnFileCreated(const mojom::FileChange& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(event.type, mojom::FileChange::Type::kCreate);

  if (!InProgress(progress_.stage)) {
    VLOG(2) << "Ignored " << Quote(event) << ": PinningManager is currently "
            << Quote(progress_.stage);
    return;
  }

  const Id id = Id(event.stable_id);
  const Path& path = event.path;

  if (id == Id::kNone) {
    // Ignore spurious event (b/268419828).
    VLOG(2) << "Ignored " << Quote(event) << ": Spurious event";
    return;
  }

  if (!Path("/root").IsParent(path)) {
    VLOG(2) << "Ignored " << Quote(event) << ": Not in 'My Drive'";
    return;
  }

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
      base::BindOnce(&PinningManager::OnMetadataForCreatedFile, GetWeakPtr(),
                     id, path));
}

void PinningManager::OnFileDeleted(const mojom::FileChange& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(event.type, mojom::FileChange::Type::kDelete);

  VLOG(1) << "Got " << Quote(event);
  NotifyDelete(Id(event.stable_id), event.path);
}

void PinningManager::OnFileModified(const mojom::FileChange& event) {
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
      base::BindOnce(&PinningManager::OnMetadataForModifiedFile, GetWeakPtr(),
                     id, path));
}

void PinningManager::OnError(const mojom::DriveError& error) {
  LOG(ERROR) << "Got DriveError " << Quote(error);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error.type == mojom::DriveError::Type::kPinningFailedDiskFull &&
      InProgress(progress_.stage)) {
    Complete(Stage::kNotEnoughSpace);
  }
}

void PinningManager::NotifyProgress() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (progress_.pinned_bytes > 0) {
    speedometer_.SetTotalBytes(progress_.bytes_to_pin);
    if (speedometer_.Update(progress_.pinned_bytes)) {
      progress_.remaining_time = speedometer_.GetRemainingTime();
    }
  }

  for (Observer& observer : observers_) {
    observer.OnProgress(progress_);
  }
}

void PinningManager::CheckStalledFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
        base::BindOnce(&PinningManager::OnMetadataForModifiedFile, GetWeakPtr(),
                       id, path));
  }

  SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PinningManager::CheckStalledFiles, GetWeakPtr()),
      kStalledFileInterval);
}

void PinningManager::OnMetadataForCreatedFile(const Id id,
                                              const Path& path,
                                              const drive::FileError error,
                                              const FileMetadataPtr metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot get metadata of created " << id << " " << Quote(path)
               << ": " << error;
    return NotifyDelete(id, path);
  }

  DCHECK(metadata);
  const FileMetadata& md = *metadata;
  DCHECK_EQ(id, Id(md.stable_id));
  VLOG(2) << "Got metadata of created " << id << " " << Quote(path) << ": "
          << Quote(md);

  if (Add(md, path)) {
    PinSomeFiles();
  }
}

void PinningManager::OnMetadataForModifiedFile(const Id id,
                                               const Path& path,
                                               const drive::FileError error,
                                               const FileMetadataPtr metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Cannot get metadata of modified " << id << " " << Quote(path)
               << ": " << error;
    return NotifyDelete(id, path);
  }

  DCHECK(metadata);
  const FileMetadata& md = *metadata;
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
    UmaHistogramBoolean("FileBrowser.GoogleDrive.BulkPinning.PinnedFiles",
                        false);
    return PinSomeFiles();
  }

  DCHECK(md.pinned);
  if (md.available_offline) {
    Remove(it, path, GetSize(md));
    VLOG(1) << "Synced " << id << " " << Quote(path);
    progress_.pinned_files++;
    UmaHistogramBoolean("FileBrowser.GoogleDrive.BulkPinning.PinnedFiles",
                        true);
    PinSomeFiles();
  }
}

void PinningManager::SetOnline(const bool online) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "Online: " << online << ", battery: " << is_battery_ok_;
  is_online_ = online;

  if (!is_online_ && InProgress(progress_.stage)) {
    VLOG(1) << "Going offline";
    return Complete(Stage::kPausedOffline);
  }

  if (is_online_ && IsPaused(progress_.stage)) {
    VLOG(1) << "Coming back online";
    Start();
  }
}

PinningManager::Observer::~Observer() {
  CHECK(!IsInObserverList());
}

}  // namespace drivefs::pinning
