// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/emoji/gif_tenor_api_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version_info/channel.h"
#include "chromeos/ash/components/channel/channel_info.h"
#include "chromeos/ash/components/emoji/tenor_types.mojom.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kTenorBaseUrl[] = "https://tenor.googleapis.com";
constexpr char kHttpMethod[] = "GET";
constexpr char kHttpContentType[] = "application/json";

constexpr char kContentFilterName[] = "contentfilter";
constexpr char kContentFilterValue[] = "high";

constexpr char kArRangeName[] = "ar_range";
constexpr char kArRangeValue[] = "wide";

constexpr char kMediaFilterName[] = "media_filter";
constexpr char kMediaFilterValue[] = "gif,tinygif,tinygifpreview";

constexpr char kClientKeyName[] = "client_key";
constexpr char kClientKeyValue[] = "chromeos";

constexpr char kPosName[] = "pos";
constexpr base::TimeDelta kTimeout = base::Milliseconds(10000);

constexpr char kSearchApi[] = "/v2/search";
constexpr net::NetworkTrafficAnnotationTag kSearchTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("chromeos_emoji_picker_search_fetcher",
                                        R"(
      semantics {
        sender: "ChromeOS Emoji Picker"
        description:
          "Gets a list of the most relevant GIFs from the tenor API "
          "(https://developers.google.com/tenor) for a given search term."
        trigger:
          "When a user opens the emoji picker and selects the GIF section, "
          "then type in a search query in the search bar."
        internal {
          contacts {
              email: "e14s-eng@google.com"
          }
        }
        user_data {
          type: USER_CONTENT
        }
        data:
          "Text a user has typed into a text field."
          "Position of the next batch of GIFs, used for infinite scroll."
          "Authentication to this API is done through Chrome's API key."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2024-09-30"
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

std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
    const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  return std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory,
      /*url=*/url,
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*timeout=*/kTimeout,
      /*post_data=*/"",
      /*headers=*/std::vector<std::string>(),
      /*cors_exempt_headers=*/std::vector<std::string>(),
      /*annotation_tag=*/annotation_tag, ash::GetChannel());
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

std::vector<tenor::mojom::GifResponsePtr> ParseGifs(
    const base::Value::List* results) {
  std::vector<tenor::mojom::GifResponsePtr> gifs;
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

    const base::Value::List* full_size = full_gif->FindList("dims");
    if (!full_size) {
      continue;
    }

    if (full_size->size() != 2) {
      // [width, height]
      continue;
    }

    const std::optional<int> full_width = full_size->front().GetIfInt();
    if (!full_width.has_value()) {
      continue;
    }

    const std::optional<int> full_height = full_size->back().GetIfInt();
    if (!full_height.has_value()) {
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

    const auto preview_width = preview_size->front().GetIfInt();
    if (!preview_width.has_value()) {
      continue;
    }

    const auto preview_height = preview_size->back().GetIfInt();
    if (!preview_height.has_value()) {
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

    const base::Value::Dict* tiny_gif_preview =
        media_formats->FindDict("tinygifpreview");
    if (!tiny_gif_preview) {
      continue;
    }

    const std::string* tiny_gif_preview_url =
        tiny_gif_preview->FindString("url");
    if (!tiny_gif_preview_url) {
      continue;
    }

    const GURL tiny_gif_preview_gurl = GURL(*tiny_gif_preview_url);
    if (!tiny_gif_preview_gurl.is_valid()) {
      continue;
    }

    gifs.push_back(tenor::mojom::GifResponse::New(
        *id, *content_description,
        tenor::mojom::GifUrls::New(full_gurl, preview_gurl,
                                   tiny_gif_preview_gurl),
        gfx::Size(preview_width.value(), preview_height.value()),
        gfx::Size(*full_width, *full_height)));
  }
  return gifs;
}

GURL GetUrl(const char* endpoint, const std::optional<std::string>& pos) {
  GURL url = net::AppendQueryParameter(GURL(kTenorBaseUrl).Resolve(endpoint),
                                       kContentFilterName, kContentFilterValue);
  url = net::AppendQueryParameter(url, kArRangeName, kArRangeValue);
  url = net::AppendQueryParameter(url, kMediaFilterName, kMediaFilterValue);
  url = net::AppendQueryParameter(url, kClientKeyName, kClientKeyValue);
  if (pos) {
    url = net::AppendQueryParameter(url, kPosName, pos.value());
  }
  return url;
}

tenor::mojom::Status GetError(std::unique_ptr<EndpointResponse> response) {
  return response->error_type.has_value() &&
                 response->error_type == FetchErrorType::kNetError
             ? tenor::mojom::Status::kNetError
             : tenor::mojom::Status::kHttpError;
}

}  // namespace

GifTenorApiFetcher::GifTenorApiFetcher()
    : endpoint_fetcher_creator_{base::BindRepeating(&CreateEndpointFetcher)} {}

GifTenorApiFetcher::GifTenorApiFetcher(
    EndpointFetcherCreator endpoint_fetcher_creator)
    : endpoint_fetcher_creator_{endpoint_fetcher_creator} {}

GifTenorApiFetcher::~GifTenorApiFetcher() = default;

// `endpoint_fetcher` may be null.
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
  std::move(callback).Run(GetError(std::move(response)),
                          tenor::mojom::PaginatedGifResponses::New(
                              "", std::vector<tenor::mojom::GifResponsePtr>{}));
}

void GifTenorApiFetcher::OnGifsJsonParsed(
    TenorGifsApiCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  const auto* gifs = FindList(result, "results");
  if (!gifs) {
    std::move(callback).Run(
        tenor::mojom::Status::kHttpError,
        tenor::mojom::PaginatedGifResponses::New(
            "", std::vector<tenor::mojom::GifResponsePtr>{}));
    return;
  }
  const auto* next = result->GetDict().FindString("next");
  std::move(callback).Run(tenor::mojom::Status::kHttpOk,
                          tenor::mojom::PaginatedGifResponses::New(
                              next ? *next : "", ParseGifs(gifs)));
}

void GifTenorApiFetcher::FetchCategories(
    GetCategoriesCallback callback,
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
        internal {
          contacts {
              email: "e14s-eng@google.com"
          }
        }
        user_data {
          type: USER_CONTENT
        }
        data:
          "None, (authentication to this API is done through Chrome's API key)."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2024-09-30"
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
      url_loader_factory,
      net::AppendQueryParameter(GURL(kTenorBaseUrl).Resolve(kCategoriesApi),
                                kClientKeyName, kClientKeyValue),
      kTrafficAnnotation);
  auto* const endpoint_fetcher_ptr = endpoint_fetcher.get();
  endpoint_fetcher_ptr->PerformRequest(
      base::BindOnce(&GifTenorApiFetcher::FetchCategoriesResponseHandler,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(endpoint_fetcher)),
      nullptr);
}

void GifTenorApiFetcher::FetchCategoriesResponseHandler(
    GetCategoriesCallback callback,
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
    GetCategoriesCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  const auto* tags = FindList(result, "tags");
  if (!tags) {
    std::move(callback).Run(tenor::mojom::Status::kHttpError,
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

  std::move(callback).Run(tenor::mojom::Status::kHttpOk, std::move(categories));
}

void GifTenorApiFetcher::FetchFeaturedGifs(
    TenorGifsApiCallback callback,
    const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::optional<std::string>& pos) {
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
        internal {
          contacts {
              email: "e14s-eng@google.com"
          }
        }
        user_data {
          type: USER_CONTENT
        }
        data:
          "Position of the next batch of GIFs, used for infiniate scroll."
          "Authentication to this API is done through Chrome's API key."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2024-09-30"
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
    const std::optional<std::string>& pos,
    std::optional<int> limit) {
  GURL url = GetUrl(kSearchApi, pos);
  url = net::AppendQueryParameter(url, "q", query);
  if (limit.has_value()) {
    url = net::AppendQueryParameter(url, "limit", base::NumberToString(*limit));
  }

  auto endpoint_fetcher = endpoint_fetcher_creator_.Run(
      url_loader_factory, url, kSearchTrafficAnnotation);
  auto* const endpoint_fetcher_ptr = endpoint_fetcher.get();
  endpoint_fetcher_ptr->PerformRequest(
      base::BindOnce(&GifTenorApiFetcher::TenorGifsApiResponseHandler,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(endpoint_fetcher)),
      nullptr);
}

std::unique_ptr<EndpointFetcher> GifTenorApiFetcher::FetchGifSearchCancellable(
    TenorGifsApiCallback callback,
    const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string_view query,
    const std::optional<std::string>& pos,
    std::optional<int> limit) {
  GURL url = GetUrl(kSearchApi, pos);
  url = net::AppendQueryParameter(url, "q", query);
  if (limit.has_value()) {
    url = net::AppendQueryParameter(url, "limit", base::NumberToString(*limit));
  }

  std::unique_ptr<EndpointFetcher> endpoint_fetcher =
      endpoint_fetcher_creator_.Run(url_loader_factory, url,
                                    kSearchTrafficAnnotation);
  CHECK_DEREF(endpoint_fetcher.get())
      .PerformRequest(
          base::BindOnce(&GifTenorApiFetcher::TenorGifsApiResponseHandler,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         /*endpoint_fetcher=*/nullptr),
          nullptr);
  return endpoint_fetcher;
}

void GifTenorApiFetcher::FetchGifsByIds(
    GetGifsByIdsCallback callback,
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
        internal {
          contacts {
              email: "e14s-eng@google.com"
          }
        }
        user_data {
          type: USER_CONTENT
        }
        data:
          "The IDs of the GIFS saved in recent."
          "Authentication to this API is done through Chrome's API key."
        destination: GOOGLE_OWNED_SERVICE
        last_reviewed: "2024-09-30"
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
      net::AppendQueryParameter(
          net::AppendQueryParameter(GURL(kTenorBaseUrl).Resolve(kPostsApi),
                                    kClientKeyName, kClientKeyValue),
          "ids", base::JoinString(ids, ",")),
      kTrafficAnnotation);
  auto* const endpoint_fetcher_ptr = endpoint_fetcher.get();
  endpoint_fetcher_ptr->PerformRequest(
      base::BindOnce(&GifTenorApiFetcher::FetchGifsByIdsResponseHandler,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(endpoint_fetcher)),
      nullptr);
}

void GifTenorApiFetcher::FetchGifsByIdsResponseHandler(
    GetGifsByIdsCallback callback,
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
                          std::vector<tenor::mojom::GifResponsePtr>{});
}

void GifTenorApiFetcher::OnGifsByIdsJsonParsed(
    GetGifsByIdsCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  const auto* gifs = FindList(result, "results");
  if (!gifs) {
    std::move(callback).Run(tenor::mojom::Status::kHttpError,
                            std::vector<tenor::mojom::GifResponsePtr>{});
    return;
  }

  std::move(callback).Run(tenor::mojom::Status::kHttpOk, ParseGifs(gifs));
}
}  // namespace ash
