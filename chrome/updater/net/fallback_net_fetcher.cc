// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/net/fallback_net_fetcher.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "components/update_client/cancellation.h"
#include "components/update_client/network.h"
#include "url/gurl.h"

namespace updater {

FallbackNetFetcher::FallbackNetFetcher(
    std::unique_ptr<update_client::NetworkFetcher> impl,
    std::unique_ptr<update_client::NetworkFetcher> next)
    : impl_(std::move(impl)), next_(std::move(next)) {}

FallbackNetFetcher::~FallbackNetFetcher() = default;

void FallbackNetFetcher::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback,
    update_client::NetworkFetcher::PostRequestCompleteCallback
        post_request_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  impl_->PostRequest(
      url, post_data, content_type, post_additional_headers,
      base::BindRepeating(
          &FallbackNetFetcher::ResponseStarted,
          base::Unretained(this),  // Assume the fetcher outlives the fetch.
          response_started_callback),
      progress_callback,
      base::BindOnce(
          &FallbackNetFetcher::PostRequestDone,
          base::Unretained(this),  // Assume the fetcher outlives the fetch.
          url, post_data, content_type, post_additional_headers,
          response_started_callback, progress_callback,
          std::move(post_request_complete_callback)));
}

void FallbackNetFetcher::ResponseStarted(
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    int32_t http_status_code,
    int64_t content_length) {
  http_status_code_ = http_status_code;
  response_started_callback.Run(http_status_code, content_length);
}

void FallbackNetFetcher::PostRequestDone(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback,
    update_client::NetworkFetcher::PostRequestCompleteCallback
        post_request_complete_callback,
    std::unique_ptr<std::string> response_body,
    int net_error,
    const std::string& header_etag,
    const std::string& header_x_cup_server_proof,
    int64_t xheader_retry_after_sec) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const int should_fallback = net_error || (http_status_code_ != 200);
  if (should_fallback && next_) {
    VLOG(1) << __func__ << " Falling back to next NetFetcher for " << url
            << ", error: " << net_error
            << ", HTTP status: " << http_status_code_;
    next_->PostRequest(url, post_data, content_type, post_additional_headers,
                       response_started_callback, progress_callback,
                       std::move(post_request_complete_callback));
    return;
  }
  std::move(post_request_complete_callback)
      .Run(std::move(response_body), net_error, header_etag,
           header_x_cup_server_proof, xheader_retry_after_sec);
}

base::OnceClosure FallbackNetFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback,
    update_client::NetworkFetcher::DownloadToFileCompleteCallback
        download_to_file_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cancellation_->OnCancel(impl_->DownloadToFile(
      url, file_path,
      base::BindRepeating(
          &FallbackNetFetcher::ResponseStarted,
          base::Unretained(this),  // Assume the fetcher outlives the fetch.
          response_started_callback),
      progress_callback,
      base::BindOnce(
          &FallbackNetFetcher::DownloadToFileDone,
          base::Unretained(this),  // Assume the fetcher outlives the fetch.
          url, file_path, response_started_callback, progress_callback,
          std::move(download_to_file_complete_callback))));
  return base::BindOnce(&update_client::Cancellation::Cancel, cancellation_);
}

void FallbackNetFetcher::DownloadToFileDone(
    const GURL& url,
    const base::FilePath& file_path,
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback,
    update_client::NetworkFetcher::DownloadToFileCompleteCallback
        download_to_file_complete_callback,
    int net_error,
    int64_t content_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cancellation_->Clear();
  const int should_fallback = net_error || (http_status_code_ != 200);
  if (should_fallback && next_) {
    VLOG(1) << __func__ << " Falling back to next NetFetcher for " << url;
    cancellation_->OnCancel(next_->DownloadToFile(
        url, file_path, response_started_callback, progress_callback,
        std::move(download_to_file_complete_callback)));
    return;
  }
  std::move(download_to_file_complete_callback).Run(net_error, content_size);
}

}  // namespace updater
