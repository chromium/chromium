// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_persistent_storage_manager.h"

#include <string.h>

#include <string>

#include "base/bind.h"
#include "base/containers/adapters.h"
#include "base/debug/alias.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/breadcrumb_persistent_storage_util.h"

namespace breadcrumbs {

namespace {

const char kEventSeparator[] = "\n";

// Minimum time between breadcrumb writes to disk.
constexpr auto kMinDelayBetweenWrites = base::Milliseconds(250);

// Writes the given |events| to |file_path| at |position|. If |append| is false,
// the existing file will be overwritten.
void DoWriteEventsToFile(const base::FilePath& file_path,
                         const size_t position,
                         const std::string& events,
                         const bool append,
                         const size_t write_counter,
                         const size_t write_counter_at_last_full_rewrite) {
  const base::MemoryMappedFile::Region region = {0, kPersistedFilesizeInBytes};
  base::MemoryMappedFile file;
  int flags = base::File::FLAG_READ | base::File::FLAG_WRITE;
  if (append) {
    flags |= base::File::FLAG_OPEN_ALWAYS;
  } else {
    flags |= base::File::FLAG_CREATE_ALWAYS;
  }
  const bool file_valid =
      file.Initialize(base::File(file_path, flags), region,
                      base::MemoryMappedFile::READ_WRITE_EXTEND);

  if (file_valid) {
    const size_t remaining_length = kPersistedFilesizeInBytes - position;

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/1327267): Remove this once crashes in this function are
    // understood. The first and last values are delimiters to aid in finding
    // this array on the stack, as CrOS and Android crashes are hard to debug.
    size_t debug_data[] = {0x1234beef,
                           reinterpret_cast<size_t>(file.data()),
                           file.length(),
                           position,
                           events.length(),
                           write_counter,
                           write_counter_at_last_full_rewrite,
                           0x5678beef};
    base::debug::Alias(&debug_data);
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

    char* data = reinterpret_cast<char*>(file.data());
    base::strlcpy(&data[position], events.c_str(), remaining_length);
  }
}

// Returns breadcrumb events stored at |file_path|.
std::vector<std::string> DoGetStoredEvents(const base::FilePath& file_path) {
  base::File events_file(file_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!events_file.IsValid()) {
    // File may not yet exist.
    return std::vector<std::string>();
  }

  size_t file_size = events_file.GetLength();
  if (file_size <= 0)
    return std::vector<std::string>();

  // Do not read more than |kPersistedFilesizeInBytes|, in case the file was
  // corrupted. If |kPersistedFilesizeInBytes| has been reduced since the last
  // breadcrumbs file was saved, this could result in a one time loss of the
  // oldest breadcrumbs which is ok because the decision has already been made
  // to reduce the size of the stored breadcrumbs.
  if (file_size > kPersistedFilesizeInBytes)
    file_size = kPersistedFilesizeInBytes;

  std::vector<uint8_t> data;
  data.resize(file_size);
  if (!events_file.ReadAndCheck(/*offset=*/0, data))
    return std::vector<std::string>();

  const std::string persisted_events(data.begin(), data.end());
  const std::string all_events =
      persisted_events.substr(/*pos=*/0, strlen(persisted_events.c_str()));
  return base::SplitString(all_events, kEventSeparator, base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

// Returns the total length of stored breadcrumb events at |file_path|. The
// file is opened and the length of the string contents calculated because
// the file size is always constant. (Due to base::MemoryMappedFile filling the
// unused space with \0s.
size_t DoGetStoredEventsLength(const base::FilePath& file_path) {
  base::File events_file(file_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!events_file.IsValid())
    return 0;

  size_t file_size = events_file.GetLength();
  if (file_size <= 0)
    return 0;

  // Do not read more than |kPersistedFilesizeInBytes|, in case the file was
  // corrupted. If |kPersistedFilesizeInBytes| has been reduced since the last
  // breadcrumbs file was saved, this could result in a one time loss of the
  // oldest breadcrumbs which is ok because the decision has already been made
  // to reduce the size of the stored breadcrumbs.
  if (file_size > kPersistedFilesizeInBytes)
    file_size = kPersistedFilesizeInBytes;

  std::vector<uint8_t> data;
  data.resize(file_size);
  if (!events_file.ReadAndCheck(/*offset=*/0, data))
    return 0;

  const std::string persisted_events(data.begin(), data.end());
  return strlen(persisted_events.c_str());
}

}  // namespace

BreadcrumbPersistentStorageManager::BreadcrumbPersistentStorageManager(
    const base::FilePath& directory,
    base::RepeatingCallback<bool()> is_metrics_enabled_callback)
    :  // Ensure first event will not be delayed by initializing with a time in
       // the past.
      last_written_time_(base::TimeTicks::Now() - kMinDelayBetweenWrites),
      breadcrumbs_file_path_(GetBreadcrumbPersistentStorageFilePath(directory)),
      is_metrics_enabled_callback_(std::move(is_metrics_enabled_callback)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      weak_ptr_factory_(this) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DoGetStoredEventsLength, breadcrumbs_file_path_),
      base::BindOnce(
          &BreadcrumbPersistentStorageManager::InitializeFilePosition,
          weak_ptr_factory_.GetWeakPtr()));
}

BreadcrumbPersistentStorageManager::~BreadcrumbPersistentStorageManager() =
    default;

void BreadcrumbPersistentStorageManager::GetStoredEvents(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoGetStoredEvents, breadcrumbs_file_path_),
      std::move(callback));
}

void BreadcrumbPersistentStorageManager::RewriteAllExistingBreadcrumbs() {
  // Clear `pending_breadcrumbs_`, as they will be included in the result from
  // BreadcrumbManager::GetEvents() and written to disk.
  pending_breadcrumbs_.clear();
  write_timer_.Stop();
  last_written_time_ = base::TimeTicks::Now();
  file_position_ = 0;

  if (!CheckForFileConsent())
    return;

  base::circular_deque events = BreadcrumbManager::GetInstance().GetEvents();
  std::vector<std::string> breadcrumbs;
  for (const std::string& event : base::Reversed(events)) {
    // Reduce saved events to only the amount that can be included in a crash
    // report. This allows future events to be appended up to
    // `kPersistedFilesizeInBytes`, reducing the number of resizes needed.
    const int event_with_separator_size =
        event.size() + strlen(kEventSeparator);
    if (event_with_separator_size + file_position_.value() >= kMaxDataLength)
      break;

    breadcrumbs.push_back(kEventSeparator);
    breadcrumbs.push_back(event);
    file_position_ = file_position_.value() + event_with_separator_size;
  }

  std::reverse(breadcrumbs.begin(), breadcrumbs.end());
  const std::string breadcrumbs_string = base::JoinString(breadcrumbs, "");

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DoWriteEventsToFile, breadcrumbs_file_path_,
                     /*position=*/0, breadcrumbs_string, /*append=*/false,
                     write_counter_, write_counter_at_last_full_rewrite_));
}

void BreadcrumbPersistentStorageManager::WritePendingBreadcrumbs() {
  if (!CheckForFileConsent() || pending_breadcrumbs_.empty())
    return;

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DoWriteEventsToFile, breadcrumbs_file_path_,
                                file_position_.value(), pending_breadcrumbs_,
                                /*append=*/true, write_counter_,
                                write_counter_at_last_full_rewrite_));

  file_position_ = file_position_.value() + pending_breadcrumbs_.size();
  last_written_time_ = base::TimeTicks::Now();

  pending_breadcrumbs_.clear();
}

void BreadcrumbPersistentStorageManager::EventAdded(const std::string& event) {
  pending_breadcrumbs_ += event + kEventSeparator;
  WriteEvents();
}

bool BreadcrumbPersistentStorageManager::CheckForFileConsent() {
  static bool should_create_files = true;
  const bool is_metrics_and_crash_reporting_enabled =
      is_metrics_enabled_callback_.Run();

  // If metrics consent has been revoked since this was last checked, delete any
  // existing breadcrumbs files.
  if (should_create_files && !is_metrics_and_crash_reporting_enabled) {
    DeleteBreadcrumbFiles(breadcrumbs_file_path_.DirName());
    file_position_ = 0;
    pending_breadcrumbs_.clear();
  }

  should_create_files = is_metrics_and_crash_reporting_enabled;
  return should_create_files;
}

void BreadcrumbPersistentStorageManager::InitializeFilePosition(
    size_t file_size) {
  file_position_ = file_size;
  // Write any startup events that have accumulated while waiting for this
  // function to run.
  WriteEvents();
}

void BreadcrumbPersistentStorageManager::WriteEvents() {
  // No events can be written to the file until the size of existing breadcrumbs
  // is known.
  if (!file_position_)
    return;

  write_timer_.Stop();

  const base::TimeDelta time_delta_since_last_write =
      base::TimeTicks::Now() - last_written_time_;
  if (time_delta_since_last_write < kMinDelayBetweenWrites) {
    // If an event was just written, delay writing the event to disk in order to
    // limit overhead.
    write_timer_.Start(
        FROM_HERE, kMinDelayBetweenWrites - time_delta_since_last_write,
        base::BindOnce(&BreadcrumbPersistentStorageManager::WriteEvents,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If the event does not fit within |kPersistedFilesizeInBytes|, rewrite the
  // file to trim old events.
  if ((file_position_.value() + pending_breadcrumbs_.size())
      // Use >= here instead of > to allow space for \0 to terminate file.
      >= kPersistedFilesizeInBytes) {
    RewriteAllExistingBreadcrumbs();
    write_counter_at_last_full_rewrite_ = ++write_counter_;
    return;
  }

  // Otherwise, simply append the pending breadcrumbs.
  WritePendingBreadcrumbs();
  ++write_counter_;
}

}  // namespace breadcrumbs
