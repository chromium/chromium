// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_IMPL_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_IMPL_H_

#include <compare>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/image_fetcher/core/image_data_fetcher.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace gfx {
class Image;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace image_fetcher {

// The standard (non-test) implementation of ImageFetcher.
class ImageFetcherImpl : public ImageFetcher {
 public:
  ImageFetcherImpl(
      std::unique_ptr<ImageDecoder> image_decoder,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ImageFetcherImpl(const ImageFetcherImpl&) = delete;
  ImageFetcherImpl& operator=(const ImageFetcherImpl&) = delete;

  ~ImageFetcherImpl() override;

  void FetchImageAndData(const GURL& image_url,
                         ImageDataFetcherCallback image_data_callback,
                         ImageFetcherCallback image_callback,
                         ImageFetcherParams params) override;

  ImageDecoder* GetImageDecoder() override;

 private:
  // Only requests with the same URL, download limit, frame size, and selected
  // decoder can share a download and decoded result. The selected DataDecoder
  // must outlive ImageDecoder, per DecodeImage's contract. Store size
  // components separately because gfx::Size does not provide an ordering.
  struct ImageRequestKey {
    auto operator<=>(const ImageRequestKey&) const = default;

    GURL image_url;
    std::optional<int64_t> max_download_size;
    int frame_width;
    int frame_height;
    raw_ptr<data_decoder::DataDecoder> data_decoder;
  };

  // State related to an image fetch (id, pending callbacks).
  struct ImageRequest {
    ImageRequest();

    ImageRequest(const ImageRequest&) = delete;
    ImageRequest& operator=(const ImageRequest&) = delete;

    ImageRequest(ImageRequest&& other);

    ~ImageRequest();

    std::string id;
    // These have the default value if the image data has not yet been fetched.
    RequestMetadata request_metadata;
    std::string image_data;
    bool image_data_fetched = false;
    // Queue for pending callbacks, which may accumulate while the request is in
    // flight.
    std::vector<ImageFetcherCallback> image_callbacks;
    std::vector<ImageDataFetcherCallback> image_data_callbacks;
  };

  using ImageRequestMap = std::map<ImageRequestKey, ImageRequest>;

  // Processes image URL fetched events. This is the continuation method used
  // for creating callbacks that are passed to the ImageDataFetcher.
  void OnImageURLFetched(const ImageRequestKey& request_key,
                         ImageFetcherParams params,
                         const std::string& image_data,
                         const RequestMetadata& metadata);

  // Processes image decoded events. This is the continuation method used for
  // creating callbacks that are passed to the ImageDecoder.
  void OnImageDecoded(const ImageRequestKey& request_key,
                      const RequestMetadata& metadata,
                      const gfx::Image& image);

  // Used to run `image_data_callback` only if `this` is still valid.
  void RunImageDataCallback(ImageDataFetcherCallback image_data_callback,
                            std::string image_data,
                            RequestMetadata request_metadata);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<ImageDecoder> image_decoder_;

  std::unique_ptr<ImageDataFetcher> image_data_fetcher_;

  // Pending requests grouped by compatible download and decode options.
  ImageRequestMap pending_net_requests_;

  base::WeakPtrFactory<ImageFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_IMPL_H_
