// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"

#include "base/functional/bind.h"
#import "base/ios/ios_util.h"
#include "base/task/thread_pool.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/url_constants.h"

#pragma mark - IOSImageDataFetcherWrapper

namespace image_fetcher {

IOSImageDataFetcherWrapper::IOSImageDataFetcherWrapper(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : image_data_fetcher_(url_loader_factory) {}

IOSImageDataFetcherWrapper::~IOSImageDataFetcherWrapper() = default;

void IOSImageDataFetcherWrapper::FetchImageDataWebpDecoded(
    const GURL& image_url,
    ImageDataFetcherBlock callback,
    bool send_cookies) {
  image_data_fetcher_.FetchImageData(image_url,
                                     CallbackForImageDataFetcher(callback),
                                     NO_TRAFFIC_ANNOTATION_YET, send_cookies);
}

void IOSImageDataFetcherWrapper::FetchImageDataWebpDecoded(
    const GURL& image_url,
    ImageDataFetcherBlock callback,
    const std::string& referrer,
    net::ReferrerPolicy referrer_policy,
    bool send_cookies) {
  DCHECK(callback);

  image_data_fetcher_.FetchImageData(
      image_url, CallbackForImageDataFetcher(callback), referrer,
      referrer_policy, NO_TRAFFIC_ANNOTATION_YET, send_cookies);
}

ImageDataFetcherCallback
IOSImageDataFetcherWrapper::CallbackForImageDataFetcher(
    ImageDataFetcherBlock callback) {
  return base::BindOnce(^(const std::string& image_data,
                          const RequestMetadata& metadata) {
    // Create a NSData from the returned data and notify the callback.
    NSData* data = [NSData dataWithBytes:image_data.data()
                                  length:image_data.size()];
    callback(data, metadata);
  });
}

}  // namespace image_fetcher
