// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_inspector.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/win/conflicts/module_info_util.h"
#include "chrome/browser/win/util_win_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"

namespace {

// The maximum amount of time a stale entry is kept in the cache before it is
// deleted.
constexpr base::TimeDelta kMaxEntryAge = base::TimeDelta::FromDays(30);

constexpr int kConnectionErrorRetryCount = 10;

StringMapping GetPathMapping() {
  return GetEnvironmentVariablesMapping({
      L"LOCALAPPDATA",
      L"ProgramFiles",
      L"ProgramData",
      L"USERPROFILE",
      L"SystemRoot",
      L"TEMP",
      L"TMP",
      L"CommonProgramFiles",
  });
}

void ReportConnectionError(bool value) {
  base::UmaHistogramBoolean("Windows.InspectModule.ConnectionError", value);
}

// Reads the inspection results cache and records the result in UMA.
InspectionResultsCache ReadInspectionResultsCacheOnBackgroundSequence(
    const base::FilePath& file_path) {
  InspectionResultsCache inspection_results_cache;

  uint32_t min_time_stamp =
      CalculateTimeStamp(base::Time::Now() - kMaxEntryAge);
  ReadCacheResult read_result = ReadInspectionResultsCache(
      file_path, min_time_stamp, &inspection_results_cache);
  base::UmaHistogramEnumeration("Windows.ModuleInspector.ReadCacheResult",
                                read_result);

  return inspection_results_cache;
}

// Writes the inspection results cache to disk and records the result in UMA.
void WriteInspectionResultCacheOnBackgroundSequence(
    const base::FilePath& file_path,
    const InspectionResultsCache& inspection_results_cache) {
  bool succeeded =
      WriteInspectionResultsCache(file_path, inspection_results_cache);
  base::UmaHistogramBoolean("Windows.ModuleInspector.WriteCacheResult",
                            succeeded);
}

}  // namespace

// static
constexpr base::Feature ModuleInspector::kDisableBackgroundModuleInspection;

// static
constexpr base::Feature ModuleInspector::kWinOOPInspectModuleFeature;

// static
constexpr base::TimeDelta ModuleInspector::kFlushInspectionResultsTimerTimeout;

ModuleInspector::ModuleInspector(
    const OnModuleInspectedCallback& on_module_inspected_callback)
    : on_module_inspected_callback_(on_module_inspected_callback),
      is_after_startup_(false),
      inspection_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      path_mapping_(GetPathMapping()),
      cache_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      inspection_results_cache_read_(false),
      flush_inspection_results_timer_(
          FROM_HERE,
          kFlushInspectionResultsTimerTimeout,
          base::BindRepeating(
              &ModuleInspector::MaybeUpdateInspectionResultsCache,
              base::Unretained(this))),
      has_new_inspection_results_(false),
      connection_error_retry_count_(kConnectionErrorRetryCount),
      background_inspection_disabled_(
          base::FeatureList::IsEnabled(kDisableBackgroundModuleInspection)) {
  // Use BEST_EFFORT as those will only run after startup is finished.
  content::BrowserThread::PostBestEffortTask(
      FROM_HERE, base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&ModuleInspector::OnStartupFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

ModuleInspector::~ModuleInspector() = default;

void ModuleInspector::AddModule(const ModuleInfoKey& module_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool was_queue_empty = queue_.empty();

  queue_.push(module_key);

  // If the queue was empty before adding the current module, then the
  // inspection must be started.
  if (inspection_results_cache_read_ && was_queue_empty)
    StartInspectingModule();
}

void ModuleInspector::IncreaseInspectionPriority() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Create a task runner with higher priority so that future inspections are
  // done faster.
  inspection_task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  // Assume startup is finished to immediately begin inspecting modules.
  OnStartupFinished();

  // Special case where this instance could be ready to start inspecting but
  // wasn't because background inspection was disabled.
  if (background_inspection_disabled_ && inspection_results_cache_read_ &&
      !queue_.empty()) {
    background_inspection_disabled_ = false;
    StartInspectingModule();
  }
}

bool ModuleInspector::IsIdle() {
  return queue_.empty();
}

void ModuleInspector::OnModuleDatabaseIdle() {
  MaybeUpdateInspectionResultsCache();
}

// static
base::FilePath ModuleInspector::GetInspectionResultsCachePath() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return base::FilePath();

  return user_data_dir.Append(L"Module Info Cache");
}

void ModuleInspector::SetModuleInspectionResultForTesting(
    const ModuleInfoKey& module_key,
    ModuleInspectionResult inspection_result) {
  AddInspectionResultToCache(module_key, inspection_result,
                             &inspection_results_cache_);
}

void ModuleInspector::EnsureUtilWinServiceBound() {
  DCHECK(base::FeatureList::IsEnabled(kWinOOPInspectModuleFeature));

  if (test_remote_util_win_ || remote_util_win_)
    return;

  remote_util_win_ = LaunchUtilWinServiceInstance();
  remote_util_win_.reset_on_idle_timeout(base::TimeDelta::FromSeconds(5));
  remote_util_win_.set_disconnect_handler(
      base::BindOnce(&ModuleInspector::OnUtilWinServiceConnectionError,
                     base::Unretained(this)));

  // Emit a false value to the connection error histogram to serve as a
  // baseline.
  ReportConnectionError(false);
}

void ModuleInspector::OnStartupFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This function will be invoked twice if IncreaseInspectionPriority() is
  // called.
  if (is_after_startup_)
    return;

  is_after_startup_ = true;

  // Read the inspection cache now that it won't affect startup.
  base::PostTaskAndReplyWithResult(
      cache_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ReadInspectionResultsCacheOnBackgroundSequence,
                     GetInspectionResultsCachePath()),
      base::BindOnce(&ModuleInspector::OnInspectionResultsCacheRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ModuleInspector::OnInspectionResultsCacheRead(
    InspectionResultsCache inspection_results_cache) {
  DCHECK(is_after_startup_);
  DCHECK(!inspection_results_cache_read_);

  inspection_results_cache_read_ = true;
  inspection_results_cache_ = std::move(inspection_results_cache);

  if (!queue_.empty())
    StartInspectingModule();
}

void ModuleInspector::OnUtilWinServiceConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ReportConnectionError(true);

  // Disconnect from the service.
  remote_util_win_.reset();

  // Restart inspection for the current module, only if the retry limit wasn't
  // reached.
  if (connection_error_retry_count_--)
    StartInspectingModule();
}

void ModuleInspector::StartInspectingModule() {
  DCHECK(inspection_results_cache_read_);
  DCHECK(!queue_.empty());

  if (background_inspection_disabled_)
    return;

  const ModuleInfoKey& module_key = queue_.front();

  // First check if the cache already contains the inspection result.
  auto inspection_result =
      GetInspectionResultFromCache(module_key, &inspection_results_cache_);
  if (inspection_result) {
    // Send asynchronously or this might cause a stack overflow.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ModuleInspector::OnInspectionFinished,
                                  weak_ptr_factory_.GetWeakPtr(), module_key,
                                  std::move(*inspection_result)));
    return;
  }

  if (base::FeatureList::IsEnabled(kWinOOPInspectModuleFeature)) {
    EnsureUtilWinServiceBound();

    // Use the test UtilWin remote if it exists.
    chrome::mojom::UtilWin* util_win = test_remote_util_win_
                                           ? test_remote_util_win_.get()
                                           : remote_util_win_.get();

    util_win->InspectModule(
        module_key.module_path,
        base::BindOnce(&ModuleInspector::OnModuleNewlyInspected,
                       weak_ptr_factory_.GetWeakPtr(), module_key));
  } else {
    // There is a small priority inversion that happens when
    // IncreaseInspectionPriority() is called while a module is currently being
    // inspected.
    //
    // This is because all the subsequent tasks on |inspection_task_runner_|
    // will be posted at a higher priority, but they are waiting on the current
    // task that is currently running at a lower priority.
    //
    // In practice, this is not an issue because the only caller of
    // IncreaseInspectionPriority() (chrome://conflicts) does not depend on the
    // inspection to finish synchronously and is not blocking anything else.
    base::PostTaskAndReplyWithResult(
        inspection_task_runner_.get(), FROM_HERE,
        base::BindOnce(&InspectModule, module_key.module_path),
        base::BindOnce(&ModuleInspector::OnModuleNewlyInspected,
                       weak_ptr_factory_.GetWeakPtr(), module_key));
  }
}

void ModuleInspector::OnModuleNewlyInspected(
    const ModuleInfoKey& module_key,
    ModuleInspectionResult inspection_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Convert the prefix of known Windows directories to their environment
  // variable mappings (ie, %systemroot$). This makes i18n localized paths
  // easily comparable.
  CollapseMatchingPrefixInPath(path_mapping_, &inspection_result.location);

  has_new_inspection_results_ = true;
  if (!flush_inspection_results_timer_.IsRunning())
    flush_inspection_results_timer_.Reset();
  AddInspectionResultToCache(module_key, inspection_result,
                             &inspection_results_cache_);

  OnInspectionFinished(module_key, std::move(inspection_result));
}

void ModuleInspector::OnInspectionFinished(
    const ModuleInfoKey& module_key,
    ModuleInspectionResult inspection_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Pop first, because the callback may want to know if there is any work left
  // to be done, which is caracterized by a non-empty queue.
  queue_.pop();

  on_module_inspected_callback_.Run(module_key, std::move(inspection_result));

  // Continue the work.
  if (!queue_.empty())
    StartInspectingModule();
}

void ModuleInspector::MaybeUpdateInspectionResultsCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!has_new_inspection_results_)
    return;

  has_new_inspection_results_ = false;
  cache_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WriteInspectionResultCacheOnBackgroundSequence,
                                GetInspectionResultsCachePath(),
                                inspection_results_cache_));
}
