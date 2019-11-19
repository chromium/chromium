// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/watcher_metrics_provider_win.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/win/registry.h"
#include "components/browser_watcher/features.h"
#include "components/browser_watcher/postmortem_report_collector.h"
#include "components/browser_watcher/stability_paths.h"
#include "components/metrics/system_session_analyzer_win.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"

namespace browser_watcher {

namespace {

// Process ID APIs on Windows talk in DWORDs, whereas for string formatting
// and parsing, this code uses int. In practice there are no process IDs with
// the high bit set on Windows, so there's no danger of overflow if this is
// done consistently.
static_assert(sizeof(DWORD) == sizeof(int),
              "process ids are expected to be no larger than int");

// This function does soft matching on the PID recorded in the key only.
// Due to PID reuse, the possibility exists that the process that's now live
// with the given PID is not the same process the data was recorded for.
// This doesn't matter for the purpose, as eventually the data will be
// scavenged and reported.
bool IsDeadProcess(base::StringPiece16 key_or_value_name) {
  // Truncate the input string to the first occurrence of '-', if one exists.
  size_t num_end = key_or_value_name.find(L'-');
  if (num_end != base::StringPiece16::npos)
    key_or_value_name = key_or_value_name.substr(0, num_end);

  // Convert to the numeric PID.
  int pid = 0;
  if (!base::StringToInt(key_or_value_name, &pid) || pid == 0)
    return true;

  // This is a very inexpensive check for the common case of our own PID.
  if (static_cast<base::ProcessId>(pid) == base::GetCurrentProcId())
    return false;

  // The process is not our own - see whether a process with this PID exists.
  // This is more expensive than the above check, but should also be very rare,
  // as this only happens more than once for a given PID if a user is running
  // multiple Chrome instances concurrently.
  base::Process process =
      base::Process::Open(static_cast<base::ProcessId>(pid));
  if (process.IsValid()) {
    // The fact that it was possible to open the process says it's live.
    return false;
  }

  return true;
}

void RecordExitCodes(const base::string16& registry_path) {
  base::win::RegKey regkey(HKEY_CURRENT_USER,
                           registry_path.c_str(),
                           KEY_QUERY_VALUE | KEY_SET_VALUE);
  if (!regkey.Valid())
    return;

  size_t num = regkey.GetValueCount();
  if (num == 0)
    return;
  std::vector<base::string16> to_delete;

  // Record the exit codes in a sparse stability histogram, as the range of
  // values used to report failures is large.
  base::HistogramBase* exit_code_histogram =
      base::SparseHistogram::FactoryGet(
          WatcherMetricsProviderWin::kBrowserExitCodeHistogramName,
          base::HistogramBase::kUmaStabilityHistogramFlag);

  for (size_t i = 0; i < num; ++i) {
    base::string16 name;
    if (regkey.GetValueNameAt(static_cast<int>(i), &name) == ERROR_SUCCESS) {
      DWORD exit_code = 0;
      if (regkey.ReadValueDW(name.c_str(), &exit_code) == ERROR_SUCCESS) {
        // Do not report exit codes for processes that are still live,
        // notably for our own process.
        if (exit_code != STILL_ACTIVE || IsDeadProcess(name)) {
          to_delete.push_back(name);
          exit_code_histogram->Add(exit_code);
        }
      }
    }
  }

  // Delete the values reported above.
  for (size_t i = 0; i < to_delete.size(); ++i)
    regkey.DeleteValue(to_delete[i].c_str());
}

void DeleteAllValues(base::win::RegKey* key) {
  DCHECK(key);

  while (key->GetValueCount() != 0) {
    base::string16 value_name;
    LONG res = key->GetValueNameAt(0, &value_name);
    if (res != ERROR_SUCCESS) {
      DVLOG(1) << "Failed to get value name " << res;
      return;
    }

    res = key->DeleteValue(value_name.c_str());
    if (res != ERROR_SUCCESS) {
      DVLOG(1) << "Failed to delete value " << value_name;
      return;
    }
  }
}

// Called from the blocking pool when metrics reporting is disabled, as there
// may be a sizable stash of data to delete.
void DeleteExitCodeRegistryKey(const base::string16& registry_path) {
  CHECK_NE(L"", registry_path);

  base::win::RegKey key;
  LONG res = key.Open(HKEY_CURRENT_USER, registry_path.c_str(),
                      KEY_QUERY_VALUE | KEY_SET_VALUE);
  if (res == ERROR_SUCCESS) {
    DeleteAllValues(&key);
    res = key.DeleteEmptyKey(L"");
  }
  if (res != ERROR_FILE_NOT_FOUND && res != ERROR_SUCCESS)
    DVLOG(1) << "Failed to delete exit code key " << registry_path;
}

enum CollectionInitializationStatus {
  INIT_SUCCESS = 0,
  UNKNOWN_DIR = 1,
  GET_STABILITY_FILE_PATH_FAILED = 2,
  CRASHPAD_DATABASE_INIT_FAILED = 3,
  INIT_STATUS_MAX = 4
};

void LogCollectionInitStatus(CollectionInitializationStatus status) {
  base::UmaHistogramEnumeration("ActivityTracker.Collect.InitStatus", status,
                                INIT_STATUS_MAX);
}

// Returns a task runner appropriate for running background tasks that perform
// file I/O.
scoped_refptr<base::TaskRunner> CreateBackgroundTaskRunner() {
  return base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

}  // namespace

const char WatcherMetricsProviderWin::kBrowserExitCodeHistogramName[] =
    "Stability.BrowserExitCodes";

WatcherMetricsProviderWin::WatcherMetricsProviderWin(
    const base::string16& registry_path,
    const base::FilePath& user_data_dir,
    const base::FilePath& crash_dir,
    const GetExecutableDetailsCallback& exe_details_cb)
    : recording_enabled_(false),
      cleanup_scheduled_(false),
      registry_path_(registry_path),
      user_data_dir_(user_data_dir),
      crash_dir_(crash_dir),
      exe_details_cb_(exe_details_cb),
      task_runner_(CreateBackgroundTaskRunner()) {}

WatcherMetricsProviderWin::~WatcherMetricsProviderWin() {
}

void WatcherMetricsProviderWin::OnRecordingEnabled() {
  recording_enabled_ = true;
}

void WatcherMetricsProviderWin::OnRecordingDisabled() {
  if (!recording_enabled_ && !cleanup_scheduled_) {
    // When metrics reporting is disabled, the providers get an
    // OnRecordingDisabled notification at startup. Use that first notification
    // to issue the cleanup task. Runs in the background because interacting
    // with the registry can block.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DeleteExitCodeRegistryKey, registry_path_));

    cleanup_scheduled_ = true;
  }
}

void WatcherMetricsProviderWin::ProvideStabilityMetrics(
    metrics::SystemProfileProto* /* system_profile_proto */) {
  // Note that if there are multiple instances of Chrome running in the same
  // user account, there's a small race that will double-report the exit codes
  // from both/multiple instances. This ought to be vanishingly rare and will
  // only manifest as low-level "random" noise. To work around this it would be
  // necessary to implement some form of global locking, which is not worth it
  // here.
  RecordExitCodes(registry_path_);
}

void WatcherMetricsProviderWin::AsyncInit(const base::Closure& done_callback) {
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WatcherMetricsProviderWin::CollectPostmortemReportsImpl,
                     weak_ptr_factory_.GetWeakPtr()),
      done_callback);
}

// TODO(manzagop): consider mechanisms for partial collection if this is to be
//     used on a critical path.
void WatcherMetricsProviderWin::CollectPostmortemReportsImpl() {
  SCOPED_UMA_HISTOGRAM_TIMER("ActivityTracker.Collect.TotalTime");

  bool is_stability_debugging_on =
      base::FeatureList::IsEnabled(browser_watcher::kStabilityDebuggingFeature);
  if (!is_stability_debugging_on) {
    return;  // TODO(manzagop): scan for possible data to delete?
  }

  if (user_data_dir_.empty() || crash_dir_.empty()) {
    LogCollectionInitStatus(UNKNOWN_DIR);
    return;
  }

  // Determine which files to harvest.
  base::FilePath stability_dir = GetStabilityDir(user_data_dir_);

  base::FilePath current_stability_file;
  if (!GetStabilityFileForProcess(base::Process::Current(), user_data_dir_,
                                  &current_stability_file)) {
    LogCollectionInitStatus(GET_STABILITY_FILE_PATH_FAILED);
    return;
  }
  const std::set<base::FilePath>& excluded_stability_files = {
      current_stability_file};

  std::vector<base::FilePath> stability_files = GetStabilityFiles(
      stability_dir, GetStabilityFilePattern(), excluded_stability_files);
  base::UmaHistogramCounts100("ActivityTracker.Collect.StabilityFileCount",
                              stability_files.size());

  // If postmortem collection is disabled, delete the files.
  const bool should_collect = base::GetFieldTrialParamByFeatureAsBool(
      browser_watcher::kStabilityDebuggingFeature,
      browser_watcher::kCollectPostmortemParam, false);

  // Create a database. Note: Chrome already has a g_database in crashpad.cc but
  // it has internal linkage. Create a new one.
  std::unique_ptr<crashpad::CrashReportDatabase> crashpad_database;
  if (should_collect) {
    crashpad_database =
        crashpad::CrashReportDatabase::InitializeWithoutCreating(crash_dir_);
    if (!crashpad_database) {
      LOG(ERROR) << "Failed to initialize a CrashPad database.";
      LogCollectionInitStatus(CRASHPAD_DATABASE_INIT_FAILED);
      // Note: continue to processing the files anyway.
    }
  }

  // Note: this is logged even when Crashpad database initialization fails.
  LogCollectionInitStatus(INIT_SUCCESS);

  const size_t kSystemSessionsToInspect = 5U;
  metrics::SystemSessionAnalyzer analyzer(kSystemSessionsToInspect);

  if (should_collect) {
    base::string16 product_name, version_number, channel_name;
    exe_details_cb_.Run(&product_name, &version_number, &channel_name);
    PostmortemReportCollector collector(
        base::UTF16ToUTF8(product_name), base::UTF16ToUTF8(version_number),
        base::UTF16ToUTF8(channel_name), crashpad_database.get(), &analyzer);
    collector.Process(stability_files);
  } else {
    PostmortemReportCollector collector(&analyzer);
    collector.Process(stability_files);
  }
}

}  // namespace browser_watcher
