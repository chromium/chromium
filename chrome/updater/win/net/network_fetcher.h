// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_NET_NETWORK_FETCHER_H_
#define CHROME_UPDATER_WIN_NET_NETWORK_FETCHER_H_

#include <windows.h>

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/update_client/network.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class SingleThreadTaskRunner;
}  // namespace base

namespace winhttp {
class NetworkFetcher;
class ProxyConfiguration;
}  // namespace winhttp

namespace updater {

class NetworkFetcher : public update_client::NetworkFetcher {
 public:
  using ResponseStartedCallback =
      update_client::NetworkFetcher::ResponseStartedCallback;
  using ProgressCallback = update_client::NetworkFetcher::ProgressCallback;
  using PostRequestCompleteCallback =
      update_client::NetworkFetcher::PostRequestCompleteCallback;
  using DownloadToFileCompleteCallback =
      update_client::NetworkFetcher::DownloadToFileCompleteCallback;

  NetworkFetcher(const HINTERNET& session_handle,
                 scoped_refptr<winhttp::ProxyConfiguration> proxy_config);
  ~NetworkFetcher() override;
  NetworkFetcher(const NetworkFetcher&) = delete;
  NetworkFetcher& operator=(const NetworkFetcher&) = delete;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override;
  void DownloadToFile(const GURL& url,
                      const base::FilePath& file_path,
                      ResponseStartedCallback response_started_callback,
                      ProgressCallback progress_callback,
                      DownloadToFileCompleteCallback
                          download_to_file_complete_callback) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  void PostRequestComplete(int response_code);
  void DownloadToFileComplete(int response_code);

  scoped_refptr<winhttp::NetworkFetcher> winhttp_network_fetcher_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DownloadToFileCompleteCallback download_to_file_complete_callback_;
  PostRequestCompleteCallback post_request_complete_callback_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_NET_NETWORK_FETCHER_H_
