// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_downloader.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#if defined(OS_WIN)
#include "components/update_client/background_downloader_win.h"
#endif
#include "components/update_client/network.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/url_fetcher_downloader.h"
#include "components/update_client/utils.h"

namespace update_client {

CrxDownloader::DownloadMetrics::DownloadMetrics()
    : downloader(kNone),
      error(0),
      downloaded_bytes(-1),
      total_bytes(-1),
      download_time_ms(0) {
}

// On Windows, the first downloader in the chain is a background downloader,
// which uses the BITS service.
std::unique_ptr<CrxDownloader> CrxDownloader::Create(
    bool is_background_download,
    scoped_refptr<NetworkFetcherFactory> network_fetcher_factory) {
  std::unique_ptr<CrxDownloader> url_fetcher_downloader =
      std::make_unique<UrlFetcherDownloader>(nullptr, network_fetcher_factory);

#if defined(OS_WIN)
  if (is_background_download) {
    return std::make_unique<BackgroundDownloader>(
        std::move(url_fetcher_downloader));
  }
#endif

  return url_fetcher_downloader;
}

CrxDownloader::CrxDownloader(std::unique_ptr<CrxDownloader> successor)
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      successor_(std::move(successor)) {}

CrxDownloader::~CrxDownloader() {}

void CrxDownloader::set_progress_callback(
    const ProgressCallback& progress_callback) {
  progress_callback_ = progress_callback;
}

GURL CrxDownloader::url() const {
  return current_url_ != urls_.end() ? *current_url_ : GURL();
}

const std::vector<CrxDownloader::DownloadMetrics>
CrxDownloader::download_metrics() const {
  if (!successor_)
    return download_metrics_;

  std::vector<DownloadMetrics> retval(successor_->download_metrics());
  retval.insert(retval.begin(), download_metrics_.begin(),
                download_metrics_.end());
  return retval;
}

void CrxDownloader::StartDownloadFromUrl(const GURL& url,
                                         const std::string& expected_hash,
                                         DownloadCallback download_callback) {
  std::vector<GURL> urls;
  urls.push_back(url);
  StartDownload(urls, expected_hash, std::move(download_callback));
}

void CrxDownloader::StartDownload(const std::vector<GURL>& urls,
                                  const std::string& expected_hash,
                                  DownloadCallback download_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

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
    return;
  }

  urls_ = urls;
  expected_hash_ = expected_hash;
  current_url_ = urls_.begin();
  download_callback_ = std::move(download_callback);

  DoStartDownload(*current_url_);
}

void CrxDownloader::OnDownloadComplete(
    bool is_handled,
    const Result& result,
    const DownloadMetrics& download_metrics) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!result.error)
    base::PostTask(
        FROM_HERE, kTaskTraits,
        base::BindOnce(&CrxDownloader::VerifyResponse, base::Unretained(this),
                       is_handled, result, download_metrics));
  else
    main_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&CrxDownloader::HandleDownloadError,
                                  base::Unretained(this), is_handled, result,
                                  download_metrics));
}

void CrxDownloader::OnDownloadProgress() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (progress_callback_.is_null())
    return;

  progress_callback_.Run();
}

// The function mutates the values of the parameters |result| and
// |download_metrics|.
void CrxDownloader::VerifyResponse(bool is_handled,
                                   Result result,
                                   DownloadMetrics download_metrics) {
  DCHECK_EQ(0, result.error);
  DCHECK_EQ(0, download_metrics.error);
  DCHECK(is_handled);

  if (VerifyFileHash256(result.response, expected_hash_)) {
    download_metrics_.push_back(download_metrics);
    main_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(download_callback_), result));
    return;
  }

  // The download was successful but the response is not trusted. Clean up
  // the download, mutate the result, and try the remaining fallbacks when
  // handling the error.
  result.error = static_cast<int>(CrxDownloaderError::BAD_HASH);
  download_metrics.error = result.error;
  DeleteFileAndEmptyParentDirectory(result.response);
  result.response.clear();

  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CrxDownloader::HandleDownloadError,
                                base::Unretained(this), is_handled, result,
                                download_metrics));
}

void CrxDownloader::HandleDownloadError(
    bool is_handled,
    const Result& result,
    const DownloadMetrics& download_metrics) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(0, result.error);
  DCHECK(result.response.empty());
  DCHECK_NE(0, download_metrics.error);

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
