// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_NET_NETWORK_FETCHER_LIBCURL_H_
#define CHROME_UPDATER_NET_NETWORK_FETCHER_LIBCURL_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/threading/sequence_bound.h"
#include "components/update_client/network.h"

class GURL;

namespace updater {

class LibcurlNetworkFetcherImpl;

// LibcurlNetworkFetcher wraps a `LibcurlNetworkFetcherImpl` in a
// `base::SequenceBound`. This allows the wrapped fetcher to run solely in a
// dedicated io sequence.
//
// A fetcher performs a single transfer, which is how update_client uses it:
// it creates one per request or download. Starting a second transfer is a
// programming error.
class LibcurlNetworkFetcher final : public update_client::NetworkFetcher {
 public:
  using ResponseStartedCallback =
      ::update_client::NetworkFetcher::ResponseStartedCallback;
  using ProgressCallback = ::update_client::NetworkFetcher::ProgressCallback;
  using PostRequestCompleteCallback =
      ::update_client::NetworkFetcher::PostRequestCompleteCallback;
  using DownloadToFileCompleteCallback =
      ::update_client::NetworkFetcher::DownloadToFileCompleteCallback;

  // Creates a fetcher backed by a new libcurl easy handle. Returns nullptr if
  // the handle cannot be initialized. `proxy` is a proxy in libcurl's syntax,
  // such as "http://proxy:3128". Empty means libcurl's defaults, which honor
  // the proxy environment variables.
  static std::unique_ptr<LibcurlNetworkFetcher> Create(
      std::string proxy = std::string());

  LibcurlNetworkFetcher() = delete;
  LibcurlNetworkFetcher(const LibcurlNetworkFetcher&) = delete;
  LibcurlNetworkFetcher& operator=(const LibcurlNetworkFetcher&) = delete;
  ~LibcurlNetworkFetcher() override;

  // Overrides for update_client::NetworkFetcher
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override;

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback)
      override;

 private:
  explicit LibcurlNetworkFetcher(
      base::SequenceBound<LibcurlNetworkFetcherImpl> impl);

  base::SequenceBound<LibcurlNetworkFetcherImpl> impl_;
  bool transfer_started_ = false;
};

}  // namespace updater

#endif  // CHROME_UPDATER_NET_NETWORK_FETCHER_LIBCURL_H_
