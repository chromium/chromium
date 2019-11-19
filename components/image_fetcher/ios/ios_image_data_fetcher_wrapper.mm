// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#import "components/image_fetcher/ios/webp_decoder.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - IOSImageDataFetcherWrapper

namespace image_fetcher {

IOSImageDataFetcherWrapper::IOSImageDataFetcherWrapper(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : image_data_fetcher_(url_loader_factory) {}

IOSImageDataFetcherWrapper::~IOSImageDataFetcherWrapper() {}

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
    net::URLRequest::ReferrerPolicy referrer_policy,
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
    NSData* data =
        [NSData dataWithBytes:image_data.data() length:image_data.size()];

    if (!webp_transcode::WebpDecoder::IsWebpImage(image_data)) {
      callback(data, metadata);
      return;
    }

    // The image is a webp image.
    RequestMetadata webp_metadata = metadata;

    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {
            base::ThreadPool(),
            base::TaskPriority::BEST_EFFORT,
            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
        },
        base::BindOnce(^NSData*() {
          return webp_transcode::WebpDecoder::DecodeWebpImage(data);
        }),
        base::BindOnce(^(NSData* decoded_data) {
          callback(decoded_data, webp_metadata);
        }));
  });
}

}  // namespace image_fetcher
