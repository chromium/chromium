// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_inspector.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/win/conflicts/module_info_util.h"
#include "chrome/browser/win/util_win_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"

namespace {

// The maximum amount of time a stale entry is kept in the cache before it is
// deleted.
constexpr base::TimeDelta kMaxEntryAge = base::Days(30);

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

// Reads the inspection results cache.
InspectionResultsCache ReadInspectionResultsCacheOnBackgroundSequence(
    const base::FilePath& file_path) {
  InspectionResultsCache inspection_results_cache;

  uint32_t min_time_stamp =
      CalculateTimeStamp(base::Time::Now() - kMaxEntryAge);
  ReadInspectionResultsCache(file_path, min_time_stamp,
                             &inspection_results_cache);

  return inspection_results_cache;
}

// Writes the inspection results cache to disk.
void WriteInspectionResultCacheOnBackgroundSequence(
    const base::FilePath& file_path,
    const InspectionResultsCache& inspection_results_cache) {
  WriteInspectionResultsCache(file_path, inspection_results_cache);
}

}  // namespace

// static
constexpr base::TimeDelta ModuleInspector::kFlushInspectionResultsTimerTimeout;

ModuleInspector::ModuleInspector(
    const OnModuleInspectedCallback& on_module_inspected_callback)
    : on_module_inspected_callback_(on_module_inspected_callback),
      is_started_(false),
      util_win_factory_callback_(
          base::BindRepeating(&LaunchUtilWinServiceInstance)),
      path_mapping_(GetPathMapping()),
      cache_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
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
      is_waiting_on_util_win_service_(false) {}

ModuleInspector::~ModuleInspector() = default;

void ModuleInspector::StartInspection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This function can be invoked multiple times.
  if (is_started_) {
    return;
  }

  is_started_ = true;

  // Read the inspection cache now that it is needed.
  cache_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadInspectionResultsCacheOnBackgroundSequence,
                     GetInspectionResultsCachePath()),
      base::BindOnce(&ModuleInspector::OnInspectionResultsCacheRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ModuleInspector::AddModule(const ModuleInfoKey& module_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool was_queue_empty = queue_.empty();

  queue_.push(module_key);

  // If the queue was empty before adding the current module, then the
  // inspection must be started.
  if (inspection_results_cache_read_ && was_queue_empty)
    StartInspectingModule();
}

bool ModuleInspector::IsIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return queue_.empty();
}

void ModuleInspector::OnModuleDatabaseIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeUpdateInspectionResultsCache();
}

// static
base::FilePath ModuleInspector::GetInspectionResultsCachePath() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return base::FilePath();

  return user_data_dir.Append(L"Module Info Cache");
}

void ModuleInspector::SetUtilWinFactoryCallbackForTesting(
    UtilWinFactoryCallback util_win_factory_callback) {
  util_win_factory_callback_ = std::move(util_win_factory_callback);
}

void ModuleInspector::EnsureUtilWinServiceBound() {
  if (remote_util_win_)
    return;

  remote_util_win_ = util_win_factory_callback_.Run();
  remote_util_win_.reset_on_idle_timeout(base::Seconds(5));
  remote_util_win_.set_disconnect_handler(
      base::BindOnce(&ModuleInspector::OnUtilWinServiceConnectionError,
                     base::Unretained(this)));
}

void ModuleInspector::OnInspectionResultsCacheRead(
    InspectionResultsCache inspection_results_cache) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_started_);
  DCHECK(!inspection_results_cache_read_);

  inspection_results_cache_read_ = true;
  inspection_results_cache_ = std::move(inspection_results_cache);

  if (!queue_.empty())
    StartInspectingModule();
}

void ModuleInspector::OnUtilWinServiceConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Disconnect from the service.
  remote_util_win_.reset();

  // If the retry limit was reached, give up.
  if (connection_error_retry_count_ == 0)
    return;
  --connection_error_retry_count_;

  bool was_waiting_on_util_win_service = is_waiting_on_util_win_service_;
  is_waiting_on_util_win_service_ = false;

  // If this connection error happened while the ModuleInspector was waiting on
  // the service, restart the inspection process.
  if (was_waiting_on_util_win_service)
    StartInspectingModule();
}

void ModuleInspector::StartInspectingModule() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(inspection_results_cache_read_);
  DCHECK(!queue_.empty());

  const ModuleInfoKey& module_key = queue_.front();

  // First check if the cache already contains the inspection result.
  auto inspection_result =
      GetInspectionResultFromCache(module_key, &inspection_results_cache_);
  if (inspection_result) {
    // Send asynchronously or this might cause a stack overflow.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ModuleInspector::OnInspectionFinished,
                                  weak_ptr_factory_.GetWeakPtr(), module_key,
                                  std::move(*inspection_result)));
    return;
  }

  EnsureUtilWinServiceBound();

  is_waiting_on_util_win_service_ = true;
  remote_util_win_->InspectModule(
      module_key.module_path,
      base::BindOnce(&ModuleInspector::OnModuleNewlyInspected,
                     weak_ptr_factory_.GetWeakPtr(), module_key));
}

void ModuleInspector::OnModuleNewlyInspected(
    const ModuleInfoKey& module_key,
    ModuleInspectionResult inspection_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_waiting_on_util_win_service_ = false;

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

  DCHECK(!queue_.empty());
  DCHECK(queue_.front() == module_key);

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
