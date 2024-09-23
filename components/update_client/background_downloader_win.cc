// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/background_downloader_win.h"

#include <objbase.h>

#include <windows.h>

#include <shlobj_core.h>
#include <stddef.h>
#include <stdint.h>
#include <winerror.h>

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_co_mem.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_client_metrics.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

// The class BackgroundDownloader in this module is an adapter between
// the CrxDownloader interface and the BITS service interfaces.
// The interface exposed on the CrxDownloader code runs on the main sequence,
// while the BITS specific code runs in a separate sequence bound to a
// COM apartment. For every url to download, a BITS job is created, unless
// there is already an existing job for that url, in which case, the downloader
// connects to it. Once a job is associated with the url, the code looks for
// changes in the BITS job state. The checks are triggered by a timer.
// The BITS job contains just one file to download. There could only be one
// download in progress at a time. If Chrome closes down before the download is
// complete, the BITS job remains active and finishes in the background, without
// any intervention. The job can be completed next time the code runs, if the
// file is still needed, otherwise it will be cleaned up on a periodic basis.
//
// To list the BITS jobs for a user, use the |bitsadmin| tool. The command line
// to do that is: "bitsadmin /list /verbose". Another useful command is
// "bitsadmin /info" and provide the job id returned by the previous /list
// command.
//
// Ignoring the suspend/resume issues since this code is not using them, the
// job state machine implemented by BITS is something like this:
//
//  Suspended--->Queued--->Connecting---->Transferring--->Transferred
//       |          ^         |                 |               |
//       |          |         V                 V               | (complete)
//       +----------|---------+-----------------+-----+         V
//                  |         |                 |     |    Acknowledged
//                  |         V                 V     |
//                  |  Transient Error------->Error   |
//                  |         |                 |     |(cancel)
//                  |         +-------+---------+--->-+
//                  |                 V               |
//                  |   (resume)      |               |
//                  +------<----------+               +---->Cancelled
//
// The job is created in the "suspended" state. Once |Resume| is called,
// BITS queues up the job, then tries to connect, begins transferring the
// job bytes, and moves the job to the "transferred state, after the job files
// have been transferred. When calling |Complete| for a job, the job files are
// made available to the caller, and the job is moved to the "acknowledged"
// state.
// At any point, the job can be cancelled, in which case, the job is moved
// to the "cancelled state" and the job object is removed from the BITS queue.
// Along the way, the job can encounter recoverable and non-recoverable errors.
// BITS moves the job to "transient error" or "error", depending on which kind
// of error has occured.
// If  the job has reached the "transient error" state, BITS retries the
// job after a certain programmable delay. If the job can't be completed in a
// certain time interval, BITS stops retrying and errors the job out. This time
// interval is also programmable.
// If the job is in either of the error states, the job parameters can be
// adjusted to handle the error, after which the job can be resumed, and the
// whole cycle starts again.
// Jobs that are not touched in 90 days (or a value set by group policy) are
// automatically disposed off by BITS. This concludes the brief description of
// a job lifetime, according to BITS.
//
// In addition to how BITS is managing the life time of the job, there are a
// couple of special cases defined by the BackgroundDownloader.
// First, if the job encounters any of the 5xx HTTP responses, the job is
// not retried, in order to avoid DDOS-ing the servers.
// Second, there is a simple mechanism to detect stuck jobs, and allow the rest
// of the code to move on to trying other urls or trying other components.
// Last, after completing a job, irrespective of the outcome, the jobs older
// than a week are proactively cleaned up.

namespace update_client {
namespace {

// All jobs created by this module have a specific description so they can
// be found at run-time or by using system administration tools.
constexpr wchar_t kJobName[] = L"Chrome Component Updater";

// How often the code looks for changes in the BITS job state.
constexpr int kJobPollingIntervalSec = 4;

// How long BITS waits before retrying a job after the job encountered
// a transient error. If this value is not set, the BITS default is 10 minutes.
constexpr int kMinimumRetryDelayMin = 1;

// How long to wait for stuck jobs. Stuck jobs could be queued for too long,
// have trouble connecting, or could be suspended for any reason.
constexpr int kJobStuckTimeoutMin = 15;

// How long BITS waits before giving up on a job that could not be completed
// since the job has encountered its first transient error. If this value is
// not set, the BITS default is 14 days.
constexpr int kSetNoProgressTimeoutDays = 1;

// How often the jobs which were started but not completed for any reason
// are cleaned up. Reasons for jobs to be left behind include browser restarts,
// system restarts, etc. Also, the check to purge stale jobs only happens
// at most once a day. If the job clean up code is not running, the BITS
// default policy is to cancel jobs after 90 days of inactivity.
constexpr int kPurgeStaleJobsAfterDays = 3;
constexpr int kPurgeStaleJobsIntervalBetweenChecksDays = 1;

// Number of maximum BITS jobs this downloader can create and queue up.
constexpr int kMaxQueuedJobs = 10;

// Prefix used for naming the temporary directories for downloads.
constexpr base::FilePath::CharType kDownloadDirectoryPrefix[] =
    FILE_PATH_LITERAL("chrome_BITS_");
constexpr base::FilePath::CharType kDownloadDirectoryPrefixMatcher[] =
    FILE_PATH_LITERAL("chrome_BITS_*");

// Returns the status code from a given BITS error.
int GetHttpStatusFromBitsError(HRESULT error) {
  // BITS errors are defined in bitsmsg.h. Although not documented, it is
  // clear that all errors corresponding to http status code have the high
  // word equal to 0x8019 and the low word equal to the http status code.
  const int kHttpStatusFirst = 100;  // Continue.
  const int kHttpStatusLast = 505;   // Version not supported.
  bool is_valid = HIWORD(error) == 0x8019 &&
                  LOWORD(error) >= kHttpStatusFirst &&
                  LOWORD(error) <= kHttpStatusLast;
  return is_valid ? LOWORD(error) : 0;
}

// Returns the files in a BITS job.
HRESULT GetFilesInJob(
    const Microsoft::WRL::ComPtr<IBackgroundCopyJob>& job,
    std::vector<Microsoft::WRL::ComPtr<IBackgroundCopyFile>>* files) {
  Microsoft::WRL::ComPtr<IEnumBackgroundCopyFiles> enum_files;
  HRESULT hr = job->EnumFiles(&enum_files);
  if (FAILED(hr)) {
    return hr;
  }

  ULONG num_files = 0;
  hr = enum_files->GetCount(&num_files);
  if (FAILED(hr)) {
    return hr;
  }

  for (ULONG i = 0; i != num_files; ++i) {
    Microsoft::WRL::ComPtr<IBackgroundCopyFile> file;
    if (enum_files->Next(1, &file, nullptr) == S_OK && file.Get()) {
      files->push_back(file);
    }
  }

  return S_OK;
}

// Returns the file name, the url, and some per-file progress information.
// The function out parameters can be NULL if that data is not requested.
HRESULT GetJobFileProperties(
    const Microsoft::WRL::ComPtr<IBackgroundCopyFile>& file,
    std::wstring* local_name,
    std::wstring* remote_name,
    BG_FILE_PROGRESS* progress) {
  if (!file) {
    return E_FAIL;
  }

  HRESULT hr = S_OK;

  if (local_name) {
    base::win::ScopedCoMem<wchar_t> name;
    hr = file->GetLocalName(&name);
    if (FAILED(hr)) {
      return hr;
    }
    local_name->assign(name);
  }

  if (remote_name) {
    base::win::ScopedCoMem<wchar_t> name;
    hr = file->GetRemoteName(&name);
    if (FAILED(hr)) {
      return hr;
    }
    remote_name->assign(name);
  }

  if (progress) {
    BG_FILE_PROGRESS bg_file_progress = {};
    hr = file->GetProgress(&bg_file_progress);
    if (FAILED(hr)) {
      return hr;
    }
    *progress = bg_file_progress;
  }

  return hr;
}

// Returns the number of bytes downloaded and bytes to download for all files
// in the job. If the values are not known or if an error has occurred,
// a value of -1 is reported.
HRESULT GetJobByteCount(const Microsoft::WRL::ComPtr<IBackgroundCopyJob>& job,
                        int64_t* downloaded_bytes,
                        int64_t* total_bytes) {
  *downloaded_bytes = -1;
  *total_bytes = -1;

  if (!job) {
    return E_FAIL;
  }

  BG_JOB_PROGRESS job_progress = {};
  HRESULT hr = job->GetProgress(&job_progress);
  if (FAILED(hr)) {
    return hr;
  }

  const uint64_t kMaxNumBytes =
      static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
  if (job_progress.BytesTransferred <= kMaxNumBytes) {
    *downloaded_bytes = job_progress.BytesTransferred;
  }

  if (job_progress.BytesTotal <= kMaxNumBytes &&
      job_progress.BytesTotal != BG_SIZE_UNKNOWN) {
    *total_bytes = job_progress.BytesTotal;
  }

  return S_OK;
}

HRESULT GetJobDisplayName(const Microsoft::WRL::ComPtr<IBackgroundCopyJob>& job,
                          std::wstring* name) {
  base::win::ScopedCoMem<wchar_t> local_name;
  const HRESULT hr = job->GetDisplayName(&local_name);
  if (FAILED(hr)) {
    return hr;
  }
  *name = local_name.get();
  return S_OK;
}

// Returns the job error code in |error_code| if the job is in the transient
// or the final error state. Otherwise, the job error is not available and
// the function fails.
HRESULT GetJobError(const Microsoft::WRL::ComPtr<IBackgroundCopyJob>& job,
                    HRESULT* error_code_out) {
  *error_code_out = S_OK;
  Microsoft::WRL::ComPtr<IBackgroundCopyError> copy_error;
  HRESULT hr = job->GetError(&copy_error);
  if (FAILED(hr)) {
    return hr;
  }

  BG_ERROR_CONTEXT error_context = BG_ERROR_CONTEXT_NONE;
  HRESULT error_code = S_OK;
  hr = copy_error->GetError(&error_context, &error_code);
  if (FAILED(hr)) {
    return hr;
  }

  *error_code_out = FAILED(error_code) ? error_code : E_FAIL;
  return S_OK;
}

// Finds the component updater jobs matching the given predicate.
// Returns S_OK if the function has found at least one job, returns S_FALSE if
// no job was found, and it returns an error otherwise.
template <class Predicate>
HRESULT FindBitsJobIf(
    Predicate pred,
    const Microsoft::WRL::ComPtr<IBackgroundCopyManager>& bits_manager,
    std::vector<Microsoft::WRL::ComPtr<IBackgroundCopyJob>>* jobs) {
  Microsoft::WRL::ComPtr<IEnumBackgroundCopyJobs> enum_jobs;
  HRESULT hr = bits_manager->EnumJobs(0, &enum_jobs);
  if (FAILED(hr)) {
    return hr;
  }

  ULONG job_count = 0;
  hr = enum_jobs->GetCount(&job_count);
  if (FAILED(hr)) {
    return hr;
  }

  // Iterate over jobs, run the predicate, and select the job only if
  // the job description matches the component updater jobs.
  for (ULONG i = 0; i != job_count; ++i) {
    Microsoft::WRL::ComPtr<IBackgroundCopyJob> current_job;
    if (enum_jobs->Next(1, &current_job, nullptr) == S_OK &&
        pred(current_job)) {
      std::wstring job_name;
      hr = GetJobDisplayName(current_job, &job_name);
      if (job_name.compare(kJobName) == 0) {
        jobs->push_back(current_job);
      }
    }
  }

  return jobs->empty() ? S_FALSE : S_OK;
}

bool JobCreationOlderThanDaysPredicate(
    Microsoft::WRL::ComPtr<IBackgroundCopyJob> job,
    int num_days) {
  BG_JOB_TIMES times = {};
  HRESULT hr = job->GetTimes(&times);
  if (FAILED(hr)) {
    return false;
  }

  const base::TimeDelta time_delta(base::Days(num_days));
  const base::Time creation_time(base::Time::FromFileTime(times.CreationTime));

  return creation_time + time_delta < base::Time::Now();
}

bool JobFileUrlEqualPredicate(Microsoft::WRL::ComPtr<IBackgroundCopyJob> job,
                              const GURL& url) {
  std::vector<Microsoft::WRL::ComPtr<IBackgroundCopyFile>> files;
  HRESULT hr = GetFilesInJob(job, &files);
  if (FAILED(hr)) {
    return false;
  }

  for (size_t i = 0; i != files.size(); ++i) {
    base::win::ScopedCoMem<wchar_t> remote_name;
    if (SUCCEEDED(files[i]->GetRemoteName(&remote_name)) &&
        url == GURL(base::SysWideToUTF8(remote_name.get()))) {
      return true;
    }
  }

  return false;
}

// Creates an instance of the BITS manager.
HRESULT CreateBitsManager(
    Microsoft::WRL::ComPtr<IBackgroundCopyManager>* bits_manager) {
  Microsoft::WRL::ComPtr<IBackgroundCopyManager> local_bits_manager;
  HRESULT hr;
  {
    // CoCreateInstance may acquire the loader lock to load a library. Doing it
    // at background priority can cause a priority inversion with the main
    // thread, perceived as a hang by the user.
    // SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY() mitigates this problem
    // by boosting the thread's priority. See crbug.com/1295941.
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    hr = ::CoCreateInstance(__uuidof(BackgroundCopyManager), nullptr,
                            CLSCTX_ALL, IID_PPV_ARGS(&local_bits_manager));
  }

  if (FAILED(hr)) {
    return hr;
  }
  *bits_manager = local_bits_manager;
  return S_OK;
}

void CleanupJob(const Microsoft::WRL::ComPtr<IBackgroundCopyJob>& job) {
  if (!job) {
    return;
  }

  // Get the file paths associated with this job before canceling the job.
  // Canceling the job removes it from the BITS queue right away. It appears
  // that it is still possible to query for the properties of the job after
  // the job has been canceled. It seems safer though to get the paths first.
  std::vector<Microsoft::WRL::ComPtr<IBackgroundCopyFile>> files;
  GetFilesInJob(job, &files);

  std::vector<base::FilePath> paths;
  for (const auto& file : files) {
    std::wstring local_name;
    HRESULT hr = GetJobFileProperties(file, &local_name, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      paths.push_back(base::FilePath(local_name));
    }
  }

  job->Cancel();

  for (const auto& path : paths) {
    DeleteFileAndEmptyParentDirectory(path);
  }
}

void CheckIsMta() {
  CHECK_EQ(base::win::GetComApartmentTypeForThread(),
           base::win::ComApartmentType::MTA);
}

}  // namespace

BackgroundDownloader::BackgroundDownloader(
    scoped_refptr<CrxDownloader> successor)
    : CrxDownloader(std::move(successor)),
      com_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          kTaskTraitsBackgroundDownloader)) {
  DETACH_FROM_SEQUENCE(com_sequence_checker_);
}

BackgroundDownloader::~BackgroundDownloader() {
  DETACH_FROM_SEQUENCE(com_sequence_checker_);
}

void BackgroundDownloader::StartTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_ = std::make_unique<base::OneShotTimer>();
  timer_->Start(FROM_HERE, base::Seconds(kJobPollingIntervalSec), this,
                &BackgroundDownloader::OnTimer);
}

void BackgroundDownloader::OnTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_ = nullptr;
  com_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundDownloader::OnDownloading, this));
}

base::OnceClosure BackgroundDownloader::DoStartDownload(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundDownloader::BeginDownload, this, url));
  return base::DoNothing();
}

// Called one time when this class is asked to do a download.
void BackgroundDownloader::BeginDownload(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  download_start_time_ = base::TimeTicks::Now();
  job_stuck_begin_time_ = download_start_time_;

  HRESULT hr = BeginDownloadHelper(url);
  if (FAILED(hr)) {
    EndDownload(hr);
    return;
  }

  VLOG(1) << "Starting BITS download for: " << url.spec();

  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundDownloader::StartTimer, this));
}

// Creates or opens an existing BITS job to download the |url|, and handles
// the marshalling of the interfaces in GIT.
HRESULT BackgroundDownloader::BeginDownloadHelper(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  HRESULT hr = CreateBitsManager(&bits_manager_);
  if (FAILED(hr)) {
    return hr;
  }

  hr = QueueBitsJob(url, &job_);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

// Called any time the timer fires.
void BackgroundDownloader::OnDownloading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  BG_JOB_STATE job_state = BG_JOB_STATE_ERROR;
  HRESULT hr = job_->GetState(&job_state);
  if (FAILED(hr)) {
    EndDownload(hr);
    return;
  }

  bool is_handled = false;
  switch (job_state) {
    case BG_JOB_STATE_TRANSFERRED:
      is_handled = OnStateTransferred();
      break;

    case BG_JOB_STATE_ERROR:
      is_handled = OnStateError();
      break;

    case BG_JOB_STATE_CANCELLED:
      is_handled = OnStateCancelled();
      break;

    case BG_JOB_STATE_ACKNOWLEDGED:
      is_handled = OnStateAcknowledged();
      break;

    case BG_JOB_STATE_QUEUED:
    // Fall through.
    case BG_JOB_STATE_CONNECTING:
    // Fall through.
    case BG_JOB_STATE_SUSPENDED:
      is_handled = OnStateQueued();
      break;

    case BG_JOB_STATE_TRANSIENT_ERROR:
      is_handled = OnStateTransientError();
      break;

    case BG_JOB_STATE_TRANSFERRING:
      is_handled = OnStateTransferring();
      break;

    default:
      break;
  }

  if (is_handled) {
    return;
  }

  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundDownloader::StartTimer, this));
}

// Completes the BITS download, picks up the file path of the response, and
// notifies the CrxDownloader. The function should be called only once.
void BackgroundDownloader::EndDownload(HRESULT error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  const base::TimeTicks download_end_time(base::TimeTicks::Now());
  const base::TimeDelta download_time =
      download_end_time >= download_start_time_
          ? download_end_time - download_start_time_
          : base::TimeDelta();

  int64_t downloaded_bytes = -1;
  int64_t total_bytes = -1;
  GetJobByteCount(job_, &downloaded_bytes, &total_bytes);

  if (FAILED(error)) {
    CleanupJob(job_);
  }

  job_ = nullptr;
  bits_manager_ = nullptr;

  // Consider the url handled if it has been successfully downloaded or a
  // 5xx has been received.
  const bool is_handled =
      SUCCEEDED(error) || IsHttpServerError(GetHttpStatusFromBitsError(error));

  const int error_to_report = SUCCEEDED(error) ? 0 : error;

  DownloadMetrics download_metrics;
  download_metrics.url = url();
  download_metrics.downloader = DownloadMetrics::kBits;
  download_metrics.error = error_to_report;
  download_metrics.downloaded_bytes = downloaded_bytes;
  download_metrics.total_bytes = total_bytes;
  download_metrics.download_time_ms = download_time.InMilliseconds();

  Result result;
  result.error = error_to_report;
  if (!result.error) {
    result.response = response_;
  }
  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundDownloader::OnDownloadComplete, this,
                                is_handled, result, download_metrics));
}

// Called when the BITS job has been transferred successfully. Completes the
// BITS job by removing it from the BITS queue and making the download
// available to the caller.
bool BackgroundDownloader::OnStateTransferred() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();
  EndDownload(CompleteJob());
  return true;
}

// Called when the job has encountered an error and no further progress can
// be made. Cancels this job and removes it from the BITS queue.
bool BackgroundDownloader::OnStateError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();
  HRESULT error_code = S_OK;
  HRESULT hr = GetJobError(job_, &error_code);
  if (FAILED(hr)) {
    error_code = hr;
  }
  CHECK(FAILED(error_code));
  EndDownload(error_code);
  return true;
}

// Called when the download was completed. This notification is not seen
// in the current implementation but provided here as a defensive programming
// measure.
bool BackgroundDownloader::OnStateAcknowledged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();
  EndDownload(E_UNEXPECTED);
  return true;
}

// Called when the download was cancelled. Same as above.
bool BackgroundDownloader::OnStateCancelled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();
  EndDownload(E_UNEXPECTED);
  return true;
}

// Called when the job has encountered a transient error, such as a
// network disconnect, a server error, or some other recoverable error.
bool BackgroundDownloader::OnStateTransientError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  // Don't retry at all if the transient error was a 5xx.
  HRESULT error_code = S_OK;
  HRESULT hr = GetJobError(job_, &error_code);
  if (SUCCEEDED(hr) &&
      IsHttpServerError(GetHttpStatusFromBitsError(error_code))) {
    return OnStateError();
  }

  return false;
}

bool BackgroundDownloader::OnStateQueued() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  if (!IsStuck()) {
    return false;
  }

  // Terminate the download if the job has not made progress in a while.
  EndDownload(E_ABORT);
  return true;
}

bool BackgroundDownloader::OnStateTransferring() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  // Resets the baseline for detecting a stuck job since the job is transferring
  // data and it is making progress.
  job_stuck_begin_time_ = base::TimeTicks::Now();

  int64_t downloaded_bytes = -1;
  int64_t total_bytes = -1;
  GetJobByteCount(job_, &downloaded_bytes, &total_bytes);

  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundDownloader::OnDownloadProgress, this,
                                downloaded_bytes, total_bytes));
  return false;
}

HRESULT BackgroundDownloader::QueueBitsJob(
    const GURL& url,
    Microsoft::WRL::ComPtr<IBackgroundCopyJob>* job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  // Remove some old jobs from the BITS queue before creating new jobs.
  CleanupStaleJobs();

  size_t num_jobs = std::numeric_limits<size_t>::max();
  HRESULT hr = GetBackgroundDownloaderJobCount(&num_jobs);

  if (FAILED(hr) || num_jobs >= kMaxQueuedJobs) {
    return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF,
                        CrxDownloaderError::BITS_TOO_MANY_JOBS);
  }

  Microsoft::WRL::ComPtr<IBackgroundCopyJob> local_job;
  hr = CreateOrOpenJob(url, &local_job);
  if (FAILED(hr)) {
    CleanupJob(local_job);
    return hr;
  }

  hr = local_job->Resume();
  if (FAILED(hr)) {
    CleanupJob(local_job);
    return hr;
  }

  *job = local_job;
  return S_OK;
}

HRESULT BackgroundDownloader::CreateOrOpenJob(
    const GURL& url,
    Microsoft::WRL::ComPtr<IBackgroundCopyJob>* job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  std::vector<Microsoft::WRL::ComPtr<IBackgroundCopyJob>> jobs;
  HRESULT hr = FindBitsJobIf(
      [&url](Microsoft::WRL::ComPtr<IBackgroundCopyJob> job) {
        return JobFileUrlEqualPredicate(job, url);
      },
      bits_manager_, &jobs);
  if (SUCCEEDED(hr) && !jobs.empty()) {
    metrics::RecordBDWExistingJobUsed(true);
    *job = jobs.front();
    return S_FALSE;
  }

  Microsoft::WRL::ComPtr<IBackgroundCopyJob> local_job;

  GUID guid = {0};
  hr = bits_manager_->CreateJob(kJobName, BG_JOB_TYPE_DOWNLOAD, &guid,
                                &local_job);
  if (FAILED(hr)) {
    CleanupJob(local_job);
    return hr;
  }

  hr = InitializeNewJob(local_job, url);
  if (FAILED(hr)) {
    CleanupJob(local_job);
    return hr;
  }

  metrics::RecordBDWExistingJobUsed(false);
  *job = local_job;
  return S_OK;
}

HRESULT BackgroundDownloader::InitializeNewJob(
    const Microsoft::WRL::ComPtr<IBackgroundCopyJob>& job,
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  base::FilePath tempdir;
  if (!base::CreateNewTempDirectory(kDownloadDirectoryPrefix, &tempdir)) {
    return E_FAIL;
  }

  const std::wstring filename(base::SysUTF8ToWide(url.ExtractFileName()));
  HRESULT hr = job->AddFile(base::SysUTF8ToWide(url.spec()).c_str(),
                            tempdir.Append(filename).value().c_str());
  if (FAILED(hr)) {
    return hr;
  }

  hr = job->SetDescription(filename.c_str());
  if (FAILED(hr)) {
    return hr;
  }

  hr = job->SetPriority(BG_JOB_PRIORITY_NORMAL);
  if (FAILED(hr)) {
    return hr;
  }

  hr = job->SetMinimumRetryDelay(60 * kMinimumRetryDelayMin);
  if (FAILED(hr)) {
    return hr;
  }

  const int kSecondsDay = 60 * 60 * 24;
  hr = job->SetNoProgressTimeout(kSecondsDay * kSetNoProgressTimeoutDays);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

bool BackgroundDownloader::IsStuck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();
  const base::TimeDelta job_stuck_timeout(base::Minutes(kJobStuckTimeoutMin));
  return job_stuck_begin_time_ + job_stuck_timeout < base::TimeTicks::Now();
}

HRESULT BackgroundDownloader::CompleteJob() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  HRESULT hr = job_->Complete();
  if (FAILED(hr) && hr != BG_S_UNABLE_TO_DELETE_FILES) {
    return hr;
  }

  std::vector<Microsoft::WRL::ComPtr<IBackgroundCopyFile>> files;
  hr = GetFilesInJob(job_, &files);
  if (FAILED(hr)) {
    return hr;
  }

  if (files.empty()) {
    return E_UNEXPECTED;
  }

  std::wstring local_name;
  BG_FILE_PROGRESS progress = {0};
  hr = GetJobFileProperties(files.front(), &local_name, nullptr, &progress);
  if (FAILED(hr)) {
    return hr;
  }

  // Check the post-conditions of a successful download, including the file and
  // job invariants. The byte counts for a job and its file must match as a job
  // only contains one file.
  CHECK(progress.Completed);
  CHECK_EQ(progress.BytesTotal, progress.BytesTransferred);

  response_ = base::FilePath(local_name);

  return S_OK;
}

HRESULT BackgroundDownloader::GetBackgroundDownloaderJobCount(
    size_t* num_jobs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();
  CHECK(bits_manager_);

  std::vector<Microsoft::WRL::ComPtr<IBackgroundCopyJob>> jobs;
  const HRESULT hr = FindBitsJobIf(
      [](const Microsoft::WRL::ComPtr<IBackgroundCopyJob>&) { return true; },
      bits_manager_, &jobs);
  if (FAILED(hr)) {
    return hr;
  }

  *num_jobs = jobs.size();
  return S_OK;
}

void BackgroundDownloader::CleanupStaleJobs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();
  CHECK(bits_manager_);

  static base::Time last_sweep;

  const base::TimeDelta time_delta(
      base::Days(kPurgeStaleJobsIntervalBetweenChecksDays));
  const base::Time current_time(base::Time::Now());
  if (last_sweep + time_delta > current_time) {
    return;
  }

  last_sweep = current_time;

  std::vector<Microsoft::WRL::ComPtr<IBackgroundCopyJob>> jobs;
  FindBitsJobIf(
      [](Microsoft::WRL::ComPtr<IBackgroundCopyJob> job) {
        return JobCreationOlderThanDaysPredicate(job, kPurgeStaleJobsAfterDays);
      },
      bits_manager_, &jobs);

  metrics::RecordBDWNumJobsCleaned(jobs.size());
  for (const auto& job : jobs) {
    CleanupJob(job);
  }

  CleanupStaleDownloads();
}

void BackgroundDownloader::CleanupStaleDownloads() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  EnumerateDownloadDirs(
      kDownloadDirectoryPrefixMatcher, [](const base::FilePath& dir) {
        const base::Time now = base::Time::Now();
        base::File::Info info;
        if (base::GetFileInfo(dir, &info) &&
            info.creation_time + base::Days(kPurgeStaleJobsAfterDays) < now) {
          metrics::RecordBDWStaleDownloadAge(now - info.creation_time);
          RetryDeletePathRecursively(dir);
        }
      });
}

void BackgroundDownloader::EnumerateDownloadDirs(
    const base::FilePath::StringType& matcher,
    base::FunctionRef<void(const base::FilePath& dir)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  base::FilePath dir;
  std::vector<base::FilePath> dirs;
  if (base::PathService::Get(base::DIR_SYSTEM_TEMP, &dir)) {
    dirs.push_back(dir);
  }
  if (base::GetTempDir(&dir)) {
    dirs.push_back(dir);
  }
  base::ranges::for_each(dirs, [&](const base::FilePath& parent_dir) {
    base::FileEnumerator(parent_dir,
                         /*recursive=*/false, base::FileEnumerator::DIRECTORIES,
                         matcher)
        .ForEach(callback);
  });
}

}  // namespace update_client
