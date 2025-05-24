// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_FILE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_FILE_METRICS_PROVIDER_H_

#include <stddef.h>

#include <list>
#include <memory>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/metrics/metrics_provider.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {

// FileMetricsProvider gathers and logs histograms written to files on disk.
// Any number of files can be registered and will be polled once per upload
// cycle (at startup and periodically thereafter -- about every 30 minutes
// for desktop) for data to send.
class FileMetricsProvider : public MetricsProvider,
                            public base::StatisticsRecorder::HistogramProvider {
 public:
  struct Params;

  enum SourceType {
    // "Atomic" files are a collection of histograms that are written
    // completely in a single atomic operation (typically a write followed
    // by an atomic rename) and the file is never updated again except to
    // be replaced by a completely new set of histograms. This is the only
    // option that can be used if the file is not writeable by *this*
    // process. Once the file has been read, an attempt will be made to
    // delete it thus providing some measure of safety should different
    // instantiations (such as by different users of a system-level install)
    // try to read it. In case the delete operation fails, this class
    // persistently tracks the last-modified time of the file so it will
    // not be read a second time.
    SOURCE_HISTOGRAMS_ATOMIC_FILE,

    // A directory of atomic PMA files. This handles a directory in which
    // files of metrics are atomically added. Only files ending with ".pma"
    // will be read. They are read according to their last-modified time and
    // never read more that once (unless they change). Only one file will
    // be read per reporting cycle. Filenames that start with a dot (.) or
    // an underscore (_) are ignored so temporary files (perhaps created by
    // the ImportantFileWriter) will not get read. Files that have been
    // read will be attempted to be deleted; should those files not be
    // deletable by this process, it is the reponsibility of the producer
    // to keep the directory pruned in some manner. Added files must have a
    // timestamp later (not the same or earlier) than the newest file that
    // already exists or it may be assumed to have been already uploaded.
    SOURCE_HISTOGRAMS_ATOMIC_DIR,

    // "Active" files may be open by one or more other processes and updated
    // at any time with new samples or new histograms. Such files may also be
    // inactive for any period of time only to be opened again and have new
    // data written to them. The file should probably never be deleted because
    // there would be no guarantee that the data has been reported.
    SOURCE_HISTOGRAMS_ACTIVE_FILE,
  };

  enum SourceAssociation {
    // Associates the metrics in the file with the current run of the browser.
    // The reporting will take place as part of the normal logging of
    // histograms.
    ASSOCIATE_CURRENT_RUN,

    // Associates the metrics in the file with the previous run of the browesr.
    // The reporting will take place as part of the "stability" histograms.
    // This is important when metrics are dumped as part of a crash of the
    // previous run. This can only be used with FILE_HISTOGRAMS_ATOMIC.
    ASSOCIATE_PREVIOUS_RUN,

    // Associates the metrics in the file with the a profile embedded in the
    // same file. The reporting will take place at a convenient time after
    // startup when the browser is otherwise idle. If there is no embedded
    // system profile, these metrics will be lost.
    ASSOCIATE_INTERNAL_PROFILE,

    // Like above but fall back to ASSOCIATE_PREVIOUS_RUN if there is no
    // embedded profile. This has a small cost during startup as that is
    // when previous-run metrics are sent so the file has be checked at
    // that time even though actual transfer will be delayed if an
    // embedded profile is found.
    ASSOCIATE_INTERNAL_PROFILE_OR_PREVIOUS_RUN,

    // Used to only record the metadata of |ASSOCIATE_INTERNAL_PROFILE| but not
    // merge the metrics. Instead, write metadata such as the samples count etc,
    // to prefs then delete file. To precisely simulate the
    // |ASSOCIATE_INTERNAL_PROFILE| behavior, one file record will be read out
    // and added to the stability prefs each time the metrics service requests
    // the |ASSOCIATE_INTERNAL_PROFILE| source metrics. Finally, the results
    // will be recoreded as stability metrics in the next run.
    ASSOCIATE_INTERNAL_PROFILE_SAMPLES_COUNTER,
  };

  enum FilterAction {
    // Process this file normally.
    FILTER_PROCESS_FILE,

    // This file is the active metrics file for the current process.  Don't
    // do anything with it. This is effectively "try later" but isn't
    // added to the results histogram because the file has to be ignored
    // throughout the life of the browser and that skews the distribution.
    FILTER_ACTIVE_THIS_PID,

    // Try again. This could happen within milliseconds or minutes but no other
    // files from the same source will get processed in between. The process
    // must have permission to "touch" the file and alter its last-modified
    // time because files are always processed in order of those stamps.
    FILTER_TRY_LATER,

    // Skip this file. This file will not be processed until it has changed
    // (i.e. had its last-modifided time updated). If it is "atomic", an
    // attempt will be made to delete it.
    FILTER_SKIP_FILE,
  };

  // A "filter" can be defined to determine what to do on a per-file basis.
  // This is called only after a file has been found to be the next one to
  // be processed so it's okay if filter calls are relatively expensive.
  // Calls are made on a background thread of low-priority and capable of
  // doing I/O.
  using FilterCallback =
      base::RepeatingCallback<FilterAction(const base::FilePath& path)>;

  // Parameters for RegisterSource, defined as a structure to allow new
  // ones to be added (with default values) that doesn't require changes
  // to all call sites.
  struct Params {
    Params(const base::FilePath& path,
           SourceType type,
           SourceAssociation association,
           std::string_view prefs_key = std::string_view());

    ~Params();

    // The standard parameters, set during construction.
    const base::FilePath path;
    const SourceType type;
    const SourceAssociation association;
    const std::string_view prefs_key;

    // Other parameters that can be set after construction.
    FilterCallback filter;       // Run-time check for what to do with file.
    base::TimeDelta max_age;     // Maximum age of a file (0=unlimited).
    size_t max_dir_kib = 0;      // Maximum bytes in a directory (0=inf).
    size_t max_dir_files = 100;  // Maximum files in a directory (0=inf).
  };

  explicit FileMetricsProvider(PrefService* local_state);

  FileMetricsProvider(const FileMetricsProvider&) = delete;
  FileMetricsProvider& operator=(const FileMetricsProvider&) = delete;

  ~FileMetricsProvider() override;

  // Indicates a file or directory to be monitored and how the file or files
  // within that directory are used. Because some metadata may need to persist
  // across process restarts, preferences entries are used based on the
  // |prefs_key| name. Call RegisterSourcePrefs() with the same name to create
  // the necessary keys in advance. Set |prefs_key| empty (nullptr will work) if
  // no persistence is required. ACTIVE files shouldn't have a pref key as
  // they update internal state about what has been previously sent.
  // If `metrics_reporting_enabled` is false, the associated file or directory
  // is deleted (except for ACTIVE files).
  void RegisterSource(const Params& params, bool metrics_reporting_enabled);

  // Registers all necessary preferences for maintaining persistent state
  // about a monitored file across process restarts. The |prefs_key| is
  // typically the filename.
  static void RegisterSourcePrefs(PrefRegistrySimple* prefs,
                                  std::string_view prefs_key);

  static void RegisterPrefs(PrefRegistrySimple* prefs);

 private:
  friend class FileMetricsProviderTest;
  friend class TestFileMetricsProvider;

  // The different results that can occur accessing a file.
  enum AccessResult {
    // File was successfully mapped.
    ACCESS_RESULT_SUCCESS,

    // File does not exist.
    ACCESS_RESULT_DOESNT_EXIST,

    // File exists but not modified since last read.
    ACCESS_RESULT_NOT_MODIFIED,

    // File is not valid: is a directory or zero-size.
    ACCESS_RESULT_INVALID_FILE,

    // System could not map file into memory.
    ACCESS_RESULT_SYSTEM_MAP_FAILURE,

    // File had invalid contents.
    ACCESS_RESULT_INVALID_CONTENTS,

    // File could not be opened.
    ACCESS_RESULT_NO_OPEN,

    // File contents were internally deleted.
    ACCESS_RESULT_MEMORY_DELETED,

    // File is scheduled to be tried again later.
    ACCESS_RESULT_FILTER_TRY_LATER,

    // File was skipped according to filtering rules.
    ACCESS_RESULT_FILTER_SKIP_FILE,

    // File was skipped because it exceeds the maximum age.
    ACCESS_RESULT_TOO_OLD,

    // File was skipped because too many files in directory.
    ACCESS_RESULT_TOO_MANY_FILES,

    // File was skipped because too many bytes in directory.
    ACCESS_RESULT_TOO_MANY_BYTES,

    // The file was skipped because it's being written by this process.
    ACCESS_RESULT_THIS_PID,

    // The file had no embedded system profile.
    ACCESS_RESULT_NO_PROFILE,

    // The file had internal data corruption.
    ACCESS_RESULT_DATA_CORRUPTION,

    // The file is not writable when it should be.
    ACCESS_RESULT_NOT_WRITABLE,

    ACCESS_RESULT_MAX
  };

  // Information about sources being monitored; defined and used exclusively
  // inside the .cc file.
  struct SourceInfo;
  using SourceInfoList = std::list<std::unique_ptr<SourceInfo>>;

  // Records an access result in a histogram.
  static void RecordAccessResult(AccessResult result);

  // Looks for the next file to read within a directory. Returns true if a
  // file was found. This is part of CheckAndMapNewMetricSourcesOnTaskRunner
  // and so runs on an thread capable of I/O. The |source| structure will
  // be internally updated to indicate the next file to be read.
  static bool LocateNextFileInDirectory(SourceInfo* source);

  // Handles the completion of a source.
  static void FinishedWithSource(SourceInfo* source, AccessResult result);

  // Checks a list of sources (on a task-runner allowed to do I/O) and merge
  // any data found within them.
  // Returns a list of histogram sample counts for sources of type
  // ASSOCIATE_INTERNAL_PROFILE_SAMPLES_COUNTER that were processed.
  static std::vector<size_t> CheckAndMergeMetricSourcesOnTaskRunner(
      SourceInfoList* sources);

  // Checks a single source and maps it into memory.
  static AccessResult CheckAndMapMetricSource(SourceInfo* source);

  // Merges all of the histograms from a |source| to the StatisticsRecorder.
  // Returns the number of histograms merged.
  static size_t MergeHistogramDeltasFromSource(SourceInfo* source);

  // Records all histograms from a given source via a snapshot-manager. Only the
  // histograms that have |required_flags| will be recorded.
  static void RecordHistogramSnapshotsFromSource(
      base::HistogramSnapshotManager* snapshot_manager,
      SourceInfo* source,
      base::HistogramBase::Flags required_flags);

  // Calls source filter (if any) and returns the desired action.
  static AccessResult HandleFilterSource(SourceInfo* source,
                                         const base::FilePath& path);

  // The part of ProvideIndependentMetrics that runs as a background task.
  static bool ProvideIndependentMetricsOnTaskRunner(
      SourceInfo* source,
      ChromeUserMetricsExtension* uma_proto,
      base::HistogramSnapshotManager* snapshot_manager,
      base::OnceClosure serialize_log_callback);

  // Collects the metadata of the |source|.
  // Returns the number of histogram samples from that source.
  static size_t CollectFileMetadataFromSource(SourceInfo* source);

  // Appends the samples count to pref on UI thread.
  void AppendToSamplesCountPref(std::vector<size_t> samples_count);

  // Creates a task to check all monitored sources for updates.
  void ScheduleSourcesCheck();

  // Takes a list of sources checked by an external task and determines what
  // to do with each. Virtual for testing.
  virtual void RecordSourcesChecked(SourceInfoList* checked,
                                    std::vector<size_t> samples_counts);

  // Schedules the deletion of a file in the background using the task-runner.
  void DeleteFileAsync(const base::FilePath& path);

  // Updates the persistent state information to show a source as being read.
  void RecordSourceAsRead(SourceInfo* source);

  // metrics::MetricsProvider:
  void OnDidCreateMetricsLog() override;
  bool HasIndependentMetrics() override;
  void ProvideIndependentMetrics(
      base::OnceClosure serialize_log_callback,
      base::OnceCallback<void(bool)> done_callback,
      ChromeUserMetricsExtension* uma_proto,
      base::HistogramSnapshotManager* snapshot_manager) override;
  bool HasPreviousSessionData() override;
  void RecordInitialHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager) override;

  // base::StatisticsRecorder::HistogramProvider:
  void MergeHistogramDeltas(bool async,
                            base::OnceClosure done_callback) override;

  // The part of ProvideIndependentMetrics that runs after background task.
  void ProvideIndependentMetricsCleanup(
      base::OnceCallback<void(bool)> done_callback,
      std::unique_ptr<SourceInfo> source,
      bool success);

  // Simulates the independent metrics to read the first item from
  // kMetricsBrowserMetricsMetadata and updates the stability prefs accordingly,
  // return true if the pref isn't empty.
  bool SimulateIndependentMetrics();

  // A list of sources not currently active that need to be checked for changes.
  SourceInfoList sources_to_check_;

  // A list of currently active sources to be merged when required.
  SourceInfoList sources_mapped_;

  // A list of currently active sources to be merged when required.
  SourceInfoList sources_with_profile_;

  // A list of sources for a previous run. These are held separately because
  // they are not subject to the periodic background checking that handles
  // metrics for the current run.
  SourceInfoList sources_for_previous_run_;

  // The preferences-service used to store persistent state about sources.
  raw_ptr<PrefService> pref_service_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<FileMetricsProvider> weak_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_FILE_METRICS_PROVIDER_H_
