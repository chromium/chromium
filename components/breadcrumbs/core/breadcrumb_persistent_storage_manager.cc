// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/breadcrumbs/core/breadcrumb_persistent_storage_manager.h"

#include <string.h>

#include <string>

#include "base/containers/adapters.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
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
                         const bool append) {
  const base::MemoryMappedFile::Region region = {0, kPersistedFilesizeInBytes};
  base::MemoryMappedFile file;
  int flags = base::File::FLAG_READ | base::File::FLAG_WRITE;
  flags |=
      append ? base::File::FLAG_OPEN_ALWAYS : base::File::FLAG_CREATE_ALWAYS;
  if (!file.Initialize(base::File(file_path, flags), region,
                       base::MemoryMappedFile::READ_WRITE_EXTEND)) {
    return;
  }

  CHECK(position + events.length() <= kPersistedFilesizeInBytes);
  char* data = reinterpret_cast<char*>(file.data());
  base::strlcpy(&data[position], events.c_str(),
                kPersistedFilesizeInBytes - position);
}

// Returns breadcrumb events stored at |file_path|.
std::string DoGetStoredEvents(const base::FilePath& file_path) {
  base::File events_file(file_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!events_file.IsValid()) {
    // File may not yet exist.
    return std::string();
  }

  size_t file_size = events_file.GetLength();
  if (file_size <= 0) {
    return std::string();
  }

  // Do not read more than |kPersistedFilesizeInBytes|, in case the file was
  // corrupted. If |kPersistedFilesizeInBytes| has been reduced since the last
  // breadcrumbs file was saved, this could result in a one time loss of the
  // oldest breadcrumbs which is ok because the decision has already been made
  // to reduce the size of the stored breadcrumbs.
  if (file_size > kPersistedFilesizeInBytes) {
    file_size = kPersistedFilesizeInBytes;
  }

  std::vector<uint8_t> data;
  data.resize(file_size);
  if (!events_file.ReadAndCheck(/*offset=*/0, data)) {
    return std::string();
  }

  std::string persisted_events(data.begin(), data.end());
  // Resize from the length of the entire file to only the portion containing
  // data, i.e., exclude trailing \0s.
  persisted_events.resize(strlen(persisted_events.c_str()));
  return persisted_events;
}

// Returns a newline-delimited string containing all breadcrumbs held by the
// BreadcrumbManager. The oldest breadcrumbs are truncated from the string if it
// would be longer than `kMaxDataLength`.
std::string GetEvents() {
  const auto& events = BreadcrumbManager::GetInstance().GetEvents();
  std::vector<std::string> breadcrumbs;
  size_t breadcrumbs_size = 0;
  for (const std::string& event : base::Reversed(events)) {
    // Reduce saved events to only the amount that can be included in a crash
    // report. This allows future events to be appended up to
    // `kPersistedFilesizeInBytes`, reducing the number of resizes needed.
    breadcrumbs_size += event.size() + strlen(kEventSeparator);
    if (breadcrumbs_size >= kMaxDataLength) {
      break;
    }
    breadcrumbs.push_back(event);
  }

  std::reverse(breadcrumbs.begin(), breadcrumbs.end());
  return base::JoinString(breadcrumbs, kEventSeparator) + kEventSeparator;
}

}  // namespace

BreadcrumbPersistentStorageManager::BreadcrumbPersistentStorageManager(
    const base::FilePath& directory,
    base::RepeatingCallback<bool()> is_metrics_enabled_callback,
    base::OnceClosure initialization_done_callback)
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
      FROM_HERE, base::BindOnce(&DoGetStoredEvents, breadcrumbs_file_path_),
      base::BindOnce(&BreadcrumbPersistentStorageManager::Initialize,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(initialization_done_callback)));
}

BreadcrumbPersistentStorageManager::~BreadcrumbPersistentStorageManager() =
    default;

void BreadcrumbPersistentStorageManager::Initialize(
    base::OnceClosure initialization_done_callback,
    const std::string& previous_session_events) {
  breadcrumbs::BreadcrumbManager::GetInstance().SetPreviousSessionEvents(
      base::SplitString(previous_session_events, kEventSeparator,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));
  file_position_ = previous_session_events.length();

  // Write any startup events that have accumulated while waiting for the file
  // position to be set.
  WriteEvents(std::move(initialization_done_callback));
}

void BreadcrumbPersistentStorageManager::Write(const std::string& events,
                                               bool append) {
  if (!CheckForFileConsent()) {
    return;
  }
  if (!append) {
    file_position_ = 0;
  }
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DoWriteEventsToFile, breadcrumbs_file_path_,
                                file_position_.value(), events, append));
  file_position_.value() += events.size();
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

void BreadcrumbPersistentStorageManager::WriteEvents(
    base::OnceClosure done_callback) {
  // No events can be written to the file until the size of existing breadcrumbs
  // is known.
  if (!file_position_) {
    return;
  }

  write_timer_.Stop();

  const base::TimeDelta time_delta_since_last_write =
      base::TimeTicks::Now() - last_written_time_;
  if (time_delta_since_last_write < kMinDelayBetweenWrites) {
    // If an event was just written, delay writing the event to disk in order to
    // limit overhead.
    write_timer_.Start(
        FROM_HERE, kMinDelayBetweenWrites - time_delta_since_last_write,
        base::BindOnce(&BreadcrumbPersistentStorageManager::WriteEvents,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(done_callback)));
    return;
  }

  // If the event does not fit within |kPersistedFilesizeInBytes|, rewrite the
  // file to trim old events.
  if ((file_position_.value() + pending_breadcrumbs_.size())
      // Use >= here instead of > to allow space for \0 to terminate file.
      >= kPersistedFilesizeInBytes) {
    Write(GetEvents(), /*append=*/false);
  } else if (!pending_breadcrumbs_.empty()) {
    // Otherwise, simply append the pending breadcrumbs.
    Write(pending_breadcrumbs_, /*append=*/true);
  }

  // Add `done_callback` to the task runner's task queue, so it runs after any
  // posted `DoWriteEventsToFile()` task has been run.
  task_runner_->PostTask(FROM_HERE, std::move(done_callback));
}

}  // namespace breadcrumbs
