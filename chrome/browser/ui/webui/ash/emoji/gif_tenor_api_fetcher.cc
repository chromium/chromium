// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emoji/gif_tenor_api_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_picker.mojom.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {
using emoji_picker::mojom::PageHandler;
constexpr char kTenorBaseUrl[] = "https://tenor.googleapis.com";
constexpr char kHttpMethod[] = "GET";
constexpr char kHttpContentType[] = "application/json";
constexpr char kCategoriesApi[] = "/v2/categories";
const int64_t kTimeoutMs = 10000;

GifTenorApiFetcher::GifTenorApiFetcher() = default;

GifTenorApiFetcher::~GifTenorApiFetcher() = default;

void GifTenorApiFetcher::FetchCategories(
    PageHandler::GetCategoriesCallback callback,
    Profile* profile) {
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
      /*url_loader_factory=*/profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess(),
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

}  // namespace ash
