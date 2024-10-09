// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/metrics/file_metrics_provider.h"

#include <stddef.h>

#include <memory>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/ranges_manager.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/persistent_histograms.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace metrics {
namespace {

// These structures provide values used to define how files are opened and
// accessed. It obviates the need for multiple code-paths within several of
// the methods.
struct SourceOptions {
  // The flags to be used to open a file on disk.
  int file_open_flags;

  // The access mode to be used when mapping a file into memory.
  base::MemoryMappedFile::Access memory_mapped_access;

  // Indicates if the file is to be accessed read-only.
  bool is_read_only;
};

// Opening a file typically requires at least these flags.
constexpr int STD_OPEN = base::File::FLAG_OPEN | base::File::FLAG_READ;

constexpr SourceOptions kSourceOptions[] = {
    // SOURCE_HISTOGRAMS_ATOMIC_FILE
    {
        // Ensure that no other process reads this at the same time.
        STD_OPEN | base::File::FLAG_WIN_EXCLUSIVE_READ,
        base::MemoryMappedFile::READ_ONLY,
        true,
    },
    // SOURCE_HISTOGRAMS_ATOMIC_DIR
    {
        // Ensure that no other process reads this at the same time.
        STD_OPEN | base::File::FLAG_WIN_EXCLUSIVE_READ,
        base::MemoryMappedFile::READ_ONLY,
        true,
    },
    // SOURCE_HISTOGRAMS_ACTIVE_FILE
    {
        // Allow writing to the file. This is needed so we can keep track of
        // deltas that have been uploaded (by modifying the file), while the
        // file may still be open by an external process (e.g. Crashpad).
        STD_OPEN | base::File::FLAG_WRITE,
        base::MemoryMappedFile::READ_WRITE,
        false,
    },
};

void DeleteFileWhenPossible(const base::FilePath& path) {
  // Open (with delete) and then immediately close the file by going out of
  // scope. This is the only cross-platform safe way to delete a file that may
  // be open elsewhere, a distinct possibility given the asynchronous nature
  // of the delete task.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_DELETE_ON_CLOSE);
}

}  // namespace

// This structure stores all the information about the sources being monitored
// and their current reporting state.
struct FileMetricsProvider::SourceInfo {
  explicit SourceInfo(const Params& params)
      : type(params.type),
        association(params.association),
        prefs_key(params.prefs_key),
        filter(params.filter),
        max_age(params.max_age),
        max_dir_kib(params.max_dir_kib),
        max_dir_files(params.max_dir_files) {
    switch (type) {
      case SOURCE_HISTOGRAMS_ACTIVE_FILE:
        DCHECK(prefs_key.empty());
        [[fallthrough]];
      case SOURCE_HISTOGRAMS_ATOMIC_FILE:
        path = params.path;
        break;
      case SOURCE_HISTOGRAMS_ATOMIC_DIR:
        directory = params.path;
        break;
    }
  }

  SourceInfo(const SourceInfo&) = delete;
  SourceInfo& operator=(const SourceInfo&) = delete;

  ~SourceInfo() = default;

  struct FoundFile {
    base::FilePath path;
    base::FileEnumerator::FileInfo info;
  };
  using FoundFiles = base::flat_map<base::Time, FoundFile>;

  // How to access this source (file/dir, atomic/active).
  const SourceType type;

  // With what run this source is associated.
  const SourceAssociation association;

  // Where on disk the directory is located. This will only be populated when
  // a directory is being monitored.
  base::FilePath directory;

  // The files found in the above directory, ordered by last-modified.
  std::unique_ptr<FoundFiles> found_files;

  // Where on disk the file is located. If a directory is being monitored,
  // this will be updated for whatever file is being read.
  base::FilePath path;

  // Name used inside prefs to persistent metadata.
  std::string prefs_key;

  // The filter callback for determining what to do with found files.
  FilterCallback filter;

  // The maximum allowed age of a file.
  base::TimeDelta max_age;

  // The maximum allowed bytes in a directory.
  size_t max_dir_kib;

  // The maximum allowed files in a directory.
  size_t max_dir_files;

  // The last-seen time of this source to detect change.
  base::Time last_seen;

  // Indicates if the data has been read out or not.
  bool read_complete = false;

  // Once a file has been recognized as needing to be read, it is mapped
  // into memory and assigned to an |allocator| object.
  std::unique_ptr<base::PersistentHistogramAllocator> allocator;
};

FileMetricsProvider::Params::Params(const base::FilePath& path,
                                    SourceType type,
                                    SourceAssociation association,
                                    std::string_view prefs_key)
    : path(path), type(type), association(association), prefs_key(prefs_key) {}

FileMetricsProvider::Params::~Params() = default;

FileMetricsProvider::FileMetricsProvider(PrefService* local_state)
    : pref_service_(local_state) {
  base::StatisticsRecorder::RegisterHistogramProvider(
      weak_factory_.GetWeakPtr());
}

FileMetricsProvider::~FileMetricsProvider() = default;

void FileMetricsProvider::RegisterSource(const Params& params,
                                         bool metrics_reporting_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ensure that kSourceOptions has been filled for this type.
  DCHECK_GT(std::size(kSourceOptions), static_cast<size_t>(params.type));

  if (!metrics_reporting_enabled) {
    // When metrics reporting is not enabled, existing files should be deleted,
    // since they won't be getting deleted as part of the upload flow.
    if (params.type == SOURCE_HISTOGRAMS_ATOMIC_DIR ||
        params.type == SOURCE_HISTOGRAMS_ATOMIC_FILE) {
      base::ThreadPool::PostTask(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
          params.type == SOURCE_HISTOGRAMS_ATOMIC_DIR
              ? base::GetDeletePathRecursivelyCallback(params.path)
              : base::GetDeleteFileCallback(params.path));
    }
    return;
  }

  std::unique_ptr<SourceInfo> source(new SourceInfo(params));

  // |prefs_key| may be empty if the caller does not wish to persist the
  // state across instances of the program.
  if (pref_service_ && !params.prefs_key.empty()) {
    source->last_seen = pref_service_->GetTime(
        metrics::prefs::kMetricsLastSeenPrefix + source->prefs_key);
  }

  switch (params.association) {
    case ASSOCIATE_CURRENT_RUN:
    case ASSOCIATE_INTERNAL_PROFILE:
    case ASSOCIATE_INTERNAL_PROFILE_SAMPLES_COUNTER:
      sources_to_check_.push_back(std::move(source));
      break;
    case ASSOCIATE_PREVIOUS_RUN:
    case ASSOCIATE_INTERNAL_PROFILE_OR_PREVIOUS_RUN:
      DCHECK_EQ(SOURCE_HISTOGRAMS_ATOMIC_FILE, source->type);
      sources_for_previous_run_.push_back(std::move(source));
      break;
  }
}

// static
void FileMetricsProvider::RegisterSourcePrefs(PrefRegistrySimple* prefs,
                                              std::string_view prefs_key) {
  prefs->RegisterInt64Pref(
      metrics::prefs::kMetricsLastSeenPrefix + std::string(prefs_key), 0);
}

//  static
void FileMetricsProvider::RegisterPrefs(PrefRegistrySimple* prefs) {
  prefs->RegisterListPref(metrics::prefs::kMetricsFileMetricsMetadata);
}

// static
void FileMetricsProvider::RecordAccessResult(AccessResult result) {
  UMA_HISTOGRAM_ENUMERATION("UMA.FileMetricsProvider.AccessResult", result,
                            ACCESS_RESULT_MAX);
}

// static
bool FileMetricsProvider::LocateNextFileInDirectory(SourceInfo* source) {
  DCHECK_EQ(SOURCE_HISTOGRAMS_ATOMIC_DIR, source->type);
  DCHECK(!source->directory.empty());

  // Cumulative directory stats. These will remain zero if the directory isn't
  // scanned but that's okay since any work they would cause to be done below
  // would have been done during the first call where the directory was fully
  // scanned.
  size_t total_size_kib = 0;  // Using KiB allows 4TiB even on 32-bit builds.
  size_t file_count = 0;

  base::Time now_time = base::Time::Now();
  if (!source->found_files) {
    source->found_files = std::make_unique<SourceInfo::FoundFiles>();
    base::FileEnumerator file_iter(source->directory, /*recursive=*/false,
                                   base::FileEnumerator::FILES);
    SourceInfo::FoundFile found_file;

    // Open the directory and find all the files, remembering the last-modified
    // time of each.
    for (found_file.path = file_iter.Next(); !found_file.path.empty();
         found_file.path = file_iter.Next()) {
      found_file.info = file_iter.GetInfo();

      // Ignore directories.
      if (found_file.info.IsDirectory()) {
        continue;
      }

      // Ignore temporary files.
      base::FilePath::CharType first_character =
          found_file.path.BaseName().value().front();
      if (first_character == FILE_PATH_LITERAL('.') ||
          first_character == FILE_PATH_LITERAL('_')) {
        continue;
      }

      // Ignore non-PMA (Persistent Memory Allocator) files.
      if (found_file.path.Extension() !=
          base::PersistentMemoryAllocator::kFileExtension) {
        continue;
      }

      // Process real files.
      total_size_kib += found_file.info.GetSize() >> 10;
      base::Time modified = found_file.info.GetLastModifiedTime();
      if (modified > source->last_seen) {
        source->found_files->emplace(modified, std::move(found_file));
        ++file_count;
      } else {
        // This file has been read. Try to delete it. Ignore any errors because
        // the file may be un-removeable by this process. It could, for example,
        // have been created by a privileged process like setup.exe. Even if it
        // is not removed, it will continue to be ignored bacuse of the older
        // modification time.
        base::DeleteFile(found_file.path);
      }
    }
  }

  // Filter files from the front until one is found for processing.
  bool have_file = false;
  while (!source->found_files->empty()) {
    SourceInfo::FoundFile found =
        std::move(source->found_files->begin()->second);
    source->found_files->erase(source->found_files->begin());

    bool too_many =
        source->max_dir_files > 0 && file_count > source->max_dir_files;
    bool too_big =
        source->max_dir_kib > 0 && total_size_kib > source->max_dir_kib;
    bool too_old =
        source->max_age != base::TimeDelta() &&
        now_time - found.info.GetLastModifiedTime() > source->max_age;
    if (too_many || too_big || too_old) {
      base::DeleteFile(found.path);
      --file_count;
      total_size_kib -= found.info.GetSize() >> 10;
      RecordAccessResult(too_many ? ACCESS_RESULT_TOO_MANY_FILES
                                  : too_big ? ACCESS_RESULT_TOO_MANY_BYTES
                                            : ACCESS_RESULT_TOO_OLD);
      continue;
    }

    AccessResult result = HandleFilterSource(source, found.path);
    if (result == ACCESS_RESULT_SUCCESS) {
      source->path = std::move(found.path);
      have_file = true;
      break;
    }

    // Record the result. Success will be recorded by the caller.
    if (result != ACCESS_RESULT_THIS_PID) {
      RecordAccessResult(result);
    }
  }

  return have_file;
}

// static
void FileMetricsProvider::FinishedWithSource(SourceInfo* source,
                                             AccessResult result) {
  // Different source types require different post-processing.
  switch (source->type) {
    case SOURCE_HISTOGRAMS_ATOMIC_FILE:
    case SOURCE_HISTOGRAMS_ATOMIC_DIR:
      // Done with this file so delete the allocator and its owned file.
      source->allocator.reset();
      // Remove the file if has been recorded. This prevents them from
      // accumulating or also being recorded by different instances of
      // the browser.
      if (result == ACCESS_RESULT_SUCCESS ||
          result == ACCESS_RESULT_NOT_MODIFIED ||
          result == ACCESS_RESULT_MEMORY_DELETED ||
          result == ACCESS_RESULT_TOO_OLD) {
        DeleteFileWhenPossible(source->path);
      }
      break;
    case SOURCE_HISTOGRAMS_ACTIVE_FILE:
      // Keep the allocator open so it doesn't have to be re-mapped each
      // time. This also allows the contents to be merged on-demand.
      break;
  }
}

// static
std::vector<size_t> FileMetricsProvider::CheckAndMergeMetricSourcesOnTaskRunner(
    SourceInfoList* sources) {
  // This method has all state information passed in |sources| and is intended
  // to run on a worker thread rather than the UI thread.
  std::vector<size_t> samples_counts;

  for (std::unique_ptr<SourceInfo>& source : *sources) {
    AccessResult result;
    do {
      result = CheckAndMapMetricSource(source.get());

      // Some results are not reported in order to keep the dashboard clean.
      if (result != ACCESS_RESULT_DOESNT_EXIST &&
          result != ACCESS_RESULT_NOT_MODIFIED &&
          result != ACCESS_RESULT_THIS_PID) {
        RecordAccessResult(result);
      }

      // If there are no files (or no more files) in this source, stop now.
      if (result == ACCESS_RESULT_DOESNT_EXIST)
        break;

      // Mapping was successful. Merge it.
      if (result == ACCESS_RESULT_SUCCESS) {
        // Metrics associated with internal profiles have to be fetched directly
        // so just keep the mapping for use by the main thread.
        if (source->association == ASSOCIATE_INTERNAL_PROFILE)
          break;

        if (source->association == ASSOCIATE_INTERNAL_PROFILE_SAMPLES_COUNTER) {
          samples_counts.push_back(CollectFileMetadataFromSource(source.get()));
        } else {
          size_t histograms_count =
              MergeHistogramDeltasFromSource(source.get());
          if (!source->prefs_key.empty()) {
            base::UmaHistogramCounts1000(
                base::StringPrintf(
                    "UMA.FileMetricsProvider.%s.MergedHistogramsCount",
                    source->prefs_key.c_str()),
                histograms_count);
          }
        }
        DCHECK(source->read_complete);
      }

      // All done with this source.
      FinishedWithSource(source.get(), result);

      // If it's a directory, keep trying until a file is successfully opened.
      // When there are no more files, ACCESS_RESULT_DOESNT_EXIST will be
      // returned and the loop will exit above.
    } while (result != ACCESS_RESULT_SUCCESS && !source->directory.empty());

    // If the set of known files is empty, clear the object so the next run
    // will do a fresh scan of the directory.
    if (source->found_files && source->found_files->empty())
      source->found_files.reset();
  }

  return samples_counts;
}

// This method has all state information passed in |source| and is intended
// to run on a worker thread rather than the UI thread.
// static
FileMetricsProvider::AccessResult FileMetricsProvider::CheckAndMapMetricSource(
    SourceInfo* source) {
  // If source was read, clean up after it.
  if (source->read_complete)
    FinishedWithSource(source, ACCESS_RESULT_SUCCESS);
  source->read_complete = false;
  DCHECK(!source->allocator);

  // If the source is a directory, look for files within it.
  if (!source->directory.empty() && !LocateNextFileInDirectory(source))
    return ACCESS_RESULT_DOESNT_EXIST;

  // Do basic validation on the file metadata.
  base::File::Info info;
  if (!base::GetFileInfo(source->path, &info))
    return ACCESS_RESULT_DOESNT_EXIST;

  if (info.is_directory || info.size == 0)
    return ACCESS_RESULT_INVALID_FILE;

  if (source->last_seen >= info.last_modified)
    return ACCESS_RESULT_NOT_MODIFIED;
  if (source->max_age != base::TimeDelta() &&
      base::Time::Now() - info.last_modified > source->max_age) {
    return ACCESS_RESULT_TOO_OLD;
  }

  // Non-directory files still need to be filtered.
  if (source->directory.empty()) {
    AccessResult result = HandleFilterSource(source, source->path);
    if (result != ACCESS_RESULT_SUCCESS)
      return result;
  }

  // A new file of metrics has been found.
  base::File file(source->path, kSourceOptions[source->type].file_open_flags);
  if (!file.IsValid())
    return ACCESS_RESULT_NO_OPEN;

  // Check that file is writable if that is expected. If a write is attempted
  // on an unwritable memory-mapped file, a SIGBUS will cause a crash.
  const bool read_only = kSourceOptions[source->type].is_read_only;
  if (!read_only) {
    constexpr int kTestSize = 16;
    char header[kTestSize];
    int amount = file.Read(0, header, kTestSize);
    if (amount != kTestSize)
      return ACCESS_RESULT_INVALID_CONTENTS;

    char zeros[kTestSize] = {0};
    file.Write(0, zeros, kTestSize);
    file.Flush();

    // A crash here would be unfortunate as the file would be left invalid
    // and skipped/deleted by later attempts. This is unlikely, however, and
    // the benefit of avoiding crashes from mapping as read/write a file that
    // can't be written more than justifies the risk.

    char check[kTestSize];
    amount = file.Read(0, check, kTestSize);
    if (amount != kTestSize)
      return ACCESS_RESULT_INVALID_CONTENTS;
    if (memcmp(check, zeros, kTestSize) != 0)
      return ACCESS_RESULT_NOT_WRITABLE;

    file.Write(0, header, kTestSize);
    file.Flush();
    amount = file.Read(0, check, kTestSize);
    if (amount != kTestSize)
      return ACCESS_RESULT_INVALID_CONTENTS;
    if (memcmp(check, header, kTestSize) != 0)
      return ACCESS_RESULT_NOT_WRITABLE;
  }

  std::unique_ptr<base::MemoryMappedFile> mapped(new base::MemoryMappedFile());
  if (!mapped->Initialize(std::move(file),
                          kSourceOptions[source->type].memory_mapped_access)) {
    return ACCESS_RESULT_SYSTEM_MAP_FAILURE;
  }

  // Ensure any problems below don't occur repeatedly.
  source->last_seen = info.last_modified;

  // Test the validity of the file contents.
  if (!base::FilePersistentMemoryAllocator::IsFileAcceptable(*mapped,
                                                             read_only)) {
    return ACCESS_RESULT_INVALID_CONTENTS;
  }

  // Map the file and validate it.
  std::unique_ptr<base::FilePersistentMemoryAllocator> memory_allocator =
      std::make_unique<base::FilePersistentMemoryAllocator>(
          std::move(mapped), 0, 0, std::string_view(),
          read_only ? base::FilePersistentMemoryAllocator::kReadOnly
                    : base::FilePersistentMemoryAllocator::kReadWriteExisting);
  if (memory_allocator->GetMemoryState() ==
      base::PersistentMemoryAllocator::MEMORY_DELETED) {
    return ACCESS_RESULT_MEMORY_DELETED;
  }
  if (memory_allocator->IsCorrupt())
    return ACCESS_RESULT_DATA_CORRUPTION;

  // Cache the file data while running in a background thread so that there
  // shouldn't be any I/O when the data is accessed from the main thread.
  // Files with an internal profile, those from previous runs that include
  // a full system profile and are fetched via ProvideIndependentMetrics(),
  // are loaded on a background task and so there's no need to cache the
  // data in advance.
  if (source->association != ASSOCIATE_INTERNAL_PROFILE)
    memory_allocator->Cache();

  // Create an allocator for the mapped file. Ownership passes to the allocator.
  source->allocator = std::make_unique<base::PersistentHistogramAllocator>(
      std::move(memory_allocator));
  // Pass a custom RangesManager so that we do not register the BucketRanges
  // with the global StatisticsRecorder when creating histogram objects using
  // the allocator's underlying data. This avoids unnecessary contention on the
  // global StatisticsRecorder lock.
  // Note: Since RangesManager is not thread safe, this means that |allocator|
  // must be iterated over one thread at a time (i.e., not concurrently). This
  // is the case.
  source->allocator->SetRangesManager(new base::RangesManager());

  // Check that an "independent" file has the necessary information present.
  if (source->association == ASSOCIATE_INTERNAL_PROFILE &&
      !PersistentSystemProfile::GetSystemProfile(
          *source->allocator->memory_allocator(), nullptr)) {
    return ACCESS_RESULT_NO_PROFILE;
  }

  return ACCESS_RESULT_SUCCESS;
}

// static
size_t FileMetricsProvider::MergeHistogramDeltasFromSource(SourceInfo* source) {
  DCHECK(source->allocator);
  base::PersistentHistogramAllocator::Iterator histogram_iter(
      source->allocator.get());

  const bool read_only = kSourceOptions[source->type].is_read_only;
  size_t histogram_count = 0;
  while (true) {
    std::unique_ptr<base::HistogramBase> histogram = histogram_iter.GetNext();
    if (!histogram)
      break;

    if (read_only) {
      source->allocator->MergeHistogramFinalDeltaToStatisticsRecorder(
          histogram.get());
    } else {
      source->allocator->MergeHistogramDeltaToStatisticsRecorder(
          histogram.get());
    }
    ++histogram_count;
  }

  source->read_complete = true;
  DVLOG(1) << "Reported " << histogram_count << " histograms from "
           << source->path.value();
  return histogram_count;
}

// static
void FileMetricsProvider::RecordHistogramSnapshotsFromSource(
    base::HistogramSnapshotManager* snapshot_manager,
    SourceInfo* source,
    base::HistogramBase::Flags required_flags) {
  DCHECK_NE(SOURCE_HISTOGRAMS_ACTIVE_FILE, source->type);

  base::PersistentHistogramAllocator::Iterator histogram_iter(
      source->allocator.get());

  int histogram_count = 0;
  while (true) {
    std::unique_ptr<base::HistogramBase> histogram = histogram_iter.GetNext();
    if (!histogram)
      break;
    if (histogram->HasFlags(required_flags)) {
      snapshot_manager->PrepareFinalDelta(histogram.get());
      ++histogram_count;
    }
  }

  source->read_complete = true;
  DVLOG(1) << "Reported " << histogram_count << " histograms from "
           << source->path.value();
}

FileMetricsProvider::AccessResult FileMetricsProvider::HandleFilterSource(
    SourceInfo* source,
    const base::FilePath& path) {
  if (!source->filter)
    return ACCESS_RESULT_SUCCESS;

  // Alternatively, pass a Params object to the filter like what was originally
  // used to configure the source.
  // Params params(path, source->type, source->association, source->prefs_key);
  FilterAction action = source->filter.Run(path);
  switch (action) {
    case FILTER_PROCESS_FILE:
      // Process the file.
      return ACCESS_RESULT_SUCCESS;

    case FILTER_ACTIVE_THIS_PID:
    // Even the file for the current process has to be touched or its stamp
    // will be less than "last processed" and thus skipped on future runs,
    // even those done by new instances of the browser if a pref key is
    // provided so that the last-uploaded stamp is recorded.
    case FILTER_TRY_LATER: {
      // Touch the file with the current timestamp making it (presumably) the
      // newest file in the directory.
      base::Time now = base::Time::Now();
      base::TouchFile(path, /*accessed=*/now, /*modified=*/now);
      if (action == FILTER_ACTIVE_THIS_PID)
        return ACCESS_RESULT_THIS_PID;
      return ACCESS_RESULT_FILTER_TRY_LATER;
    }

    case FILTER_SKIP_FILE:
      switch (source->type) {
        case SOURCE_HISTOGRAMS_ATOMIC_FILE:
        case SOURCE_HISTOGRAMS_ATOMIC_DIR:
          // Only "atomic" files are deleted (best-effort).
          DeleteFileWhenPossible(path);
          break;
        case SOURCE_HISTOGRAMS_ACTIVE_FILE:
          // File will presumably get modified elsewhere and thus tried again.
          break;
      }
      return ACCESS_RESULT_FILTER_SKIP_FILE;
  }

  // Code never gets here but some compilers don't realize that and so complain
  // that "not all control paths return a value".
  NOTREACHED_IN_MIGRATION();
  return ACCESS_RESULT_SUCCESS;
}

/* static */
bool FileMetricsProvider::ProvideIndependentMetricsOnTaskRunner(
    SourceInfo* source,
    ChromeUserMetricsExtension* uma_proto,
    base::HistogramSnapshotManager* snapshot_manager,
    base::OnceClosure serialize_log_callback) {
  // Include various crash keys about the file/allocator being read so that if
  // there is ever a crash report being dumped while reading its contents, we
  // have some info about its state.
  // TODO(crbug.com/40064026): Clean this up.

  // Useful to know the metadata version of the source (e.g. to know if some
  // fields like memory_state below are up to date).
  SCOPED_CRASH_KEY_NUMBER("PMA", "version",
                          source->allocator->memory_allocator()->version());
  // Useful to know whether the source comes from a crashed session.
  SCOPED_CRASH_KEY_NUMBER(
      "PMA", "memory_state",
      source->allocator->memory_allocator()->GetMemoryState());
  // Useful to know the freeptr as it can help determine if the source comes
  // from a session that crashed due to failing to allocate an object across
  // different pages.
  SCOPED_CRASH_KEY_NUMBER("PMA", "freeptr",
                          source->allocator->memory_allocator()->freeptr());
  SCOPED_CRASH_KEY_BOOL("PMA", "full",
                        source->allocator->memory_allocator()->IsFull());
  SCOPED_CRASH_KEY_BOOL("PMA", "corrupt",
                        source->allocator->memory_allocator()->IsCorrupt());

  SystemProfileProto* system_profile_proto =
      uma_proto->mutable_system_profile();

  if (PersistentSystemProfile::GetSystemProfile(
          *source->allocator->memory_allocator(), system_profile_proto)) {
    system_profile_proto->mutable_stability()->set_from_previous_run(true);
    RecordHistogramSnapshotsFromSource(
        snapshot_manager, source,
        /*required_flags=*/base::HistogramBase::kUmaTargetedHistogramFlag);

    // NOTE: If you are adding anything here, consider also changing
    // MetricsStateMetricsProvider::ProvidePreviousSessionData().

    // Use the client UUID stored in the system profile (if there is one) as the
    // independent log's client ID. Usually, this has no effect, but there are
    // scenarios where the log may have come from a session that had a different
    // client ID than the one currently in use (e.g., client ID was reset due to
    // being detected as a cloned install), so make sure to associate it with
    // the proper one.
    const std::string& client_uuid = system_profile_proto->client_uuid();
    if (!client_uuid.empty()) {
      uma_proto->set_client_id(MetricsLog::Hash(client_uuid));
    }

    // Serialize the log while we are still in the background, instead of on the
    // callback that runs on the main thread.
    std::move(serialize_log_callback).Run();

    return true;
  }

  return false;
}

void FileMetricsProvider::AppendToSamplesCountPref(
    std::vector<size_t> samples_counts) {
  ScopedListPrefUpdate update(pref_service_,
                              metrics::prefs::kMetricsFileMetricsMetadata);
  for (size_t samples_count : samples_counts) {
    update->Append(static_cast<int>(samples_count));
  }
}

// static
size_t FileMetricsProvider::CollectFileMetadataFromSource(SourceInfo* source) {
  base::HistogramBase::Count samples_count = 0;
  base::PersistentHistogramAllocator::Iterator it{source->allocator.get()};
  std::unique_ptr<base::HistogramBase> histogram;
  while ((histogram = it.GetNext()) != nullptr) {
    samples_count += histogram->SnapshotFinalDelta()->TotalCount();
  }
  source->read_complete = true;
  return samples_count;
}

void FileMetricsProvider::ScheduleSourcesCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (sources_to_check_.empty())
    return;

  // Create an independent list of sources for checking. This will be Owned()
  // by the reply call given to the task-runner, to be deleted when that call
  // has returned. It is also passed Unretained() to the task itself, safe
  // because that must complete before the reply runs.
  SourceInfoList* check_list = new SourceInfoList();
  std::swap(sources_to_check_, *check_list);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       // SKIP_ON_SHUTDOWN because the task must be run to completion once
       // started. Since the task may merge metrics from files on disk, the task
       // should be completed so that those files are deleted (to prevent
       // re-merging them in another session, which would cause duplication).
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &FileMetricsProvider::CheckAndMergeMetricSourcesOnTaskRunner,
          base::Unretained(check_list)),
      base::BindOnce(&FileMetricsProvider::RecordSourcesChecked,
                     weak_factory_.GetWeakPtr(), base::Owned(check_list)));
}

void FileMetricsProvider::RecordSourcesChecked(
    SourceInfoList* checked,
    std::vector<size_t> samples_counts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AppendToSamplesCountPref(std::move(samples_counts));

  // Sources that still have an allocator at this point are read/write "active"
  // files that may need their contents merged on-demand. If there is no
  // allocator (not a read/write file) but a read was done on the task-runner,
  // try again immediately to see if more is available (in a directory of
  // files). Otherwise, remember the source for checking again at a later time.
  bool did_read = false;
  for (auto iter = checked->begin(); iter != checked->end();) {
    auto temp = iter++;
    SourceInfo* source = temp->get();
    if (source->read_complete) {
      RecordSourceAsRead(source);
      did_read = true;
    }
    if (source->allocator) {
      if (source->association == ASSOCIATE_INTERNAL_PROFILE) {
        sources_with_profile_.splice(sources_with_profile_.end(), *checked,
                                     temp);
      } else {
        sources_mapped_.splice(sources_mapped_.end(), *checked, temp);
      }
    } else {
      sources_to_check_.splice(sources_to_check_.end(), *checked, temp);
    }
  }

  // If a read was done, schedule another one immediately. In the case of a
  // directory of files, this ensures that all entries get processed. It's
  // done here instead of as a loop in CheckAndMergeMetricSourcesOnTaskRunner
  // so that (a) it gives the disk a rest and (b) testing of individual reads
  // is possible.
  if (did_read)
    ScheduleSourcesCheck();
}

void FileMetricsProvider::DeleteFileAsync(const base::FilePath& path) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       // CONTINUE_ON_SHUTDOWN because files that are scheduled to be deleted
       // asynchronously are not guaranteed to be deleted this session anyway,
       // so no need to block shutdown if the task has already started running.
       // Further, for such files, there are different ways to ensure they won't
       // be consumed again (i.e., prefs).
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(DeleteFileWhenPossible, path));
}

void FileMetricsProvider::RecordSourceAsRead(SourceInfo* source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Persistently record the "last seen" timestamp of the source file to
  // ensure that the file is never read again unless it is modified again.
  if (pref_service_ && !source->prefs_key.empty()) {
    pref_service_->SetTime(
        metrics::prefs::kMetricsLastSeenPrefix + source->prefs_key,
        source->last_seen);
  }
}

void FileMetricsProvider::OnDidCreateMetricsLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Schedule a check to see if there are new metrics to load. If so, they will
  // be reported during the next collection run after this one. The check is run
  // off of a MayBlock() TaskRunner so as to not cause delays on the main UI
  // thread (which is currently where metric collection is done).
  ScheduleSourcesCheck();

  // Clear any data for initial metrics since they're always reported
  // before the first call to this method. It couldn't be released after
  // being reported in RecordInitialHistogramSnapshots because the data
  // will continue to be used by the caller after that method returns. Once
  // here, though, all actions to be done on the data have been completed.
  for (const std::unique_ptr<SourceInfo>& source : sources_for_previous_run_)
    DeleteFileAsync(source->path);
  sources_for_previous_run_.clear();
}

bool FileMetricsProvider::HasIndependentMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !sources_with_profile_.empty() || SimulateIndependentMetrics();
}

void FileMetricsProvider::ProvideIndependentMetrics(
    base::OnceClosure serialize_log_callback,
    base::OnceCallback<void(bool)> done_callback,
    ChromeUserMetricsExtension* uma_proto,
    base::HistogramSnapshotManager* snapshot_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (sources_with_profile_.empty()) {
    std::move(done_callback).Run(false);
    return;
  }

  std::unique_ptr<SourceInfo> source =
      std::move(*sources_with_profile_.begin());
  sources_with_profile_.pop_front();
  SourceInfo* source_ptr = source.get();
  DCHECK(source->allocator);

  // Do the actual work as a background task.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       // CONTINUE_ON_SHUTDOWN because the work done is only useful once the
       // reply task is run (and there are no side effects). So, no need to
       // block shutdown since the reply task won't be run anyway.
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          &FileMetricsProvider::ProvideIndependentMetricsOnTaskRunner,
          source_ptr, uma_proto, snapshot_manager,
          std::move(serialize_log_callback)),
      base::BindOnce(&FileMetricsProvider::ProvideIndependentMetricsCleanup,
                     weak_factory_.GetWeakPtr(), std::move(done_callback),
                     std::move(source)));
}

void FileMetricsProvider::ProvideIndependentMetricsCleanup(
    base::OnceCallback<void(bool)> done_callback,
    std::unique_ptr<SourceInfo> source,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Regardless of whether this source was successfully recorded, it is
  // never read again.
  source->read_complete = true;
  RecordSourceAsRead(source.get());
  sources_to_check_.push_back(std::move(source));
  ScheduleSourcesCheck();

  std::move(done_callback).Run(success);
}

bool FileMetricsProvider::HasPreviousSessionData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check all sources for previous run to see if they need to be read.
  for (auto iter = sources_for_previous_run_.begin();
       iter != sources_for_previous_run_.end();) {
    auto temp = iter++;
    SourceInfo* source = temp->get();

    // This would normally be done on a background I/O thread but there
    // hasn't been a chance to run any at the time this method is called.
    // Do the check in-line.
    AccessResult result = CheckAndMapMetricSource(source);
    UMA_HISTOGRAM_ENUMERATION("UMA.FileMetricsProvider.InitialAccessResult",
                              result, ACCESS_RESULT_MAX);

    // If it couldn't be accessed, remove it from the list. There is only ever
    // one chance to record it so no point keeping it around for later. Also
    // mark it as having been read since uploading it with a future browser
    // run would associate it with the then-previous run which would no longer
    // be the run from which it came.
    if (result != ACCESS_RESULT_SUCCESS) {
      DCHECK(!source->allocator);
      RecordSourceAsRead(source);
      DeleteFileAsync(source->path);
      sources_for_previous_run_.erase(temp);
      continue;
    }

    DCHECK(source->allocator);

    // If the source should be associated with an existing internal profile,
    // move it to |sources_with_profile_| for later upload.
    if (source->association == ASSOCIATE_INTERNAL_PROFILE_OR_PREVIOUS_RUN) {
      if (PersistentSystemProfile::HasSystemProfile(
              *source->allocator->memory_allocator())) {
        sources_with_profile_.splice(sources_with_profile_.end(),
                                     sources_for_previous_run_, temp);
      }
    }
  }

  return !sources_for_previous_run_.empty();
}

void FileMetricsProvider::RecordInitialHistogramSnapshots(
    base::HistogramSnapshotManager* snapshot_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const std::unique_ptr<SourceInfo>& source : sources_for_previous_run_) {
    // The source needs to have an allocator attached to it in order to read
    // histograms out of it.
    DCHECK(!source->read_complete);
    DCHECK(source->allocator);

    // Dump all stability histograms contained within the source to the
    // snapshot-manager.
    RecordHistogramSnapshotsFromSource(
        snapshot_manager, source.get(),
        /*required_flags=*/base::HistogramBase::kUmaStabilityHistogramFlag);

    // Update the last-seen time so it isn't read again unless it changes.
    RecordSourceAsRead(source.get());
  }
}

void FileMetricsProvider::MergeHistogramDeltas(
    bool async,
    base::OnceClosure done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40213327): Consider if this work can be done asynchronously.
  for (std::unique_ptr<SourceInfo>& source : sources_mapped_) {
    MergeHistogramDeltasFromSource(source.get());
  }
  std::move(done_callback).Run();
}

bool FileMetricsProvider::SimulateIndependentMetrics() {
  if (!pref_service_->HasPrefPath(
          metrics::prefs::kMetricsFileMetricsMetadata)) {
    return false;
  }

  ScopedListPrefUpdate list_pref(pref_service_,
                                 metrics::prefs::kMetricsFileMetricsMetadata);
  base::Value::List& list_value = list_pref.Get();
  if (list_value.empty())
    return false;

  size_t count = pref_service_->GetInteger(
      metrics::prefs::kStabilityFileMetricsUnsentSamplesCount);
  pref_service_->SetInteger(
      metrics::prefs::kStabilityFileMetricsUnsentSamplesCount,
      list_value[0].GetInt() + count);
  pref_service_->SetInteger(
      metrics::prefs::kStabilityFileMetricsUnsentFilesCount,
      list_value.size() - 1);
  list_value.erase(list_value.begin());

  return true;
}

}  // namespace metrics
