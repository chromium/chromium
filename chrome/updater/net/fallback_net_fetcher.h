// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_NET_FALLBACK_NET_FETCHER_H_
#define CHROME_UPDATER_NET_FALLBACK_NET_FETCHER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "components/update_client/cancellation.h"
#include "components/update_client/network.h"

class GURL;

namespace base {
class FilePath;
}

namespace updater {

// A `FallbackNetFecher` contains a number of `NetworkFetchers` and falls back
// between them when one encounters an error.
class FallbackNetFetcher : public update_client::NetworkFetcher {
 public:
  FallbackNetFetcher() = delete;
  FallbackNetFetcher(const FallbackNetFetcher&) = delete;
  FallbackNetFetcher& operator=(const FallbackNetFetcher&) = delete;
  ~FallbackNetFetcher() override;
  FallbackNetFetcher(std::unique_ptr<update_client::NetworkFetcher> impl,
                     std::unique_ptr<update_client::NetworkFetcher> next);

  // Overrides for update_client::NetworkFetcher
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::PostRequestCompleteCallback
          post_request_complete_callback) override;

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::DownloadToFileCompleteCallback
          download_to_file_complete_callback) override;

 private:
  void ResponseStarted(update_client::NetworkFetcher::ResponseStartedCallback
                           response_started_callback,
                       int32_t http_status_code,
                       int64_t content_length);
  void PostRequestDone(
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
      int64_t xheader_retry_after_sec);

  void DownloadToFileDone(
      const GURL& url,
      const base::FilePath& file_path,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::DownloadToFileCompleteCallback
          download_to_file_complete_callback,
      int net_error,
      int64_t content_size);

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<update_client::NetworkFetcher> impl_;
  std::unique_ptr<update_client::NetworkFetcher> next_;
  int32_t http_status_code_ = 200;
  scoped_refptr<update_client::Cancellation> cancellation_ =
      base::MakeRefCounted<update_client::Cancellation>();
};

}  // namespace updater

#endif  // CHROME_UPDATER_NET_FALLBACK_NET_FETCHER_H_
