// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/cached_image_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/query_tiles/internal/stats.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

using image_fetcher::ImageDataFetcherCallback;
using image_fetcher::ImageFetcher;
using image_fetcher::ImageFetcherParams;
using image_fetcher::RequestMetadata;
using query_tiles::stats::ImagePreloadingEvent;

namespace query_tiles {
namespace {

// A string used to log UMA for query tiles in image fetcher service.
constexpr char kImageFetcherUmaClientName[] = "QueryTiles";

// The time interval for the images to stay in image fetcher's cache after last
// used time.
constexpr base::TimeDelta kImageCacheExpirationInterval = base::Days(1);

constexpr net::NetworkTrafficAnnotationTag kQueryTilesTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("query_tiles_image_loader", R"(
      semantics {
        sender: "Query Tiles Image Loader"
        description:
          "Fetches image for query tiles on Android NTP. Images are hosted on"
          " Google static content server, the data source may come from third"
          " parties."
        trigger:
          "When the user opens NTP to view the query tiles on Android, and"
          " the image cache doesn't have a fresh copy on disk."
        data: "URL of the image to be fetched."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting: "Disabled when the user uses search engines other than Google."
        chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
        }
      })");

ImageFetcherParams CreateImageFetcherParams() {
  ImageFetcherParams params(kQueryTilesTrafficAnnotation,
                            kImageFetcherUmaClientName);
  params.set_hold_for_expiration_interval(kImageCacheExpirationInterval);
  return params;
}

void OnImageFetched(ImageLoader::BitmapCallback callback,
                    const gfx::Image& image,
                    const image_fetcher::RequestMetadata& request_metadata) {
  DCHECK(callback);
  std::move(callback).Run(image.AsBitmap());

  bool success = request_metadata.http_response_code == net::OK;
  stats::RecordImageLoading(success ? ImagePreloadingEvent::kSuccess
                                    : ImagePreloadingEvent::kFailure);
}

void OnImageDataPrefetched(ImageLoader::SuccessCallback callback,
                           const std::string&,
                           const RequestMetadata& request_metadata) {
  DCHECK(callback);
  bool success = request_metadata.http_response_code == net::OK;
  std::move(callback).Run(success);

  stats::RecordImageLoading(success
                                ? ImagePreloadingEvent::kSuccessReducedMode
                                : ImagePreloadingEvent::kFailureReducedMode);
}

}  // namespace

CachedImageLoader::CachedImageLoader(ImageFetcher* cached_image_fetcher,
                                     ImageFetcher* reduced_mode_image_fetcher)
    : cached_image_fetcher_(cached_image_fetcher),
      reduced_mode_image_fetcher_(reduced_mode_image_fetcher) {
  DCHECK(cached_image_fetcher_);
  DCHECK(reduced_mode_image_fetcher_);
}

CachedImageLoader::~CachedImageLoader() = default;

void CachedImageLoader::FetchImage(const GURL& url, BitmapCallback callback) {
  // Fetch and decode the image from network or disk cache.
  cached_image_fetcher_->FetchImage(
      url, base::BindOnce(&OnImageFetched, std::move(callback)),
      CreateImageFetcherParams());
  stats::RecordImageLoading(ImagePreloadingEvent::kStart);
}

void CachedImageLoader::PrefetchImage(const GURL& url,
                                      SuccessCallback callback) {
  // In reduced mode, the image will not be decoded immediately,
  reduced_mode_image_fetcher_->FetchImageData(
      url, base::BindOnce(&OnImageDataPrefetched, std::move(callback)),
      CreateImageFetcherParams());
  stats::RecordImageLoading(ImagePreloadingEvent::kStartReducedMode);
}

}  // namespace query_tiles
