// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_NET_NETWORK_FETCHER_H_
#define CHROME_UPDATER_MAC_NET_NETWORK_FETCHER_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/network.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace updater {

class NetworkFetcher : public update_client::NetworkFetcher {
 public:
  NetworkFetcher();
  NetworkFetcher& operator=(const NetworkFetcher&) = delete;
  NetworkFetcher(const NetworkFetcher&) = delete;
  ~NetworkFetcher() override;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::PostRequestCompleteCallback
          post_request_complete_callback) override;

  void DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::DownloadToFileCompleteCallback
          download_to_file_complete_callback) override;

 private:
  THREAD_CHECKER(thread_checker_);
};

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_NET_NETWORK_FETCHER_H_
