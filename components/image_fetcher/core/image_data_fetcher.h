// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_DATA_FETCHER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_DATA_FETCHER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/referrer_policy.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace image_fetcher {

class ImageDataFetcher {
 public:
  // Note that this must be used consistently on the thread that owns
  // |url_loader_factory|. See SharedURLLoaderFactory::Clone() if changing
  // thread is required.
  explicit ImageDataFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ImageDataFetcher(const ImageDataFetcher&) = delete;
  ImageDataFetcher& operator=(const ImageDataFetcher&) = delete;

  ~ImageDataFetcher();

  // Sets an upper limit for image downloads.
  // Already running downloads are not affected.
  void SetImageDownloadLimit(std::optional<int64_t> max_download_bytes);

  // Fetches the raw image bytes from the given |image_url| and calls the given
  // |callback|. The callback is run even if fetching the URL fails. In case
  // of an error an empty string is passed to the callback. May return
  // synchronously.
  void FetchImageData(const GURL& image_url,
                      ImageDataFetcherCallback callback,
                      ImageFetcherParams params,
                      bool send_cookies = false);

  // Like above, but lets the caller set a referrer.
  void FetchImageData(const GURL& image_url,
                      ImageDataFetcherCallback callback,
                      ImageFetcherParams params,
                      const std::string& referrer,
                      net::ReferrerPolicy referrer_policy,
                      bool send_cookies = false);

  // Like above, but supports providing only a traffic annotation.
  void FetchImageData(
      const GURL& image_url,
      ImageDataFetcherCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      bool send_cookies = false);

  // Like above, but supports providing only a traffic annotation.
  void FetchImageData(
      const GURL& image_url,
      ImageDataFetcherCallback callback,
      const std::string& referrer,
      net::ReferrerPolicy referrer_policy,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      bool send_cookies = false);

  // Test-only method to inject a fetch result directly, w/o regard for how the
  // underlying loading is doing. This requires there to be a single pending
  // fetch only.
  void InjectResultForTesting(const RequestMetadata& metadata,
                              const std::string& image_data);

 private:
  struct ImageDataFetcherRequest;

  void OnURLLoaderComplete(const network::SimpleURLLoader* source,
                           ImageFetcherParams params,
                           std::unique_ptr<std::string> response_body);

  void FinishRequest(const network::SimpleURLLoader* source,
                     const RequestMetadata& metadata,
                     const std::string& image_data);

  // All active image url requests.
  std::map<const network::SimpleURLLoader*,
           std::unique_ptr<ImageDataFetcherRequest>>
      pending_requests_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Upper limit for the number of bytes to download per image.
  std::optional<int64_t> max_download_bytes_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_DATA_FETCHER_H_
