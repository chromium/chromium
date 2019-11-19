// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/visuals_fetch_by_url.h"

#include <utility>

#include "base/bind.h"
#include "components/image_fetcher/core/image_fetcher.h"

namespace offline_pages {

namespace {

constexpr char kImageFetcherUmaClientName[] = "OfflinePages";

constexpr net::NetworkTrafficAnnotationTag kThumbnailTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("prefetch_visuals", R"(
        semantics {
          sender: "Offline Pages Prefetch"
          description:
            "Chromium fetches suggested articles for offline viewing. This"
            " network request is for a thumbnail or favicon that matches the"
            " article."
          trigger:
            "Two attempts, directly before and after the article is fetched."
          data:
            "The requested thumbnail or favicon URL."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable offline prefetch by toggling "
            "'Download articles for you' in settings under Downloads or "
            "by toggling chrome://flags#offline-prefetch."
          chrome_policy {
            NTPContentSuggestionsEnabled {
              policy_options {mode: MANDATORY}
              NTPContentSuggestionsEnabled: false
            }
          }
        })");

const int kPreferredFaviconWidthPixels = 16;
const int kPreferredFaviconHeightPixels = 16;

void FetchImageByURL(
    base::OnceCallback<void(const std::string& image_data)> callback,
    image_fetcher::ImageFetcher* fetcher,
    const GURL& image_url,
    const image_fetcher::ImageFetcherParams& params) {
  auto forward_callback =
      [](base::OnceCallback<void(const std::string& image_data)> callback,
         const std::string& image_data,
         const image_fetcher::RequestMetadata& request_metadata) {
        std::move(callback).Run(image_data);
      };

  fetcher->FetchImageData(image_url,
                          base::BindOnce(forward_callback, std::move(callback)),
                          std::move(params));
}

}  // namespace

void FetchThumbnailByURL(
    base::OnceCallback<void(const std::string& image_data)> callback,
    image_fetcher::ImageFetcher* fetcher,
    const GURL& thumbnail_url) {
  image_fetcher::ImageFetcherParams params(kThumbnailTrafficAnnotation,
                                           kImageFetcherUmaClientName);
  FetchImageByURL(std::move(callback), fetcher, thumbnail_url, params);
}

void FetchFaviconByURL(
    base::OnceCallback<void(const std::string& image_data)> callback,
    image_fetcher::ImageFetcher* fetcher,
    const GURL& favicon_url) {
  image_fetcher::ImageFetcherParams params(kThumbnailTrafficAnnotation,
                                           kImageFetcherUmaClientName);
  params.set_frame_size(
      gfx::Size(kPreferredFaviconWidthPixels, kPreferredFaviconHeightPixels));
  FetchImageByURL(std::move(callback), fetcher, favicon_url, params);
}

}  // namespace offline_pages
