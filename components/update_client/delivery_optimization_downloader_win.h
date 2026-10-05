// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_DELIVERY_OPTIMIZATION_DOWNLOADER_WIN_H_
#define COMPONENTS_UPDATE_CLIENT_DELIVERY_OPTIMIZATION_DOWNLOADER_WIN_H_

#include <windows.h>

#include <deliveryoptimization.h>
#include <stddef.h>
#include <stdint.h>
#include <wrl/client.h>

#include <memory>
#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/update_client/crx_downloader.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

class GURL;

namespace update_client {

// When enabled, the Windows CRX downloader chain is
// Delivery Optimization -> BITS -> URL fetcher, instead of BITS -> URL fetcher.
// Delivery Optimization is probed at run time: if the service or its COM
// class is not available on the host, the download is handed over to the
// successor (BITS) transparently.
BASE_DECLARE_FEATURE(kDeliveryOptimizationDownloader);

// Implements a downloader in terms of the Windows Delivery Optimization (DO)
// service. The public interface of this class and the CrxDownloader overrides
// are expected to be called from the main sequence. The rest of the class code
// runs on a sequenced task runner which is initialized by default as an MTA by
// the thread pool.
//
// The design mirrors the BITS-based BackgroundDownloader: the class manages a
// COM client for DO and uses polling, triggered by an one-shot timer, to get
// state updates. Since the timer has sequence affinity for the main sequence,
// the callbacks from the timer are delegated to a sequenced task runner, which
// handles all client COM interaction with the DO service.
class DeliveryOptimizationDownloader : public CrxDownloader {
 public:
  explicit DeliveryOptimizationDownloader(
      scoped_refptr<CrxDownloader> successor,
      const std::string& prod_id);

  // Pure helpers used by the state machine. Exposed for testing.
  //
  // Returns the status code from a given HRESULT, if the HRESULT encodes an
  // HTTP status (HTTP_E_STATUS_* codes), or 0 otherwise.
  static int GetHttpStatusFromHresult(HRESULT error);

  // Returns the failure HRESULT associated with a download status, if any. DO
  // reports its own DO_E_* code in |Error| and often places the underlying
  // error, such as an HTTP_E_STATUS_* code, in |ExtendedError|. |Error| takes
  // precedence; |ExtendedError| is consulted when |Error| is not a failure.
  // Returns S_OK if neither field is a failure.
  static HRESULT GetDownloadError(const DO_DOWNLOAD_STATUS& status);

  // Returns the number of bytes downloaded and bytes to download for the
  // download. If the values are not known or if an error has occurred, a
  // value of -1 is reported.
  static void GetDownloadByteCount(const DO_DOWNLOAD_STATUS& status,
                                   int64_t* downloaded_bytes,
                                   int64_t* total_bytes);

 private:
  friend class DeliveryOptimizationDownloaderWinTest;
  FRIEND_TEST_ALL_PREFIXES(DeliveryOptimizationDownloaderWinTest,
                           CleansStaleDownloads);
  FRIEND_TEST_ALL_PREFIXES(DeliveryOptimizationDownloaderWinTest,
                           RetainsRecentDownloads);

  // Overrides for CrxDownloader.
  ~DeliveryOptimizationDownloader() override;
  base::OnceClosure DoStartDownload(const GURL& url) override;

  // Called asynchronously on the |com_task_runner_| at different stages during
  // the download. |OnDownloading| can be called multiple times.
  // |EndDownload| switches the execution flow from the |com_task_runner_| to
  // the main sequence. |http_status| may be provided by callers which have
  // already retrieved the HTTP status of the download.
  void BeginDownload(const GURL& url);
  void OnDownloading();
  void EndDownload(HRESULT hr, std::optional<int> http_status = std::nullopt);

  HRESULT BeginDownloadHelper(const GURL& url);

  // Handles the download state transitions to a final state. Returns true
  // always since the download has reached a final state and no further
  // processing for this download is needed.
  bool OnStateTransferred();
  bool OnStateAborted(const DO_DOWNLOAD_STATUS& status);
  bool OnStateFinalized();

  // Handles the transition to a transient state where the download has been
  // started but is not actively transferring data. Returns true if the
  // download has been in this state for too long and it will be abandoned, or
  // false, if further processing for this download is needed.
  bool OnStateCreated();

  // Handles a download state unknown to this module. Returns true if the
  // download has been in this state for too long and it will be abandoned, or
  // false, if further processing for this download is needed.
  bool OnStateUnknown();

  // Handles the download state transition to the paused state, which DO enters
  // on demand or after a transient error. The state may or may not be
  // considered final, depending on the error. Returns true if the state is
  // final, or false, if the download is allowed to continue.
  bool OnStatePaused(const DO_DOWNLOAD_STATUS& status);

  // Handles the download state corresponding to transferring data. Returns
  // false always since this is never a final state.
  bool OnStateTransferring(const DO_DOWNLOAD_STATUS& status);

  void StartTimer();
  void OnTimer();

  // Creates or opens a download for the given url and starts it. Returns S_OK
  // if a new download was created or S_FALSE if an existing download for the
  // |url| was found in the DO queue.
  HRESULT QueueDownload(const GURL& url,
                        Microsoft::WRL::ComPtr<IDODownload>* download);
  HRESULT CreateOrOpenDownload(const GURL& url,
                               Microsoft::WRL::ComPtr<IDODownload>* download);
  HRESULT InitializeNewDownload(
      const Microsoft::WRL::ComPtr<IDODownload>& download,
      const GURL& url);

  // Returns true if at the time of the call, it appears that the download
  // has not been making progress toward completion.
  bool IsStuck();

  // Caches the download status and the byte counts it reports, so that they
  // are available for reporting after the download has been finalized.
  void RecordStatus(const DO_DOWNLOAD_STATUS& status);

  // Makes the downloaded file available to the caller by finalizing the
  // download and picking up its local path.
  HRESULT CompleteDownload();

  // Returns the number of downloads in the DO queue which were created by this
  // downloader.
  HRESULT GetDownloaderDownloadCount(size_t* num_downloads);

  // Cleans up incompleted downloads that are too old.
  void CleanupStaleJobs();

  // Performs a best-effort cleanup of downloads that are too old.
  void CleanupStaleDownloads();

  // This sequence checker is bound to the main sequence.
  SEQUENCE_CHECKER(sequence_checker_);
  SEQUENCE_CHECKER(com_sequence_checker_);

  // Executes blocking COM calls to DO.
  scoped_refptr<base::SequencedTaskRunner> com_task_runner_;

  // The timer has sequence affinity. This member is created and destroyed
  // on the main task runner.
  std::unique_ptr<base::OneShotTimer> timer_;

  // Valid only in the MTA associated with `com_task_runner_`;
  Microsoft::WRL::ComPtr<IDOManager> do_manager_;
  Microsoft::WRL::ComPtr<IDODownload> download_;

  // Contains the time when the download of the current url has started.
  base::TimeTicks download_start_time_;

  // Contains the time when the DO download is last seen making progress, and
  // the number of bytes transferred at that time. The download only counts as
  // making progress when the number of bytes transferred grows.
  base::TimeTicks job_stuck_begin_time_;
  int64_t stuck_baseline_bytes_ = 0;

  // Contains the time when the download was last resumed after a transient
  // error, so that resumes are rate limited.
  base::TimeTicks last_resume_time_;

  // The last download status retrieved from DO, and the largest byte counts
  // seen so far. These are retained for reporting since a finalized download
  // may no longer report its byte counts.
  DO_DOWNLOAD_STATUS last_status_ = {};
  int64_t downloaded_bytes_ = -1;
  int64_t total_bytes_ = -1;

  // Contains the path of the downloaded file if the download was successful.
  base::FilePath response_;

  // Used as a prefix for temporary directories.
  const base::FilePath::StringType prod_id_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_DELIVERY_OPTIMIZATION_DOWNLOADER_WIN_H_
