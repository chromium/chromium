// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emoji/gif_tenor_api_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ui/webui/ash/emoji/emoji_picker.mojom.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace ash {
using emoji_picker::mojom::PageHandler;

constexpr char kTenorBaseUrl[] = "https://tenor.googleapis.com";
constexpr char kHttpMethod[] = "GET";
constexpr char kHttpContentType[] = "application/json";

constexpr char kContentFilterName[] = "contenfilter";
constexpr char kContentFilterValue[] = "high";

constexpr char kArRangeName[] = "ar_range";
constexpr char kArRangeValue[] = "wide";

constexpr char kMediaFilterName[] = "media_filter";
constexpr char kMediaFilterValue[] = "gif,mediumgif";

constexpr char kPosName[] = "pos";
const int64_t kTimeoutMs = 10000;

GifTenorApiFetcher::GifTenorApiFetcher() = default;

GifTenorApiFetcher::~GifTenorApiFetcher() = default;

void GifTenorApiFetcher::FetchCategories(
    PageHandler::GetCategoriesCallback callback,
    const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  constexpr char kCategoriesApi[] = "/v2/categories";
  constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
      net::DefineNetworkTrafficAnnotation(
          "chromeos_emoji_picker_categories_fetcher",
          R"(
      semantics {
        sender: "ChromeOS Emoji Picker"
        description:
          "Gets GIF categories from the tenor API "
          "(https://developers.google.com/tenor)."
        trigger:
          "When a user opens the emoji picker and select the GIF section."
        data:
          "None, (authentication to this API is done through Chrome's API key)."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "No setting. The feature does nothing by default. Users must take "
          "an explicit action to trigger it."
        policy_exception_justification:
          "Not implemented, not considered useful. This request is part of a "
          "flow which is user-initiated, and is not a background request."
      }
  )");

  endpoint_fetcher_ = std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory,
      /*url=*/GURL(kTenorBaseUrl).Resolve(kCategoriesApi),
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*timeout_ms=*/kTimeoutMs,
      /*post_data=*/"",
      /*headers=*/std::vector<std::string>(),
      /*annotation_tag=*/kTrafficAnnotation,
      /*is_stable_channel=*/chrome::GetChannel() ==
          version_info::Channel::STABLE);

  endpoint_fetcher_->PerformRequest(
      base::BindOnce(&GifTenorApiFetcher::FetchCategoriesResponseHandler,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      nullptr);
}

void GifTenorApiFetcher::FetchCategoriesResponseHandler(
    PageHandler::GetCategoriesCallback callback,
    std::unique_ptr<EndpointResponse> response) {
  std::move(callback).Run(response->response);
}

void GifTenorApiFetcher::FetchFeaturedGifs(
    PageHandler::GetFeaturedGifsCallback callback,
    const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const absl::optional<std::string>& pos) {
  constexpr char kFeaturedApi[] = "/v2/featured";
  constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
      net::DefineNetworkTrafficAnnotation(
          "chromeos_emoji_picker_featured_fetcher",
          R"(
      semantics {
        sender: "ChromeOS Emoji Picker"
        description:
          "Gets featured GIFs from the tenor API "
          "(https://developers.google.com/tenor)."
        trigger:
          "When a user opens the emoji picker and selects the GIF section, "
          "and the trending GIFs subcategory is active."
        data:
          "Position of the next batch of GIFs, used for infiniate scroll."
          "Authentication to this API is done through Chrome's API key."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "No setting. The feature does nothing by default. Users must take"
          "an explicit action to trigger it."
        policy_exception_justification:
          "Not implemented, not considered useful. This request is part of a "
          "flow which is user-initiated, and is not a background request."
      }
  )");

  GURL url =
      net::AppendQueryParameter(GURL(kTenorBaseUrl).Resolve(kFeaturedApi),
                                kContentFilterName, kContentFilterValue);
  url = net::AppendQueryParameter(url, kArRangeName, kArRangeValue);
  url = net::AppendQueryParameter(url, kMediaFilterName, kMediaFilterValue);
  if (pos) {
    url = net::AppendQueryParameter(url, kPosName, pos.value());
  }
  endpoint_fetcher_ = std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory,
      /*url=*/url,
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*timeout_ms=*/kTimeoutMs,
      /*post_data=*/"",
      /*headers=*/std::vector<std::string>(),
      /*annotation_tag=*/kTrafficAnnotation,
      /*is_stable_channel=*/chrome::GetChannel() ==
          version_info::Channel::STABLE);

  endpoint_fetcher_->PerformRequest(
      base::BindOnce(&GifTenorApiFetcher::FetchFeaturedGifsResponseHandler,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      nullptr);
}

void GifTenorApiFetcher::FetchFeaturedGifsResponseHandler(
    PageHandler::GetFeaturedGifsCallback callback,
    std::unique_ptr<EndpointResponse> response) {
  std::move(callback).Run(response->response);
}

}  // namespace ash
