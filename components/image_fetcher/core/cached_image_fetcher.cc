// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cached_image_fetcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher_metrics_reporter.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace image_fetcher {

struct CachedImageFetcherRequest {
  // The url to be fetched.
  const GURL url;

  ImageFetcherParams params;

  // Analytic events below.

  // True if there was a cache hit during the fetch sequence.
  bool cache_hit_before_network_request;

  // The start time of the fetch sequence.
  base::Time start_time;
};

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
                            const gfx::Image& image,
                            const image_fetcher::RequestMetadata& metadata) {
  if (image_callback.is_null()) {
    return;
  }
  std::move(image_callback).Run(image, metadata);
}

std::string EncodeSkBitmapToPNG(const std::string& uma_client_name,
                                const SkBitmap& bitmap) {
  std::vector<unsigned char> encoded_data;
  bool result = gfx::PNGCodec::Encode(
      static_cast<const unsigned char*>(bitmap.getPixels()),
      gfx::PNGCodec::FORMAT_SkBitmap,
      gfx::Size(bitmap.width(), bitmap.height()),
      static_cast<int>(bitmap.rowBytes()), /* discard_transparency */ false,
      std::vector<gfx::PNGCodec::Comment>(), &encoded_data);
  if (!result) {
    ImageFetcherMetricsReporter::ReportEvent(
        uma_client_name, ImageFetcherEvent::kTranscodingError);
    return "";
  } else {
    return std::string(encoded_data.begin(), encoded_data.end());
  }
}

}  // namespace

CachedImageFetcher::CachedImageFetcher(ImageFetcher* image_fetcher,
                                       scoped_refptr<ImageCache> image_cache,
                                       bool read_only)
    : image_fetcher_(image_fetcher),
      image_cache_(image_cache),
      read_only_(read_only) {
  DCHECK(image_fetcher_);
  DCHECK(image_cache_);
}

CachedImageFetcher::~CachedImageFetcher() = default;

ImageDecoder* CachedImageFetcher::GetImageDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return image_fetcher_->GetImageDecoder();
}

void CachedImageFetcher::FetchImageAndData(
    const GURL& image_url,
    ImageDataFetcherCallback image_data_callback,
    ImageFetcherCallback image_callback,
    ImageFetcherParams params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(wylieb): Inject a clock for better testability.
  CachedImageFetcherRequest request = {
      image_url, std::move(params),
      /* cache_hit_before_network_request */ false,
      /* start_time */ base::Time::Now()};

  ImageFetcherMetricsReporter::ReportEvent(request.params.uma_client_name(),
                                           ImageFetcherEvent::kImageRequest);

  if (request.params.skip_disk_cache_read()) {
    EnqueueFetchImageFromNetwork(std::move(request),
                                 std::move(image_data_callback),
                                 std::move(image_callback));
  } else {
    // First, try to load the image from the cache, then try the network.
    image_cache_->LoadImage(
        read_only_, image_url.spec(),
        base::BindOnce(&CachedImageFetcher::OnImageFetchedFromCache,
                       weak_ptr_factory_.GetWeakPtr(), std::move(request),
                       std::move(image_data_callback),
                       std::move(image_callback)));
  }
}

void CachedImageFetcher::OnImageFetchedFromCache(
    CachedImageFetcherRequest request,
    ImageDataFetcherCallback image_data_callback,
    ImageFetcherCallback image_callback,
    bool cache_result_needs_transcoding,
    std::string image_data) {
  if (image_data.empty()) {
    ImageFetcherMetricsReporter::ReportEvent(request.params.uma_client_name(),
                                             ImageFetcherEvent::kCacheMiss);

    // Fetching from the DB failed, start a network fetch.
    EnqueueFetchImageFromNetwork(std::move(request),
                                 std::move(image_data_callback),
                                 std::move(image_callback));
  } else {
    DataCallbackIfPresent(std::move(image_data_callback), image_data,
                          RequestMetadata());
    ImageFetcherMetricsReporter::ReportEvent(request.params.uma_client_name(),
                                             ImageFetcherEvent::kCacheHit);

    // Only continue with decoding if the user actually asked for an image, or
    // the image hadn't been transcoded yet and NOT loaded in the reduced mode.
    if (!image_callback.is_null() ||
        (cache_result_needs_transcoding &&
         !request.params.allow_needs_transcoding_file())) {
      auto* data_decoder = request.params.data_decoder();
      GetImageDecoder()->DecodeImage(
          image_data, gfx::Size(), data_decoder,
          base::BindOnce(&CachedImageFetcher::OnImageDecodedFromCache,
                         weak_ptr_factory_.GetWeakPtr(), std::move(request),
                         ImageDataFetcherCallback(), std::move(image_callback),
                         cache_result_needs_transcoding));
    }
  }
}

void CachedImageFetcher::OnImageDecodedFromCache(
    CachedImageFetcherRequest request,
    ImageDataFetcherCallback image_data_callback,
    ImageFetcherCallback image_callback,
    bool cache_result_needs_transcoding,
    const gfx::Image& image) {
  if (image.IsEmpty()) {
    ImageFetcherMetricsReporter::ReportEvent(
        request.params.uma_client_name(),
        ImageFetcherEvent::kCacheDecodingError);

    // Upon failure, fetch from the network.
    request.cache_hit_before_network_request = true;
    EnqueueFetchImageFromNetwork(std::move(request),
                                 std::move(image_data_callback),
                                 std::move(image_callback));
  } else {
    ImageCallbackIfPresent(std::move(image_callback), image, RequestMetadata());
    ImageFetcherMetricsReporter::ReportImageLoadFromCacheTime(
        request.params.uma_client_name(), request.start_time);

    // If cache_result_needs_transcoding is true, then this should be stored
    // again to replace the image data already on disk with the transcoded data.
    if (cache_result_needs_transcoding) {
      ImageFetcherMetricsReporter::ReportEvent(
          request.params.uma_client_name(),
          ImageFetcherEvent::kImageQueuedForTranscodingDecoded);
      EncodeAndStoreData(/* cache_result_needs_transcoding */ true,
                         /* is_image_data_transcoded */ true,
                         std::move(request), image);
    }
  }
}

void CachedImageFetcher::EnqueueFetchImageFromNetwork(
    CachedImageFetcherRequest request,
    ImageDataFetcherCallback image_data_callback,
    ImageFetcherCallback image_callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&CachedImageFetcher::FetchImageFromNetwork,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     std::move(image_data_callback),
                     std::move(image_callback)));
}

void CachedImageFetcher::FetchImageFromNetwork(
    CachedImageFetcherRequest request,
    ImageDataFetcherCallback image_data_callback,
    ImageFetcherCallback image_callback) {
  const GURL& url = request.url;

  ImageDataFetcherCallback wrapper_data_callback;
  ImageFetcherCallback wrapper_image_callback;

  bool skip_transcoding = request.params.skip_transcoding();
  ImageFetcherParams params_copy(request.params);
  if (skip_transcoding) {
    wrapper_data_callback =
        base::BindOnce(&CachedImageFetcher::OnImageFetchedWithoutTranscoding,
                       weak_ptr_factory_.GetWeakPtr(), std::move(request),
                       std::move(image_data_callback));
  } else {
    // Transcode the image when its downloaded from the network.
    // 1. Download the data.
    // 2. Decode the data to a gfx::Image in a utility process.
    // 3. Encode the data as a PNG in the browser process using
    //    base::ThreadPool.
    // 4. Cache the result.
    wrapper_data_callback = std::move(image_data_callback);
    wrapper_image_callback =
        base::BindOnce(&CachedImageFetcher::OnImageFetchedForTranscoding,
                       weak_ptr_factory_.GetWeakPtr(), std::move(request),
                       std::move(image_callback));
  }
  image_fetcher_->FetchImageAndData(url, std::move(wrapper_data_callback),
                                    std::move(wrapper_image_callback),
                                    std::move(params_copy));
}

void CachedImageFetcher::OnImageFetchedWithoutTranscoding(
    CachedImageFetcherRequest request,
    ImageDataFetcherCallback image_data_callback,
    const std::string& image_data,
    const RequestMetadata& request_metadata) {
  DataCallbackIfPresent(std::move(image_data_callback), image_data,
                        request_metadata);

  if (image_data.empty()) {
    ImageFetcherMetricsReporter::ReportEvent(request.params.uma_client_name(),
                                             ImageFetcherEvent::kTotalFailure);
  }

  StoreData(/* cache_result_needs_transcoding */ false,
            /* is_image_data_transcoded */ false, std::move(request),
            image_data);
}

void CachedImageFetcher::OnImageFetchedForTranscoding(
    CachedImageFetcherRequest request,
    ImageFetcherCallback image_callback,
    const gfx::Image& image,
    const RequestMetadata& request_metadata) {
  ImageCallbackIfPresent(std::move(image_callback), image, request_metadata);

  // Report to different histograms depending upon if there was a cache hit.
  if (request.cache_hit_before_network_request) {
    ImageFetcherMetricsReporter::ReportImageLoadFromNetworkAfterCacheHit(
        request.params.uma_client_name(), request.start_time);
  } else {
    ImageFetcherMetricsReporter::ReportImageLoadFromNetworkTime(
        request.params.uma_client_name(), request.start_time);
  }

  EncodeAndStoreData(/* cache_result_needs_transcoding */ false,
                     /* is_image_data_transcoded */ true, std::move(request),
                     image);
}

void CachedImageFetcher::EncodeAndStoreData(bool cache_result_needs_transcoding,
                                            bool is_image_data_transcoded,
                                            CachedImageFetcherRequest request,
                                            const gfx::Image& image) {
  // Copy the image data out and store it on disk.
  const SkBitmap* bitmap = image.IsEmpty() ? nullptr : image.ToSkBitmap();
  // If the bitmap is null or otherwise not ready, skip encoding.
  if (bitmap == nullptr || bitmap->isNull() || !bitmap->readyToDraw()) {
    ImageFetcherMetricsReporter::ReportEvent(request.params.uma_client_name(),
                                             ImageFetcherEvent::kTotalFailure);

    image_cache_->DeleteImage(request.url.spec());
  } else {
    std::string uma_client_name = request.params.uma_client_name();
    // Post a task to another thread to encode the image data downloaded.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&EncodeSkBitmapToPNG, uma_client_name, *bitmap),
        base::BindOnce(&CachedImageFetcher::StoreData,
                       weak_ptr_factory_.GetWeakPtr(),
                       cache_result_needs_transcoding, is_image_data_transcoded,
                       std::move(request)));
  }
}

void CachedImageFetcher::StoreData(bool cache_result_needs_transcoding,
                                   bool is_image_data_transcoded,
                                   CachedImageFetcherRequest request,
                                   std::string image_data) {
  std::string url = request.url.spec();
  // If the image is empty, delete the image.
  if (image_data.empty()) {
    image_cache_->DeleteImage(std::move(url));
    return;
  }

  if (!read_only_) {
    if (cache_result_needs_transcoding) {
      ImageFetcherMetricsReporter::ReportEvent(
          request.params.uma_client_name(),
          ImageFetcherEvent::kImageQueuedForTranscodingStoredBack);
      if (!is_image_data_transcoded)
        return;
    }

    // |needs_transcoding| is only true when the image to save isn't transcoded
    // and |allow_needs_transcoding_file()| is true (set by
    // ReducedModeImageFetcher).
    bool needs_transcoding = !is_image_data_transcoded &&
                             request.params.allow_needs_transcoding_file();
    image_cache_->SaveImage(std::move(url), std::move(image_data),
                            /* needs_transcoding */ needs_transcoding,
                            request.params.expiration_interval());
  }
}

}  // namespace image_fetcher
