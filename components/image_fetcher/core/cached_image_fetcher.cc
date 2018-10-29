// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cached_image_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "components/image_fetcher/core/cache/cached_image_fetcher_metrics_reporter.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace image_fetcher {

namespace {

void DataCallbackIfPresent(ImageDataFetcherCallback data_callback,
                           const std::string& image_data,
                           const image_fetcher::RequestMetadata& metadata) {
  if (data_callback.is_null()) {
    return;
  }
  std::move(data_callback).Run(image_data, metadata);
}

void ImageCallbackIfPresent(ImageFetcherCallback image_callback,
                            const std::string& id,
                            const gfx::Image& image,
                            const image_fetcher::RequestMetadata& metadata) {
  if (image_callback.is_null()) {
    return;
  }
  std::move(image_callback).Run(id, image, metadata);
}

bool EncodeSkBitmapToPNG(const SkBitmap& bitmap,
                         std::vector<unsigned char>* dest) {
  if (!bitmap.readyToDraw() || bitmap.isNull()) {
    return false;
  }

  return gfx::PNGCodec::Encode(
      static_cast<const unsigned char*>(bitmap.getPixels()),
      gfx::PNGCodec::FORMAT_RGBA, gfx::Size(bitmap.width(), bitmap.height()),
      static_cast<int>(bitmap.rowBytes()), /* discard_transparency */ false,
      std::vector<gfx::PNGCodec::Comment>(), dest);
}

}  // namespace

CachedImageFetcher::CachedImageFetcher(
    std::unique_ptr<ImageFetcher> image_fetcher,
    scoped_refptr<ImageCache> image_cache,
    bool read_only)
    : image_fetcher_(std::move(image_fetcher)),
      image_cache_(image_cache),
      read_only_(read_only),
      weak_ptr_factory_(this) {
  DCHECK(image_fetcher_);
  DCHECK(image_cache_);
}

CachedImageFetcher::~CachedImageFetcher() = default;

void CachedImageFetcher::SetDataUseServiceName(
    DataUseServiceName data_use_service_name) {
  image_fetcher_->SetDataUseServiceName(data_use_service_name);
}

void CachedImageFetcher::SetDesiredImageFrameSize(const gfx::Size& size) {
  image_fetcher_->SetDesiredImageFrameSize(size);
  desired_image_frame_size_ = size;
}

void CachedImageFetcher::SetImageDownloadLimit(
    base::Optional<int64_t> max_download_bytes) {
  image_fetcher_->SetImageDownloadLimit(max_download_bytes);
}

ImageDecoder* CachedImageFetcher::GetImageDecoder() {
  return image_fetcher_->GetImageDecoder();
}

void CachedImageFetcher::FetchImageAndData(
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback data_callback,
    ImageFetcherCallback image_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  // First, try to load the image from the cache, then try the network.
  image_cache_->LoadImage(
      read_only_, image_url.spec(),
      base::BindOnce(&CachedImageFetcher::OnImageFetchedFromCache,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now(), id,
                     image_url, std::move(data_callback),
                     std::move(image_callback), traffic_annotation));

  CachedImageFetcherMetricsReporter::ReportEvent(
      CachedImageFetcherEvent::kImageRequest);
}

void CachedImageFetcher::OnImageFetchedFromCache(
    base::Time start_time,
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback data_callback,
    ImageFetcherCallback image_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    std::string image_data) {
  if (image_data.empty()) {
    // Fetching from the DB failed, start a network fetch.
    FetchImageFromNetwork(/* cache_hit */ false, start_time, id, image_url,
                          std::move(data_callback), std::move(image_callback),
                          traffic_annotation);

    CachedImageFetcherMetricsReporter::ReportEvent(
        CachedImageFetcherEvent::kCacheMiss);
  } else {
    DataCallbackIfPresent(std::move(data_callback), image_data,
                          RequestMetadata());
    // TODO(wylieb): On Android, do this in-process.
    GetImageDecoder()->DecodeImage(
        image_data, desired_image_frame_size_,
        base::BindRepeating(&CachedImageFetcher::OnImageDecodedFromCache,
                            weak_ptr_factory_.GetWeakPtr(), start_time, id,
                            image_url, base::Passed(std::move(data_callback)),
                            base::Passed(std::move(image_callback)),
                            traffic_annotation, image_data));
    CachedImageFetcherMetricsReporter::ReportEvent(
        CachedImageFetcherEvent::kCacheHit);
  }
}

void CachedImageFetcher::OnImageDecodedFromCache(
    base::Time start_time,
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback data_callback,
    ImageFetcherCallback image_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& image_data,
    const gfx::Image& image) {
  if (image.IsEmpty()) {
    // Upon failure, fetch from the network.
    FetchImageFromNetwork(/* cache_hit */ true, start_time, id, image_url,
                          std::move(data_callback), std::move(image_callback),
                          traffic_annotation);

    CachedImageFetcherMetricsReporter::ReportEvent(
        CachedImageFetcherEvent::kCacheDecodingError);
  } else {
    ImageCallbackIfPresent(std::move(image_callback), id, image,
                           RequestMetadata());
    CachedImageFetcherMetricsReporter::ReportImageLoadFromCacheTime(start_time);
  }
}

void CachedImageFetcher::FetchImageFromNetwork(
    bool cache_hit,
    base::Time start_time,
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback data_callback,
    ImageFetcherCallback image_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  // Fetch image data and the image itself. The image data will be stored in
  // the image cache, and the image will be returned to the caller.
  image_fetcher_->FetchImageAndData(
      id, image_url,
      base::BindOnce(&CachedImageFetcher::DecodeDataForCaching,
                     weak_ptr_factory_.GetWeakPtr(), std::move(data_callback),
                     image_url),
      base::BindOnce(&CachedImageFetcher::OnImageFetchedFromNetwork,
                     weak_ptr_factory_.GetWeakPtr(), cache_hit, start_time,
                     std::move(image_callback), image_url),
      traffic_annotation);
}

void CachedImageFetcher::OnImageFetchedFromNetwork(
    bool cache_hit,
    base::Time start_time,
    ImageFetcherCallback image_callback,
    const GURL& image_url,
    const std::string& id,
    const gfx::Image& image,
    const RequestMetadata& request_metadata) {
  // The image has been deocded by the fetcher already, return straight to the
  // caller.
  ImageCallbackIfPresent(std::move(image_callback), id, image,
                         request_metadata);

  // Report failure if the image is empty.
  if (image.IsEmpty()) {
    CachedImageFetcherMetricsReporter::ReportEvent(
        CachedImageFetcherEvent::kTotalFailure);
  }

  // Report to different histograms depending upon if there was a cache hit.
  if (cache_hit) {
    CachedImageFetcherMetricsReporter::ReportImageLoadFromNetworkAfterCacheHit(
        start_time);
  } else {
    CachedImageFetcherMetricsReporter::ReportImageLoadFromNetworkTime(
        start_time);
  }
}

void CachedImageFetcher::DecodeDataForCaching(
    ImageDataFetcherCallback data_callback,
    const GURL& image_url,
    const std::string& image_data,
    const RequestMetadata& request_metadata) {
  DataCallbackIfPresent(std::move(data_callback), image_data, request_metadata);
  GetImageDecoder()->DecodeImage(
      image_data, /* Decoding for cache shouldn't specify size */ gfx::Size(),
      base::BindRepeating(&CachedImageFetcher::EncodeDataAndCache,
                          weak_ptr_factory_.GetWeakPtr(), image_url));
}

void CachedImageFetcher::EncodeDataAndCache(const GURL& image_url,
                                            const gfx::Image& image) {
  std::vector<unsigned char> encoded_data;
  // If the image is empty, or there's a problem encoding the image, don't save
  // it.
  if (image.IsEmpty() ||
      !EncodeSkBitmapToPNG(*image.ToSkBitmap(), &encoded_data)) {
    CachedImageFetcherMetricsReporter::ReportEvent(
        CachedImageFetcherEvent::kTranscodingError);
    image_cache_->DeleteImage(image_url.spec());
    return;
  }

  if (!read_only_) {
    image_cache_->SaveImage(image_url.spec(), std::string(encoded_data.begin(),
                                                          encoded_data.end()));
  }
}

}  // namespace image_fetcher
