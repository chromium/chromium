// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/delivery_optimization_downloader_win.h"

#include <objbase.h>

#include <windows.h>

#include <deliveryoptimization.h>
#include <oleauto.h>
#include <stddef.h>
#include <stdint.h>
#include <winerror.h>
#include <wrl/client.h>

#include <algorithm>
#include <ios>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_variant.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

// The class DeliveryOptimizationDownloader in this module is an adapter
// between the CrxDownloader interface and the Windows Delivery Optimization
// (DO) service interfaces. It is modeled after the BITS-based
// BackgroundDownloader in background_downloader_win.cc, and is meant to be
// chained ahead of it: DO -> BITS -> URL fetcher.
//
// The interface exposed on the CrxDownloader code runs on the main sequence,
// while the DO specific code runs in a separate sequence bound to a COM
// apartment. For every url to download, a DO download is created, unless there
// is already an existing download for that url, in which case, the downloader
// connects to it. Once a download is associated with the url, the code looks
// for changes in the DO download state. The checks are triggered by a timer.
// A DO download contains just one file. If Chrome closes down before the
// download is complete, the download remains owned by the DO service and may
// finish in the background. The download can be completed next time the code
// runs, if the file is still needed, otherwise it will be cleaned up on a
// periodic basis.
//
// The download state machine implemented by DO is:
//
// clang-format off
//
//  Created --(Start)--> Transferring ---> Transferred --(Finalize)--> Finalized
//     |                  ^    |     |
//     |         (Start)  |    V     |
//     |                  +-Paused   |
//     |                       |     | (Abort / fatal error)
//     +-----------------------+-----+------------------------------> Aborted
//
// clang-format on
//
// The download is created in the "Created" state. Once |Start| is called, DO
// begins transferring the bytes and moves the download to the "Transferred"
// state after the file has been transferred. When calling |Finalize| for a
// download, the file is made available to the caller, and the download is
// moved to the "Finalized" state.
// At any point, the download can be aborted, in which case, the download is
// moved to the "Aborted" state and removed from the DO queue. Unlike BITS, DO
// does not have a separate "transient error" state: transient errors move the
// download to "Paused" with the error available from the download status, and
// non-recoverable errors move the download to "Aborted".
// If a download makes no progress for a programmable timeout, DO aborts it.
//
// In addition to how DO is managing the life time of the download, there are a
// couple of special cases defined by the DeliveryOptimizationDownloader.
// First, if the download encounters any of the 5xx HTTP responses, the
// download is not retried, in order to avoid DDOS-ing the servers.
// Second, there is a simple mechanism to detect stuck downloads, and allow the
// rest of the code to move on to trying other urls or trying other components.
// Third, if DO is not available on the host (the COM class is not registered,
// the service is disabled by policy, or access is denied), the download fails
// fast and is handed over to the successor in the chain.
// Last, before creating a new download, the downloads which can no longer be
// completed are removed from the DO queue, and, at most once a day, the
// downloads older than a few days are proactively cleaned up.

namespace update_client {

BASE_FEATURE(kDeliveryOptimizationDownloader,
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

// All downloads created by this module have a specific display name so they
// can be found at run-time or by using system administration tools.
constexpr wchar_t kDownloadName[] = L"Chrome Component Updater";

// File name used for the downloaded file when the url does not have one.
constexpr wchar_t kDefaultDownloadFilename[] = L"download";

// How often the code looks for changes in the DO download state.
constexpr int kDownloadPollingIntervalSec = 4;

// Minimum delay between two attempts to resume a download which has been
// paused by DO after a transient error.
constexpr int kMinimumRetryDelayMin = 1;

// How long to wait for stuck downloads. Stuck downloads could be created but
// not started, paused for too long, or could not be connecting for any reason.
constexpr int kDownloadStuckTimeoutMin = 15;

// How long DO waits before giving up on a download that has not made any
// progress.
constexpr int kNoProgressTimeoutDays = 1;

// How often the downloads which were started but not completed for any reason
// are cleaned up. Reasons for downloads to be left behind include browser
// restarts, system restarts, etc. Also, the check to purge stale downloads only
// happens at most once a day.
constexpr int kPurgeStaleDownloadsAfterDays = 3;
constexpr int kPurgeStaleDownloadsIntervalBetweenChecksDays = 1;

// Number of maximum DO downloads this downloader can create and queue up.
constexpr int kMaxQueuedDownloads = 10;

// Prefix used for naming the temporary directories for downloads.
constexpr base::FilePath::CharType kDownloadDirectoryPrefix[] =
    FILE_PATH_LITERAL("_chrome_DO_");
constexpr base::FilePath::CharType kDownloadDirectoryPrefixMatcher[] =
    FILE_PATH_LITERAL("_chrome_DO_*");

// Returns the HTTP status code of the download, if DO exposes it. Falls back
// to decoding the download errors otherwise, looking at both the error and the
// extended error, since DO may report a DO_E_* code in the former and the
// underlying HTTP_E_STATUS_* code in the latter.
int GetDownloadHttpStatus(const Microsoft::WRL::ComPtr<IDODownload>& download,
                          HRESULT error,
                          HRESULT extended_error) {
  if (download) {
    base::win::ScopedVariant status;
    if (SUCCEEDED(download->GetProperty(DODownloadProperty_HttpStatusCode,
                                        status.Receive())) &&
        status.type() == VT_UI4 && V_UI4(status.ptr()) != 0) {
      return static_cast<int>(V_UI4(status.ptr()));
    }
  }
  const int http_status =
      DeliveryOptimizationDownloader::GetHttpStatusFromHresult(error);
  return http_status ? http_status
                     : DeliveryOptimizationDownloader::GetHttpStatusFromHresult(
                           extended_error);
}

HRESULT SetDownloadProperty(const Microsoft::WRL::ComPtr<IDODownload>& download,
                            DODownloadProperty property,
                            const base::win::ScopedVariant& value) {
  return download->SetProperty(property, value.ptr());
}

// Returns a string property of the download, such as its uri or local path.
HRESULT GetDownloadStringProperty(
    const Microsoft::WRL::ComPtr<IDODownload>& download,
    DODownloadProperty property,
    std::wstring* value) {
  if (!download) {
    return E_FAIL;
  }
  base::win::ScopedVariant var;
  HRESULT hr = download->GetProperty(property, var.Receive());
  if (FAILED(hr)) {
    return hr;
  }
  if (var.type() != VT_BSTR) {
    return E_UNEXPECTED;
  }
  const BSTR bstr = V_BSTR(var.ptr());
  value->assign(bstr ? bstr : L"", bstr ? ::SysStringLen(bstr) : 0);
  return S_OK;
}

HRESULT GetDownloadLocalPath(
    const Microsoft::WRL::ComPtr<IDODownload>& download,
    base::FilePath* local_path) {
  std::wstring path;
  const HRESULT hr =
      GetDownloadStringProperty(download, DODownloadProperty_LocalPath, &path);
  if (FAILED(hr)) {
    return hr;
  }
  *local_path = base::FilePath(path);
  return S_OK;
}

HRESULT GetDownloadStatus(const Microsoft::WRL::ComPtr<IDODownload>& download,
                          DO_DOWNLOAD_STATUS* status) {
  *status = {};
  if (!download) {
    return E_FAIL;
  }
  return download->GetStatus(status);
}

// Finds the component updater downloads matching the given predicate.
// Returns S_OK if the function has found at least one download, returns
// S_FALSE if no download was found, and it returns an error otherwise.
// The enumeration is scoped to the downloads which have the display name used
// by this module. DO only enumerates downloads created by the same caller.
template <class Predicate>
HRESULT FindDownloadsIf(
    Predicate pred,
    const Microsoft::WRL::ComPtr<IDOManager>& do_manager,
    std::vector<Microsoft::WRL::ComPtr<IDODownload>>* downloads) {
  DO_DOWNLOAD_ENUM_CATEGORY category = {};
  category.Property = DODownloadProperty_DisplayName;
  category.Value = kDownloadName;

  Microsoft::WRL::ComPtr<IEnumUnknown> enum_downloads;
  HRESULT hr = do_manager->EnumDownloads(&category, &enum_downloads);
  if (FAILED(hr)) {
    return hr;
  }
  if (!enum_downloads) {
    return S_FALSE;
  }

  for (;;) {
    Microsoft::WRL::ComPtr<IUnknown> unknown;
    ULONG fetched = 0;
    if (enum_downloads->Next(1, &unknown, &fetched) != S_OK || !unknown) {
      break;
    }
    Microsoft::WRL::ComPtr<IDODownload> download;
    if (SUCCEEDED(unknown.As(&download)) && download && pred(download)) {
      downloads->push_back(download);
    }
  }

  return downloads->empty() ? S_FALSE : S_OK;
}

bool DownloadUrlEqualPredicate(
    const Microsoft::WRL::ComPtr<IDODownload>& download,
    const GURL& url) {
  std::wstring uri;
  return SUCCEEDED(GetDownloadStringProperty(download, DODownloadProperty_Uri,
                                             &uri)) &&
         url == GURL(base::WideToUTF8(uri));
}

// Returns true if the download has a local path and the directory which is
// supposed to contain the downloaded file still exists.
bool HasDownloadDirectory(const Microsoft::WRL::ComPtr<IDODownload>& download,
                          base::FilePath* local_path) {
  return SUCCEEDED(GetDownloadLocalPath(download, local_path)) &&
         !local_path->empty() && base::DirectoryExists(local_path->DirName());
}

// Returns true if the download can never be completed by this module and its
// file can be safely removed: its status can't be retrieved, it has been
// aborted (for instance, by DO in the background after a fatal error or after
// the no-progress timeout expired), or the temporary directory which is
// supposed to contain the downloaded file is gone.
//
// Finalized downloads are not considered unusable. Finalizing a download hands
// its file over to the caller, which may still be verifying or unpacking it,
// possibly from a different instance of this class. Such downloads are only
// removed by the age-based stale download sweep.
//
// When the function returns false, |local_path|, if provided, receives the
// path of the downloaded file.
bool IsDownloadUnusable(const Microsoft::WRL::ComPtr<IDODownload>& download,
                        base::FilePath* local_path = nullptr) {
  DO_DOWNLOAD_STATUS status = {};
  if (FAILED(GetDownloadStatus(download, &status)) ||
      status.State == DODownloadState_Aborted) {
    return true;
  }
  base::FilePath path;
  if (!HasDownloadDirectory(download, &path)) {
    return true;
  }
  if (local_path) {
    *local_path = path;
  }
  return false;
}

// DO does not expose the creation time of a download. The age of a download
// is approximated by the creation time of the temporary directory this module
// creates for each download. Unusable downloads are considered stale as well,
// irrespective of their age. Finalized downloads are only removed here, once
// they are old enough that their file can't be in use anymore.
bool DownloadOlderThanDaysPredicate(
    const Microsoft::WRL::ComPtr<IDODownload>& download,
    int num_days) {
  base::FilePath local_path;
  base::File::Info info;
  if (IsDownloadUnusable(download, &local_path) ||
      !base::GetFileInfo(local_path.DirName(), &info)) {
    return true;
  }
  return info.creation_time + base::Days(num_days) < base::Time::Now();
}

// Creates an instance of the DO manager. Fails with REGDB_E_CLASSNOTREG on
// hosts where Delivery Optimization is not available.
HRESULT CreateDeliveryOptimizationManager(
    Microsoft::WRL::ComPtr<IDOManager>* do_manager) {
  Microsoft::WRL::ComPtr<IDOManager> local_do_manager;
  HRESULT hr;
  {
    // CoCreateInstance may acquire the loader lock to load a library. Doing it
    // at background priority can cause a priority inversion with the main
    // thread, perceived as a hang by the user.
    // SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY() mitigates this problem
    // by boosting the thread's priority. See crbug.com/1295941.
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    hr = ::CoCreateInstance(__uuidof(DeliveryOptimization), nullptr,
                            CLSCTX_LOCAL_SERVER,
                            IID_PPV_ARGS(&local_do_manager));
  }

  if (FAILED(hr)) {
    VLOG(1) << "Delivery Optimization is not available: " << std::hex << hr;
    return hr;
  }
  *do_manager = local_do_manager;
  return S_OK;
}

void CleanupDownload(const Microsoft::WRL::ComPtr<IDODownload>& download) {
  if (!download) {
    return;
  }

  // Get the file path associated with this download before aborting it.
  // Aborting removes the download from the DO queue right away, so it is safer
  // to get the path first.
  base::FilePath local_path;
  const bool has_path = SUCCEEDED(GetDownloadLocalPath(download, &local_path));

  // Aborting a finalized download fails with DO_E_INVALID_STATE, since the
  // download is already in a final state; in that case, only the downloaded
  // file and its directory are removed below.
  download->Abort();

  // DO may report an empty local path, and an empty path must not be deleted
  // since its parent directory resolves to the current directory.
  if (has_path && !local_path.empty()) {
    DeleteFileAndEmptyParentDirectory(local_path);
  }
}

// Removes the downloads which can never be completed from the DO queue, so
// that they do not count toward the limit of queued downloads. Unlike the
// stale download sweep, this clean up is not throttled since it only looks at
// the downloads already in the queue, which are few.
void CleanupUnusableDownloads(
    const Microsoft::WRL::ComPtr<IDOManager>& do_manager) {
  std::vector<Microsoft::WRL::ComPtr<IDODownload>> downloads;
  FindDownloadsIf(
      [](const Microsoft::WRL::ComPtr<IDODownload>& download) {
        return IsDownloadUnusable(download);
      },
      do_manager, &downloads);
  for (const auto& download : downloads) {
    CleanupDownload(download);
  }
}

void CheckIsMta() {
  CHECK_EQ(base::win::GetComApartmentTypeForThread(),
           base::win::ComApartmentType::MTA);
}

}  // namespace

DeliveryOptimizationDownloader::DeliveryOptimizationDownloader(
    scoped_refptr<CrxDownloader> successor,
    const std::string& prod_id)
    : CrxDownloader(std::move(successor)),
      com_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          kTaskTraitsBackgroundDownloader)),
      prod_id_(base::UTF8ToWide(prod_id)) {
  DETACH_FROM_SEQUENCE(com_sequence_checker_);
}

DeliveryOptimizationDownloader::~DeliveryOptimizationDownloader() {
  DETACH_FROM_SEQUENCE(com_sequence_checker_);
}

// static
int DeliveryOptimizationDownloader::GetHttpStatusFromHresult(HRESULT error) {
  // DO's own error codes (DO_E_*, facility 0xD0) are different from the BITS
  // error codes (BG_E_*), but both services surface HTTP failures using the
  // HTTP_E_STATUS_* codes from winerror.h, which have the high word equal to
  // 0x8019 (FACILITY_HTTP) and the low word equal to the HTTP status code. The
  // upper bound is 599 rather than the last standard status code, so that any
  // 5xx code is classified as a server error by |IsHttpServerError|.
  static constexpr int kHttpStatusFirst = 100;  // Continue.
  static constexpr int kHttpStatusLast = 599;   // Last 5xx code.
  bool is_valid = HIWORD(error) == 0x8019 &&
                  LOWORD(error) >= kHttpStatusFirst &&
                  LOWORD(error) <= kHttpStatusLast;
  return is_valid ? LOWORD(error) : 0;
}

// static
HRESULT DeliveryOptimizationDownloader::GetDownloadError(
    const DO_DOWNLOAD_STATUS& status) {
  if (FAILED(status.Error)) {
    return status.Error;
  }
  if (FAILED(status.ExtendedError)) {
    return status.ExtendedError;
  }
  return S_OK;
}

// static
void DeliveryOptimizationDownloader::GetDownloadByteCount(
    const DO_DOWNLOAD_STATUS& status,
    int64_t* downloaded_bytes,
    int64_t* total_bytes) {
  *downloaded_bytes = -1;
  *total_bytes = -1;

  static constexpr uint64_t kMaxNumBytes =
      static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
  if (status.BytesTransferred <= kMaxNumBytes) {
    *downloaded_bytes = status.BytesTransferred;
  }
  if (status.BytesTotal != 0 && status.BytesTotal <= kMaxNumBytes) {
    *total_bytes = status.BytesTotal;
  }
}

void DeliveryOptimizationDownloader::StartTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_ = std::make_unique<base::OneShotTimer>();
  timer_->Start(FROM_HERE, base::Seconds(kDownloadPollingIntervalSec), this,
                &DeliveryOptimizationDownloader::OnTimer);
}

void DeliveryOptimizationDownloader::OnTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_ = nullptr;
  com_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeliveryOptimizationDownloader::OnDownloading, this));
}

base::OnceClosure DeliveryOptimizationDownloader::DoStartDownload(
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  com_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeliveryOptimizationDownloader::BeginDownload,
                                this, url));
  return base::DoNothing();
}

// Called one time when this class is asked to do a download.
void DeliveryOptimizationDownloader::BeginDownload(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  // This function is called once for each url the downloader is asked to try,
  // so all per-download state is reset here, and not only the timestamps.
  download_start_time_ = base::TimeTicks::Now();
  job_stuck_begin_time_ = download_start_time_;
  last_resume_time_ = download_start_time_;
  stuck_baseline_bytes_ = 0;
  last_status_ = {};
  downloaded_bytes_ = -1;
  total_bytes_ = -1;
  response_.clear();

  HRESULT hr = BeginDownloadHelper(url);
  if (FAILED(hr)) {
    EndDownload(hr);
    return;
  }

  VLOG(1) << "Starting Delivery Optimization download for: " << url.spec();

  main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DeliveryOptimizationDownloader::StartTimer, this));
}

// Creates or opens an existing DO download for the |url|.
HRESULT DeliveryOptimizationDownloader::BeginDownloadHelper(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  HRESULT hr = CreateDeliveryOptimizationManager(&do_manager_);
  if (FAILED(hr)) {
    return hr;
  }

  hr = QueueDownload(url, &download_);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

// Called any time the timer fires.
void DeliveryOptimizationDownloader::OnDownloading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  DO_DOWNLOAD_STATUS status = {};
  HRESULT hr = GetDownloadStatus(download_, &status);
  if (FAILED(hr)) {
    EndDownload(hr);
    return;
  }
  RecordStatus(status);

  bool is_handled = false;
  switch (status.State) {
    case DODownloadState_Transferred:
      is_handled = OnStateTransferred();
      break;

    case DODownloadState_Aborted:
      is_handled = OnStateAborted(status);
      break;

    case DODownloadState_Finalized:
      is_handled = OnStateFinalized();
      break;

    case DODownloadState_Created:
      is_handled = OnStateCreated();
      break;

    case DODownloadState_Paused:
      is_handled = OnStatePaused(status);
      break;

    case DODownloadState_Transferring:
      is_handled = OnStateTransferring(status);
      break;

    default:
      is_handled = OnStateUnknown();
      break;
  }

  if (is_handled) {
    return;
  }

  main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DeliveryOptimizationDownloader::StartTimer, this));
}

// Completes the DO download, picks up the file path of the response, and
// notifies the CrxDownloader. The function should be called only once.
// |http_status| is the HTTP status of the download, if the caller has already
// retrieved it, which avoids querying DO for it a second time.
void DeliveryOptimizationDownloader::EndDownload(
    HRESULT error,
    std::optional<int> http_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  const base::TimeTicks download_end_time(base::TimeTicks::Now());
  const base::TimeDelta download_time =
      download_end_time >= download_start_time_
          ? download_end_time - download_start_time_
          : base::TimeDelta();

  // The byte counts and errors are taken from |last_status_|, which is
  // recorded by |OnDownloading| before dispatching to the state handlers. It
  // is not refreshed here since a finalized download may no longer report its
  // byte counts, and since |download_| is null when the download could not be
  // started at all.

  // Consider the url handled if it has been successfully downloaded or a
  // 5xx has been received.
  if (FAILED(error) && !http_status) {
    http_status =
        GetDownloadHttpStatus(download_, error, last_status_.ExtendedError);
  }
  const bool is_handled =
      SUCCEEDED(error) || IsHttpServerError(http_status.value_or(0));

  if (FAILED(error)) {
    CleanupDownload(download_);
  }

  download_ = nullptr;
  do_manager_ = nullptr;

  const int error_to_report = SUCCEEDED(error) ? 0 : error;

  DownloadMetrics download_metrics;
  download_metrics.url = url();
  download_metrics.downloader = DownloadMetrics::kDeliveryOptimization;
  download_metrics.error = error_to_report;
  download_metrics.extra_code1 =
      FAILED(error) && last_status_.ExtendedError != error
          ? last_status_.ExtendedError
          : 0;
  download_metrics.downloaded_bytes = downloaded_bytes_;
  download_metrics.total_bytes = total_bytes_;
  download_metrics.download_time_ms = download_time.InMilliseconds();

  Result result;
  result.error = error_to_report;
  result.extra_code1 = download_metrics.extra_code1;
  if (!result.error) {
    result.response = response_;
  }
  main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DeliveryOptimizationDownloader::OnDownloadComplete, this,
                     is_handled, result, download_metrics));
}

// Called when the DO download has been transferred successfully. Completes the
// download by finalizing it and making the download available to the caller.
bool DeliveryOptimizationDownloader::OnStateTransferred() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();
  EndDownload(CompleteDownload());
  return true;
}

// Called when the download has been aborted, either by DO after a
// non-recoverable error or by an external actor.
bool DeliveryOptimizationDownloader::OnStateAborted(
    const DO_DOWNLOAD_STATUS& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();
  const HRESULT error = GetDownloadError(status);
  EndDownload(FAILED(error) ? error : E_UNEXPECTED);
  return true;
}

// Called when the download was finalized by someone else. This notification is
// not seen in the current implementation but provided here as a defensive
// programming measure.
bool DeliveryOptimizationDownloader::OnStateFinalized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();
  EndDownload(E_UNEXPECTED);
  return true;
}

bool DeliveryOptimizationDownloader::OnStateCreated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  if (!IsStuck()) {
    return false;
  }

  // Terminate the download if it has not started transferring in a while.
  EndDownload(E_ABORT);
  return true;
}

// Called when DO reports a state this module does not know about, which could
// happen if a future version of DO extends the state machine. The download is
// given the benefit of the doubt for a while, then it is abandoned so that the
// successor in the chain gets a chance to download the file.
bool DeliveryOptimizationDownloader::OnStateUnknown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  if (!IsStuck()) {
    return false;
  }

  EndDownload(E_UNEXPECTED);
  return true;
}

// Called when the download has been paused by DO after a transient error, such
// as a network disconnect, a server error, or some other recoverable error.
bool DeliveryOptimizationDownloader::OnStatePaused(
    const DO_DOWNLOAD_STATUS& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  const HRESULT error = GetDownloadError(status);

  // Don't retry at all if the transient error was a 5xx.
  if (FAILED(error)) {
    const int http_status =
        GetDownloadHttpStatus(download_, status.Error, status.ExtendedError);
    if (IsHttpServerError(http_status)) {
      EndDownload(error, http_status);
      return true;
    }
  }

  if (IsStuck()) {
    EndDownload(FAILED(error) ? error : E_ABORT);
    return true;
  }

  // Try to resume the download, but not more often than once per
  // |kMinimumRetryDelayMin|.
  const base::TimeTicks now = base::TimeTicks::Now();
  if (last_resume_time_ + base::Minutes(kMinimumRetryDelayMin) < now) {
    last_resume_time_ = now;
    download_->Start(nullptr);
  }

  return false;
}

bool DeliveryOptimizationDownloader::OnStateTransferring(
    const DO_DOWNLOAD_STATUS& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  int64_t downloaded_bytes = -1;
  int64_t total_bytes = -1;
  GetDownloadByteCount(status, &downloaded_bytes, &total_bytes);

  // Unlike BITS, DO reports the transferring state as soon as the download is
  // started or resumed, before any bytes have been received. The baseline for
  // detecting a stuck download is therefore only reset when the number of
  // bytes transferred grows, so that a download stalled in this state, or one
  // cycling between the paused and transferring states, still times out.
  if (downloaded_bytes > stuck_baseline_bytes_) {
    stuck_baseline_bytes_ = downloaded_bytes;
    job_stuck_begin_time_ = base::TimeTicks::Now();
  } else if (IsStuck()) {
    EndDownload(E_ABORT);
    return true;
  }

  main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DeliveryOptimizationDownloader::OnDownloadProgress, this,
                     downloaded_bytes, total_bytes));
  return false;
}

HRESULT DeliveryOptimizationDownloader::QueueDownload(
    const GURL& url,
    Microsoft::WRL::ComPtr<IDODownload>* download) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  // Remove the downloads which can't be completed and some old downloads from
  // the DO queue before creating new ones, so that they do not count toward
  // the limit of queued downloads. The former is done every time, since DO
  // may abort downloads in the background, while the latter is throttled.
  CleanupUnusableDownloads(do_manager_);
  CleanupStaleJobs();

  size_t num_downloads = std::numeric_limits<size_t>::max();
  HRESULT hr = GetDownloaderDownloadCount(&num_downloads);

  if (FAILED(hr) || num_downloads >= kMaxQueuedDownloads) {
    return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF,
                        CrxDownloaderError::DO_TOO_MANY_DOWNLOADS);
  }

  Microsoft::WRL::ComPtr<IDODownload> local_download;
  hr = CreateOrOpenDownload(url, &local_download);
  if (FAILED(hr)) {
    CleanupDownload(local_download);
    return hr;
  }

  if (hr == S_OK) {
    // A new download must be started with an empty ranges structure, which
    // requests the entire file.
    DO_DOWNLOAD_RANGES_INFO ranges = {};
    hr = local_download->Start(&ranges);
  } else {
    DO_DOWNLOAD_STATUS status = {};
    hr = GetDownloadStatus(local_download, &status);
    if (SUCCEEDED(hr)) {
      switch (status.State) {
        case DODownloadState_Created: {
          // The existing download has never been started, so it is started
          // from scratch; DO rejects resuming a download in this state.
          DO_DOWNLOAD_RANGES_INFO ranges = {};
          hr = local_download->Start(&ranges);
          break;
        }
        case DODownloadState_Transferring:
        case DODownloadState_Transferred:
          // The download is making progress or only needs to be finalized.
          // Leave it alone.
          break;
        case DODownloadState_Paused:
          // Don't force a retry of a download paused after a 5xx response.
          // |OnStatePaused| ends the download without retrying it, consistent
          // with the handling of downloads which fail with a 5xx while being
          // polled. Otherwise, resume the download with no change to the
          // requested ranges.
          if (!IsHttpServerError(GetDownloadHttpStatus(
                  local_download, status.Error, status.ExtendedError))) {
            hr = local_download->Start(nullptr);
          }
          break;
        default:
          hr = local_download->Start(nullptr);
          break;
      }
    }
  }
  if (FAILED(hr)) {
    CleanupDownload(local_download);
    return hr;
  }

  *download = local_download;
  return S_OK;
}

HRESULT DeliveryOptimizationDownloader::CreateOrOpenDownload(
    const GURL& url,
    Microsoft::WRL::ComPtr<IDODownload>* download) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  std::vector<Microsoft::WRL::ComPtr<IDODownload>> downloads;
  HRESULT hr = FindDownloadsIf(
      [&url](const Microsoft::WRL::ComPtr<IDODownload>& download) {
        return DownloadUrlEqualPredicate(download, url);
      },
      do_manager_, &downloads);
  if (SUCCEEDED(hr) && !downloads.empty()) {
    for (const auto& existing : downloads) {
      // Aborted downloads can't be resumed, and downloads whose temporary
      // directory is gone can't be completed. Such downloads are normally
      // removed by |CleanupUnusableDownloads| before getting here, but DO may
      // change the state of a download at any time. Clean them up so that a
      // new download can be created below.
      if (IsDownloadUnusable(existing)) {
        CleanupDownload(existing);
        continue;
      }
      // A finalized download can't be resumed either, but its file may still
      // be in use by the caller it was handed over to, so it is left alone for
      // the stale download sweep, and a new download is created below.
      DO_DOWNLOAD_STATUS status = {};
      if (FAILED(GetDownloadStatus(existing, &status)) ||
          status.State == DODownloadState_Finalized) {
        continue;
      }
      *download = existing;
      return S_FALSE;
    }
  }

  Microsoft::WRL::ComPtr<IDODownload> local_download;
  hr = do_manager_->CreateDownload(&local_download);
  if (FAILED(hr)) {
    CleanupDownload(local_download);
    return hr;
  }

  hr = InitializeNewDownload(local_download, url);
  if (FAILED(hr)) {
    CleanupDownload(local_download);
    return hr;
  }

  *download = local_download;
  return S_OK;
}

HRESULT DeliveryOptimizationDownloader::InitializeNewDownload(
    const Microsoft::WRL::ComPtr<IDODownload>& download,
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  base::FilePath tempdir;
  if (!base::CreateNewTempDirectory(
          base::StrCat({prod_id_, kDownloadDirectoryPrefix}), &tempdir)) {
    return E_FAIL;
  }

  // A url which ends in a slash has no file name, in which case a placeholder
  // name is used, so that the local path never refers to the temporary
  // directory itself.
  std::wstring filename(base::UTF8ToWide(url.ExtractFileName()));
  if (filename.empty()) {
    filename = kDefaultDownloadFilename;
  }
  const base::FilePath local_path = tempdir.Append(filename);

  // The local path is set first: once it is set, |CleanupDownload| is able to
  // find and remove the temporary directory if initializing or starting the
  // download fails later on. If setting the local path itself fails, nothing
  // references the temporary directory, so it is removed here.
  HRESULT hr =
      SetDownloadProperty(download, DODownloadProperty_LocalPath,
                          base::win::ScopedVariant(local_path.value().c_str()));
  if (FAILED(hr)) {
    base::DeletePathRecursively(tempdir);
    return hr;
  }

  hr = SetDownloadProperty(download, DODownloadProperty_DisplayName,
                           base::win::ScopedVariant(kDownloadName));
  if (FAILED(hr)) {
    return hr;
  }

  hr = SetDownloadProperty(
      download, DODownloadProperty_Uri,
      base::win::ScopedVariant(base::UTF8ToWide(url.spec()).c_str()));
  if (FAILED(hr)) {
    return hr;
  }

  // Background priority, which is the DO default. Set explicitly for clarity.
  hr = SetDownloadProperty(download, DODownloadProperty_ForegroundPriority,
                           base::win::ScopedVariant(false));
  if (FAILED(hr)) {
    return hr;
  }

  // The property is a VT_UI4 number of seconds.
  base::win::ScopedVariant no_progress_timeout;
  no_progress_timeout.Set(base::checked_cast<uint32_t>(
      base::Days(kNoProgressTimeoutDays).InSeconds()));
  hr =
      SetDownloadProperty(download, DODownloadProperty_NoProgressTimeoutSeconds,
                          no_progress_timeout);
  if (FAILED(hr)) {
    return hr;
  }

  // Ask DO to persist the download so that it survives restarts of the DO
  // service and of the machine, similar to a BITS job. The property is not
  // available on older versions of DO, in which case the failure is ignored
  // and the download is merely not persisted.
  SetDownloadProperty(download, DODownloadProperty_NonVolatile,
                      base::win::ScopedVariant(true));

  return S_OK;
}

bool DeliveryOptimizationDownloader::IsStuck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();
  const base::TimeDelta stuck_timeout(base::Minutes(kDownloadStuckTimeoutMin));
  return job_stuck_begin_time_ + stuck_timeout < base::TimeTicks::Now();
}

void DeliveryOptimizationDownloader::RecordStatus(
    const DO_DOWNLOAD_STATUS& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  last_status_ = status;

  // The byte counts only ever grow. A finalized or aborted download may report
  // zero byte counts, in which case the last known values are retained.
  int64_t downloaded_bytes = -1;
  int64_t total_bytes = -1;
  GetDownloadByteCount(status, &downloaded_bytes, &total_bytes);
  downloaded_bytes_ = std::max(downloaded_bytes_, downloaded_bytes);
  total_bytes_ = std::max(total_bytes_, total_bytes);
}

HRESULT DeliveryOptimizationDownloader::CompleteDownload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();

  // Check the post-conditions of a successful download before finalizing it,
  // so that an incomplete or empty download is never finalized, and so that a
  // failure here leaves the download in a state which can still be aborted and
  // cleaned up. |last_status_| was recorded by |OnDownloading| just before
  // this call and is used instead of querying DO again; this also preserves
  // the byte counts, which a finalized download may no longer report.
  CHECK_EQ(last_status_.State, DODownloadState_Transferred);

  // The byte counts must match since the entire file was requested.
  if (last_status_.BytesTotal == 0 ||
      last_status_.BytesTotal != last_status_.BytesTransferred) {
    return E_UNEXPECTED;
  }

  base::FilePath local_path;
  HRESULT hr = GetDownloadLocalPath(download_, &local_path);
  if (FAILED(hr)) {
    return hr;
  }
  if (local_path.empty() || !base::PathExists(local_path)) {
    return E_UNEXPECTED;
  }

  hr = download_->Finalize();
  if (FAILED(hr)) {
    return hr;
  }

  response_ = local_path;

  return S_OK;
}

HRESULT DeliveryOptimizationDownloader::GetDownloaderDownloadCount(
    size_t* num_downloads) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();
  CHECK(do_manager_);

  std::vector<Microsoft::WRL::ComPtr<IDODownload>> downloads;
  const HRESULT hr = FindDownloadsIf(
      [](const Microsoft::WRL::ComPtr<IDODownload>&) { return true; },
      do_manager_, &downloads);
  if (FAILED(hr)) {
    return hr;
  }

  *num_downloads = downloads.size();
  return S_OK;
}

void DeliveryOptimizationDownloader::CleanupStaleJobs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);
  CheckIsMta();
  CHECK(do_manager_);

  // The sweep is throttled process-wide. Each downloader instance runs on its
  // own COM sequence, so the shared state is guarded by a lock. The lock is
  // held only for the check-and-update of the timestamp, not for the sweep.
  {
    static base::NoDestructor<base::Lock> lock;
    static base::Time last_sweep;

    base::AutoLock auto_lock(*lock);
    const base::TimeDelta time_delta(
        base::Days(kPurgeStaleDownloadsIntervalBetweenChecksDays));
    const base::Time current_time(base::Time::Now());
    if (last_sweep + time_delta > current_time) {
      return;
    }
    last_sweep = current_time;
  }

  std::vector<Microsoft::WRL::ComPtr<IDODownload>> downloads;
  FindDownloadsIf(
      [](const Microsoft::WRL::ComPtr<IDODownload>& download) {
        return DownloadOlderThanDaysPredicate(download,
                                              kPurgeStaleDownloadsAfterDays);
      },
      do_manager_, &downloads);

  for (const auto& download : downloads) {
    CleanupDownload(download);
  }

  CleanupStaleDownloads();
}

void DeliveryOptimizationDownloader::CleanupStaleDownloads() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(com_sequence_checker_);

  base::FilePath dir;
  if (!base::GetSecureTempDirectory(&dir)) {
    return;
  }
  CleanupDirectoriesOlderThan(
      dir, base::StrCat({prod_id_, kDownloadDirectoryPrefixMatcher}),
      base::Days(kPurgeStaleDownloadsAfterDays));
}

}  // namespace update_client
