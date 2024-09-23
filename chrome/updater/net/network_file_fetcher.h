// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_NET_NETWORK_FILE_FETCHER_H_
#define CHROME_UPDATER_NET_NETWORK_FILE_FETCHER_H_

#include "base/files/file.h"
#include "base/sequence_checker.h"
#include "components/update_client/network.h"

class GURL;

namespace updater {

// A customized fetcher that takes a `base::File` argument as the
// output destination.
class NetworkFileFetcher {
 public:
  NetworkFileFetcher() = default;
  NetworkFileFetcher& operator=(const NetworkFileFetcher&) = delete;
  NetworkFileFetcher(const NetworkFileFetcher&) = delete;
  ~NetworkFileFetcher() = default;

  base::OnceClosure Download(
      const GURL& url,
      base::File output,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::DownloadToFileCompleteCallback
          download_to_file_complete_callback);

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace updater

#endif  // CHROME_UPDATER_NET_NETWORK_FILE_FETCHER_H_
