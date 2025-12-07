// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/net/network.h"

#include "base/base64.h"
#include "chrome/updater/event_history.h"

namespace updater {

LoggingNetworkFetcher::LoggingNetworkFetcher(
    std::unique_ptr<update_client::NetworkFetcher> impl)
    : impl_(std::move(impl)) {}
LoggingNetworkFetcher::~LoggingNetworkFetcher() = default;

void LoggingNetworkFetcher::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  impl_->PostRequest(
      url, post_data, content_type, post_additional_headers,
      std::move(response_started_callback), progress_callback,
      base::BindOnce(
          [](PostRequestEndEvent event, PostRequestCompleteCallback next,
             std::optional<std::string> response_body, int net_error,
             const std::string& header_etag,
             const std::string& header_x_cup_server_proof,
             const std::string& header_set_cookie,
             int64_t xheader_retry_after_sec) {
            if (net_error) {
              event.AddError({.code = net_error});
            }
            if (response_body) {
              event.SetResponse(base::Base64Encode(*response_body));
            }
            event.WriteAsync();
            std::move(next).Run(std::move(response_body), net_error,
                                header_etag, header_x_cup_server_proof,
                                header_set_cookie, xheader_retry_after_sec);
          },
          PostRequestStartEvent()
              .SetRequest(base::Base64Encode(post_data))
              .WriteAsyncAndReturnEndEvent(),
          std::move(post_request_complete_callback)));
}

base::OnceClosure LoggingNetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  return impl_->DownloadToFile(
      url, file_path, std::move(response_started_callback), progress_callback,
      std::move(download_to_file_complete_callback));
}

}  // namespace updater
