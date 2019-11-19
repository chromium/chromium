// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_IOS_IOS_IMAGE_DATA_FETCHER_WRAPPER_H_
#define COMPONENTS_IMAGE_FETCHER_IOS_IOS_IMAGE_DATA_FETCHER_WRAPPER_H_

#import <Foundation/Foundation.h>

#include "base/memory/ref_counted.h"
#include "components/image_fetcher/core/image_data_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_types.h"

namespace network {
class SharedURLLoaderFactory;
}

class GURL;

namespace image_fetcher {

class IOSImageDataFetcherWrapper {
 public:
  // The TaskRunner is used to decode the image if it is WebP-encoded.
  explicit IOSImageDataFetcherWrapper(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  virtual ~IOSImageDataFetcherWrapper();

  // Helper to start downloading and possibly decoding the image without a
  // referrer.
  void FetchImageDataWebpDecoded(const GURL& image_url,
                                 ImageDataFetcherBlock callback,
                                 bool send_cookies = false);

  // Start downloading the image at the given |image_url|. The |callback| will
  // be called with the downloaded image, or nil if any error happened. If the
  // image is WebP it will be decoded.
  // The |referrer| and |referrer_policy| will be passed on to the underlying
  // URLFetcher.
  // |callback| cannot be nil.
  void FetchImageDataWebpDecoded(
      const GURL& image_url,
      ImageDataFetcherBlock callback,
      const std::string& referrer,
      net::URLRequest::ReferrerPolicy referrer_policy,
      bool send_cookies = false);

  // Test-only accessor for underlying ImageDataFetcher.
  ImageDataFetcher* AccessImageDataFetcherForTesting() {
    return &image_data_fetcher_;
  }

 private:
  ImageDataFetcherCallback CallbackForImageDataFetcher(
      ImageDataFetcherBlock callback);

  ImageDataFetcher image_data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(IOSImageDataFetcherWrapper);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_IOS_IOS_IMAGE_DATA_FETCHER_WRAPPER_H_
