// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/url_fetcher_downloader.h"

#include <stdint.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/update_client/network.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

namespace update_client {

UrlFetcherDownloader::UrlFetcherDownloader(
    scoped_refptr<CrxDownloader> successor,
    scoped_refptr<NetworkFetcherFactory> network_fetcher_factory)
    : CrxDownloader(std::move(successor)),
      network_fetcher_factory_(network_fetcher_factory) {}

UrlFetcherDownloader::~UrlFetcherDownloader() = default;

base::OnceClosure UrlFetcherDownloader::DoStartDownload(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, kTaskTraits,
      base::BindOnce(&UrlFetcherDownloader::CreateDownloadDir, this),
      base::BindOnce(&UrlFetcherDownloader::StartURLFetch, this, url));
  return base::BindOnce(&UrlFetcherDownloader::Cancel, this);
}

void UrlFetcherDownloader::CreateDownloadDir() {
  base::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_url_fetcher_"),
                               &download_dir_);
}

void UrlFetcherDownloader::StartURLFetch(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (cancelled_ || download_dir_.empty()) {
    Result result;
    result.error =
        static_cast<int>(cancelled_ ? CrxDownloaderError::CANCELLED
                                    : CrxDownloaderError::NO_DOWNLOAD_DIR);

    DownloadMetrics download_metrics;
    download_metrics.url = url;
    download_metrics.downloader = DownloadMetrics::kUrlFetcher;
    download_metrics.error = -1;
    download_metrics.downloaded_bytes = -1;
    download_metrics.total_bytes = -1;
    download_metrics.download_time_ms = 0;

    main_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&UrlFetcherDownloader::OnDownloadComplete,
                                  this, false, result, download_metrics));
    return;
  }

  file_path_ = download_dir_.AppendASCII(url.ExtractFileName());
  network_fetcher_ = network_fetcher_factory_->Create();
  cancel_callback_ = network_fetcher_->DownloadToFile(
      url, file_path_,
      base::BindRepeating(&UrlFetcherDownloader::OnResponseStarted, this),
      base::BindRepeating(&UrlFetcherDownloader::OnDownloadProgress, this),
      base::BindOnce(&UrlFetcherDownloader::OnNetworkFetcherComplete, this));

  download_start_time_ = base::TimeTicks::Now();
}

void UrlFetcherDownloader::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cancelled_ = true;
  if (cancel_callback_) {
    std::move(cancel_callback_).Run();
  }
}

void UrlFetcherDownloader::OnNetworkFetcherComplete(int net_error,
                                                    int64_t content_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::TimeTicks download_end_time(base::TimeTicks::Now());
  const base::TimeDelta download_time =
      download_end_time >= download_start_time_
          ? download_end_time - download_start_time_
          : base::TimeDelta();

  // Consider a 5xx response from the server as an indication to terminate
  // the request and avoid overloading the server in this case.
  // is not accepting requests for the moment.
  int error = -1;
  int extra_code1 = 0;
  if (!net_error && response_code_ == 200) {
    error = 0;
  } else if (response_code_ != -1) {
    error = response_code_;
    extra_code1 = net_error;
  } else {
    error = net_error;
  }

  const bool is_handled = error == 0 || IsHttpServerError(error);

  Result result;
  result.error = error;
  result.extra_code1 = extra_code1;
  if (!error) {
    result.response = file_path_;
  }

  DownloadMetrics download_metrics;
  download_metrics.url = url();
  download_metrics.downloader = DownloadMetrics::kUrlFetcher;
  download_metrics.error = error;
  download_metrics.extra_code1 = extra_code1;
  // Tests expected -1, in case of failures and no content is available.
  download_metrics.downloaded_bytes = error ? -1 : content_size;
  download_metrics.total_bytes = total_bytes_;
  download_metrics.download_time_ms = download_time.InMilliseconds();

  VLOG(1) << "Downloaded " << content_size << " bytes in "
          << download_time.InMilliseconds() << "ms from " << url().spec()
          << " to " << result.response.value();

  // Delete the download directory in the error cases.
  if (error && !download_dir_.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, kTaskTraits,
        base::BindOnce(IgnoreResult(&RetryDeletePathRecursively),
                       download_dir_));
  }

  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&UrlFetcherDownloader::OnDownloadComplete, this,
                                is_handled, result, download_metrics));
  network_fetcher_ = nullptr;
}

// This callback is used to indicate that a download has been started.
void UrlFetcherDownloader::OnResponseStarted(int response_code,
                                             int64_t content_length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << "url fetcher response started for: " << url().spec();

  response_code_ = response_code;
  total_bytes_ = content_length;
}

void UrlFetcherDownloader::OnDownloadProgress(int64_t current) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CrxDownloader::OnDownloadProgress(current, total_bytes_);
}

}  // namespace update_client
