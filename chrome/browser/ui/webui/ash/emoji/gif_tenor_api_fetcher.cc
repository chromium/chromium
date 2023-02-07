// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emoji/gif_tenor_api_fetcher.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_picker.mojom.h"
#include "chrome/common/channel_info.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/version_info/channel.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace ash {

namespace {

using emoji_picker::mojom::PageHandler;

constexpr char kTenorBaseUrl[] = "https://tenor.googleapis.com";
constexpr char kHttpMethod[] = "GET";
constexpr char kHttpContentType[] = "application/json";

constexpr char kContentFilterName[] = "contentfilter";
constexpr char kContentFilterValue[] = "high";

constexpr char kArRangeName[] = "ar_range";
constexpr char kArRangeValue[] = "wide";

constexpr char kMediaFilterName[] = "media_filter";
constexpr char kMediaFilterValue[] = "gif,tinygif";

constexpr char kPosName[] = "pos";
const int64_t kTimeoutMs = 10000;

std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
    const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  return std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory,
      /*url=*/url,
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*timeout_ms=*/kTimeoutMs,
      /*post_data=*/"",
      /*headers=*/std::vector<std::string>(),
      /*annotation_tag=*/annotation_tag,
      /*is_stable_channel=*/chrome::GetChannel() ==
          version_info::Channel::STABLE);
}

const base::Value::List* FindList(
    data_decoder::DataDecoder::ValueOrError& result,
    const std::string& key) {
  if (!result.has_value()) {
    return nullptr;
  }

  const auto* response = result->GetIfDict();
  if (!response) {
    return nullptr;
  }

  const auto* list = response->FindList(key);
  return list ? list : nullptr;
}

std::vector<emoji_picker::mojom::GifResponsePtr> ParseGifs(
    const base::Value::List* results) {
  std::vector<emoji_picker::mojom::GifResponsePtr> gifs;
  for (const auto& result : *results) {
    const auto* gif = result.GetIfDict();
    if (!gif) {
      continue;
    }

    const auto* id = gif->FindString("id");
    if (!id) {
      continue;
    }

    const auto* content_description = gif->FindString("content_description");
    if (!content_description) {
      continue;
    }

    const auto* media_formats = gif->FindDict("media_formats");
    if (!media_formats) {
      continue;
    }

    const auto* full_gif = media_formats->FindDict("gif");
    if (!full_gif) {
      continue;
    }

    const auto* full_url = full_gif->FindString("url");
    if (!full_url) {
      continue;
    }

    const auto full_gurl = GURL(full_url->c_str());
    if (!full_gurl.is_valid()) {
      continue;
    }

    const auto* preview_gif = media_formats->FindDict("tinygif");
    if (!preview_gif) {
      continue;
    }

    const auto* preview_size = preview_gif->FindList("dims");
    if (!preview_size) {
      continue;
    }

    if (preview_size->size() != 2) {
      // [width, height]
      continue;
    }

    const auto width = preview_size->front().GetIfInt();
    if (!width.has_value()) {
      continue;
    }

    const auto height = preview_size->back().GetIfInt();
    if (!height.has_value()) {
      continue;
    }

    const auto* preview_url = preview_gif->FindString("url");
    if (!preview_url) {
      continue;
    }

    const auto preview_gurl = GURL(preview_url->c_str());
    if (!preview_gurl.is_valid()) {
      continue;
    }

    gifs.push_back(emoji_picker::mojom::GifResponse::New(
        *id, *content_description,
        emoji_picker::mojom::GifUrls::New(full_gurl, preview_gurl),
        gfx::Size(width.value(), height.value())));
  }
  return gifs;
}

GURL GetUrl(const char* endpoint, const absl::optional<std::string>& pos) {
  GURL url = net::AppendQueryParameter(GURL(kTenorBaseUrl).Resolve(endpoint),
                                       kContentFilterName, kContentFilterValue);
  url = net::AppendQueryParameter(url, kArRangeName, kArRangeValue);
  url = net::AppendQueryParameter(url, kMediaFilterName, kMediaFilterValue);
  if (pos) {
    url = net::AppendQueryParameter(url, kPosName, pos.value());
  }
  return url;
}

emoji_picker::mojom::Status GetError(
    std::unique_ptr<EndpointResponse> response) {
  return response->error_type.has_value() &&
                 response->error_type == FetchErrorType::kNetError
             ? emoji_picker::mojom::Status::kNetError
             : emoji_picker::mojom::Status::kHttpError;
}

}  // namespace

GifTenorApiFetcher::GifTenorApiFetcher()
    : endpoint_fetcher_creator_{base::BindRepeating(&CreateEndpointFetcher)} {}

GifTenorApiFetcher::GifTenorApiFetcher(
    EndpointFetcherCreator endpoint_fetcher_creator)
    : endpoint_fetcher_creator_{endpoint_fetcher_creator} {}

GifTenorApiFetcher::~GifTenorApiFetcher() = default;

void GifTenorApiFetcher::TenorGifsApiResponseHandler(
    TenorGifsApiCallback callback,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> response) {
  if (response->http_status_code == net::HTTP_OK) {
    data_decoder::DataDecoder::ParseJsonIsolated(
        response->response,
        base::BindOnce(&GifTenorApiFetcher::OnGifsJsonParsed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  std::move(callback).Run(
      GetError(std::move(response)),
      emoji_picker::mojom::TenorGifResponse::New(
          "", std::vector<emoji_picker::mojom::GifResponsePtr>{}));
}

void GifTenorApiFetcher::OnGifsJsonParsed(
    TenorGifsApiCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  const auto* gifs = FindList(result, "results");
  if (!gifs) {
    std::move(callback).Run(
        emoji_picker::mojom::Status::kHttpError,
        emoji_picker::mojom::TenorGifResponse::New(
            "", std::vector<emoji_picker::mojom::GifResponsePtr>{}));
    return;
  }
  const auto* next = result->FindStringKey("next");
  std::move(callback).Run(emoji_picker::mojom::Status::kHttpOk,
                          emoji_picker::mojom::TenorGifResponse::New(
                              next ? *next : "", ParseGifs(gifs)));
}

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

  auto endpoint_fetcher = endpoint_fetcher_creator_.Run(
      url_loader_factory, GURL(kTenorBaseUrl).Resolve(kCategoriesApi),
      kTrafficAnnotation);
  auto* const endpoint_fetcher_ptr = endpoint_fetcher.get();
  endpoint_fetcher_ptr->PerformRequest(
      base::BindOnce(&GifTenorApiFetcher::FetchCategoriesResponseHandler,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(endpoint_fetcher)),
      nullptr);
}

void GifTenorApiFetcher::FetchCategoriesResponseHandler(
    PageHandler::GetCategoriesCallback callback,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> response) {
  if (response->http_status_code == net::HTTP_OK) {
    data_decoder::DataDecoder::ParseJsonIsolated(
        response->response,
        base::BindOnce(&GifTenorApiFetcher::OnCategoriesJsonParsed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  std::move(callback).Run(GetError(std::move(response)),
                          std::vector<std::string>{});
}

void GifTenorApiFetcher::OnCategoriesJsonParsed(
    PageHandler::GetCategoriesCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  const auto* tags = FindList(result, "tags");
  if (!tags) {
    std::move(callback).Run(emoji_picker::mojom::Status::kHttpError,
                            std::vector<std::string>{});
    return;
  }

  std::vector<std::string> categories;
  for (const auto& tag : *tags) {
    const auto* category = tag.GetIfDict();
    if (!category) {
      continue;
    }

    const auto* name = category->FindString("name");
    if (!name) {
      continue;
    }

    categories.push_back(*name);
  }

  std::move(callback).Run(emoji_picker::mojom::Status::kHttpOk,
                          std::move(categories));
}

void GifTenorApiFetcher::FetchFeaturedGifs(
    TenorGifsApiCallback callback,
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

  auto endpoint_fetcher = endpoint_fetcher_creator_.Run(
      url_loader_factory, GetUrl(kFeaturedApi, pos), kTrafficAnnotation);
  auto* const endpoint_fetcher_ptr = endpoint_fetcher.get();
  endpoint_fetcher_ptr->PerformRequest(
      base::BindOnce(&GifTenorApiFetcher::TenorGifsApiResponseHandler,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(endpoint_fetcher)),
      nullptr);
}

void GifTenorApiFetcher::FetchGifSearch(
    TenorGifsApiCallback callback,
    const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& query,
    const absl::optional<std::string>& pos) {
  constexpr char kSearchApi[] = "/v2/search";
  constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
      net::DefineNetworkTrafficAnnotation(
          "chromeos_emoji_picker_search_fetcher",
          R"(
      semantics {
        sender: "ChromeOS Emoji Picker"
        description:
          "Gets a list of the most relevant GIFs from the tenor API "
          "(https://developers.google.com/tenor) for a given search term."
        trigger:
          "When a user opens the emoji picker and selects the GIF section, "
          "then type in a search query in the search bar."
        data:
          "Text a user has typed into a text field."
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

  GURL url = GetUrl(kSearchApi, pos);
  url = net::AppendQueryParameter(url, "q", query);

  auto endpoint_fetcher = endpoint_fetcher_creator_.Run(url_loader_factory, url,
                                                        kTrafficAnnotation);
  auto* const endpoint_fetcher_ptr = endpoint_fetcher.get();
  endpoint_fetcher_ptr->PerformRequest(
      base::BindOnce(&GifTenorApiFetcher::TenorGifsApiResponseHandler,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(endpoint_fetcher)),
      nullptr);
}

void GifTenorApiFetcher::FetchGifsByIds(
    PageHandler::GetGifsByIdsCallback callback,
    const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::vector<std::string>& ids) {
  constexpr char kPostsApi[] = "/v2/posts";
  constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
      net::DefineNetworkTrafficAnnotation("chromeos_emoji_picker_posts_fetcher",
                                          R"(
      semantics {
        sender: "ChromeOS Emoji Picker"
        description:
          "Gets a list of GIFs from the tenor API "
          "(https://developers.google.com/tenor) for the specified IDs."
        trigger:
          "When a user opens the emoji picker and selects the GIF section, "
          "and the recent GIFs subcategory is active."
        data:
          "The IDs of the GIFS saved in recent."
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

  auto endpoint_fetcher = endpoint_fetcher_creator_.Run(
      url_loader_factory,
      net::AppendQueryParameter(GURL(kTenorBaseUrl).Resolve(kPostsApi), "ids",
                                base::JoinString(ids, ",")),
      kTrafficAnnotation);
  auto* const endpoint_fetcher_ptr = endpoint_fetcher.get();
  endpoint_fetcher_ptr->PerformRequest(
      base::BindOnce(&GifTenorApiFetcher::FetchGifsByIdsResponseHandler,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(endpoint_fetcher)),
      nullptr);
}

void GifTenorApiFetcher::FetchGifsByIdsResponseHandler(
    emoji_picker::mojom::PageHandler::GetGifsByIdsCallback callback,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> response) {
  if (response->http_status_code == net::HTTP_OK) {
    data_decoder::DataDecoder::ParseJsonIsolated(
        response->response,
        base::BindOnce(&GifTenorApiFetcher::OnGifsByIdsJsonParsed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  std::move(callback).Run(GetError(std::move(response)),
                          std::vector<emoji_picker::mojom::GifResponsePtr>{});
}

void GifTenorApiFetcher::OnGifsByIdsJsonParsed(
    emoji_picker::mojom::PageHandler::GetGifsByIdsCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  const auto* gifs = FindList(result, "results");
  if (!gifs) {
    std::move(callback).Run(emoji_picker::mojom::Status::kHttpError,
                            std::vector<emoji_picker::mojom::GifResponsePtr>{});
    return;
  }

  std::move(callback).Run(emoji_picker::mojom::Status::kHttpOk,
                          ParseGifs(gifs));
}
}  // namespace ash
