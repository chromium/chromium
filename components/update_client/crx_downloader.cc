// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_downloader.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_WIN)
#include "components/update_client/background_downloader_win.h"
#endif
#include "components/update_client/network.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_client_metrics.h"
#include "components/update_client/url_fetcher_downloader.h"
#include "components/update_client/utils.h"

namespace update_client {

CrxDownloader::CrxDownloader(scoped_refptr<CrxDownloader> successor)
    : main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      successor_(std::move(successor)) {}

CrxDownloader::~CrxDownloader() = default;

void CrxDownloader::set_progress_callback(
    const ProgressCallback& progress_callback) {
  progress_callback_ = progress_callback;
}

GURL CrxDownloader::url() const {
  return current_url_ != urls_.end() ? *current_url_ : GURL();
}

const std::vector<CrxDownloader::DownloadMetrics>
CrxDownloader::download_metrics() const {
  if (!successor_) {
    return download_metrics_;
  }

  std::vector<DownloadMetrics> retval(successor_->download_metrics());
  retval.insert(retval.begin(), download_metrics_.begin(),
                download_metrics_.end());
  return retval;
}

base::OnceClosure CrxDownloader::StartDownloadFromUrl(
    const GURL& url,
    const std::string& expected_hash,
    DownloadCallback download_callback) {
  std::vector<GURL> urls;
  urls.push_back(url);
  return StartDownload(urls, expected_hash, std::move(download_callback));
}

base::OnceClosure CrxDownloader::StartDownload(
    const std::vector<GURL>& urls,
    const std::string& expected_hash,
    DownloadCallback download_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto error = CrxDownloaderError::NONE;
  if (urls.empty()) {
    error = CrxDownloaderError::NO_URL;
  } else if (expected_hash.empty()) {
    error = CrxDownloaderError::NO_HASH;
  }

  if (error != CrxDownloaderError::NONE) {
    Result result;
    result.error = static_cast<int>(error);
    main_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(download_callback), result));
    return base::DoNothing();
  }

  urls_ = urls;
  expected_hash_ = expected_hash;
  current_url_ = urls_.begin();
  download_callback_ = std::move(download_callback);

  return DoStartDownload(*current_url_);
}

void CrxDownloader::OnDownloadComplete(
    bool is_handled,
    const Result& result,
    const DownloadMetrics& download_metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Release any references held by the progress callback, in case the
  // CrxDownloader outlives the receiver of the progress_callback. (This is
  // often the case in tests.)
  progress_callback_.Reset();

  metrics::RecordCRXDownloadComplete(result.error);
  if (result.error) {
    main_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&CrxDownloader::HandleDownloadError, this,
                                  is_handled, result, download_metrics));
    return;
  }

  CHECK_EQ(0, download_metrics.error);
  CHECK(is_handled);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(
          // Verifies the hash of a CRX file. Returns NONE or BAD_HASH if
          // the hash of the CRX does not match the |expected_hash|. The input
          // file is deleted in case of errors.
          [](const base::FilePath& filepath, const std::string& expected_hash) {
            if (VerifyFileHash256(filepath, expected_hash)) {
              return CrxDownloaderError::NONE;
            }
            DeleteFileAndEmptyParentDirectory(filepath);
            return CrxDownloaderError::BAD_HASH;
          },
          result.response, expected_hash_),
      base::BindOnce(
          // Handles CRX verification result, and retries the download from
          // a different URL if the verification fails.
          [](scoped_refptr<CrxDownloader> downloader, Result result,
             DownloadMetrics download_metrics, CrxDownloaderError error) {
            if (error == CrxDownloaderError::NONE) {
              downloader->download_metrics_.push_back(download_metrics);
              downloader->main_task_runner()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(downloader->download_callback_),
                                 result));
              return;
            }
            result.response.clear();
            result.error = static_cast<int>(error);
            download_metrics.error = result.error;
            downloader->main_task_runner()->PostTask(
                FROM_HERE,
                base::BindOnce(&CrxDownloader::HandleDownloadError, downloader,
                               true, result, download_metrics));
          },
          scoped_refptr<CrxDownloader>(this), result, download_metrics));
}

void CrxDownloader::OnDownloadProgress(int64_t downloaded_bytes,
                                       int64_t total_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (progress_callback_.is_null()) {
    return;
  }

  progress_callback_.Run(downloaded_bytes, total_bytes);
}

void CrxDownloader::HandleDownloadError(
    bool is_handled,
    const Result& result,
    const DownloadMetrics& download_metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(0, result.error);
  CHECK(result.response.empty());
  CHECK_NE(0, download_metrics.error);

  download_metrics_.push_back(download_metrics);

  // If an error has occured, try the next url if there is any,
  // or try the successor in the chain if there is any successor.
  // If this downloader has received a 5xx error for the current url,
  // as indicated by the |is_handled| flag, remove that url from the list of
  // urls so the url is never tried again down the chain.
  if (is_handled) {
    current_url_ = urls_.erase(current_url_);
  } else {
    ++current_url_;
  }

  // Try downloading from another url from the list.
  if (current_url_ != urls_.end()) {
    DoStartDownload(*current_url_);
    return;
  }

  // Try downloading using the next downloader.
  if (successor_ && !urls_.empty()) {
    metrics::RecordCRXDownloaderFallback();
    successor_->StartDownload(urls_, expected_hash_,
                              std::move(download_callback_));
    return;
  }

  // The download ends here since there is no url nor downloader to handle this
  // download request further.
  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(download_callback_), result));
}

}  // namespace update_client
