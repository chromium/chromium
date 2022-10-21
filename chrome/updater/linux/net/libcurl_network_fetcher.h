// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_LINUX_NET_LIBCURL_NETWORK_FETCHER_H_
#define CHROME_UPDATER_LINUX_NET_LIBCURL_NETWORK_FETCHER_H_

#include <curl/curl.h>

#include <array>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_native_library.h"
#include "base/sequence_checker.h"
#include "components/update_client/network.h"
#include "url/gurl.h"

namespace updater {

class LibcurlNetworkFetcher : public update_client::NetworkFetcher {
 public:
  using ResponseStartedCallback =
      update_client::NetworkFetcher::ResponseStartedCallback;
  using ProgressCallback = update_client::NetworkFetcher::ProgressCallback;
  using PostRequestCompleteCallback =
      update_client::NetworkFetcher::PostRequestCompleteCallback;
  using DownloadToFileCompleteCallback =
      update_client::NetworkFetcher::DownloadToFileCompleteCallback;

  LibcurlNetworkFetcher() = delete;
  LibcurlNetworkFetcher(const LibcurlNetworkFetcher&) = delete;
  LibcurlNetworkFetcher& operator=(const LibcurlNetworkFetcher&) = delete;
  ~LibcurlNetworkFetcher() override;

  static std::unique_ptr<LibcurlNetworkFetcher> Create();

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
  struct LibcurlFunctionPtrs;
  SEQUENCE_CHECKER(sequence_checker_);

  LibcurlNetworkFetcher(CURL* curl,
                        base::ScopedNativeLibrary library,
                        std::unique_ptr<LibcurlFunctionPtrs> curl_functions);

  // Static callbacks for libcurl.
  static size_t CurlWriteStringCallback(void* data,
                                        size_t size,
                                        size_t nmemb,
                                        void* userp);
  static size_t CurlHeaderCallback(char* data,
                                   size_t size,
                                   size_t nmemb,
                                   void* userp);
  static size_t CurlWriteFileCallback(void* data,
                                      size_t size,
                                      size_t nmemb,
                                      void* userp);
  static int CurlTransferCallback(void* userp,
                                  curl_off_t dltotal,
                                  curl_off_t dlnow,
                                  curl_off_t ultotal,
                                  curl_off_t ulnow);

  void OnTransferInfo(curl_off_t total, curl_off_t current);

  base::raw_ptr<CURL> curl_;
  base::ScopedNativeLibrary library_;
  std::unique_ptr<LibcurlFunctionPtrs> curl_functions_;
  std::array<char, CURL_ERROR_SIZE> curl_error_buf_;

  size_t downloaded_bytes_ = 0;
  ResponseStartedCallback response_started_callback_;
  ProgressCallback progress_callback_;

  base::WeakPtrFactory<LibcurlNetworkFetcher> weak_factory_{this};
};

}  // namespace updater

#endif  // CHROME_UPDATER_LINUX_NET_LIBCURL_NETWORK_FETCHER_H_
