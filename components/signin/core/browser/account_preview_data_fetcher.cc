// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_fetcher.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace signin {

namespace {

// TODO(crbug.com/510760810): Exact content of the parser will need to be
// re-iterated on, when we will be able to fully test the final API.
//
// Parses the response from the stats endpoint. Returns std::nullopt if the
// response format is not as expected or if the data is empty.
std::optional<AccountPreviewData> ParseStatsResponse(
    const std::optional<std::string>& response_body,
    std::optional<AccountPreviewData> data) {
  if (!data.has_value() || !response_body.has_value()) {
    return std::nullopt;
  }
  std::optional<base::Value> value =
      base::JSONReader::Read(response_body.value(), base::JSON_PARSE_RFC);
  if (!value || !value->is_dict()) {
    return std::nullopt;
  }
  const auto& dict = value->GetDict();
  const auto* list = dict.FindList("dataTypeStatistics");
  if (!list) {
    return std::nullopt;
  }
  for (const auto& item : *list) {
    if (!item.is_dict()) {
      continue;
    }
    const auto& item_dict = item.GetDict();
    const std::string* name = item_dict.FindString("name");
    const std::string* count_str = item_dict.FindString("count");
    int64_t count = 0;
    if (count_str) {
      base::StringToInt64(*count_str, &count);
    } else {
      std::optional<int> count_int = item_dict.FindInt("count");
      if (count_int) {
        count = *count_int;
      }
    }

    if (name) {
      if (*name == "dataTypes/bookmarks/stats") {
        data->bookmark_count = count;
      } else if (*name == "dataTypes/passwords/stats") {
        data->password_count = count;
      } else if (*name == "dataTypes/history/stats") {
        data->history_count = count;
      }
    }
  }
  return data;
}

// Parses the response from the previews endpoint. Returns std::nullopt if the
// response format is not as expected or if the data is empty.
std::optional<AccountPreviewData> ParsePreviewsResponse(
    const std::optional<std::string>& response_body,
    std::optional<AccountPreviewData> data) {
  if (!data.has_value() || !response_body.has_value()) {
    return std::nullopt;
  }
  std::optional<base::Value> value =
      base::JSONReader::Read(response_body.value(), base::JSON_PARSE_RFC);
  if (!value || !value->is_dict()) {
    return std::nullopt;
  }
  const auto& dict = value->GetDict();
  const auto* list = dict.FindList("entitiesPreviews");
  if (!list) {
    return std::nullopt;
  }
  data->password_domains.clear();
  for (const auto& item : *list) {
    if (!item.is_dict()) {
      continue;
    }
    const auto& item_dict = item.GetDict();
    const auto* specifics = item_dict.FindDict("specifics");
    if (!specifics) {
      continue;
    }
    const auto* password_preview = specifics->FindDict("passwordPreview");
    if (!password_preview) {
      continue;
    }
    const std::string* url = password_preview->FindString("url");
    const std::string* domain =
        url ? url : password_preview->FindString("hashedUrl");
    if (domain) {
      data->password_domains.push_back(*domain);
    }
  }
  return data;
}

}  // namespace

AccountPreviewDataFetcher::AccountPreviewDataFetcher(
    const GaiaId& gaia_id,
    IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    FetchCompleteCallback callback)
    : gaia_id_(gaia_id),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      callback_(std::move(callback)) {
  CHECK(identity_manager_);
  AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByGaiaId(gaia_id_);
  if (account_info.IsEmpty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), gaia_id_, std::nullopt));
    return;
  }

  token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      account_info.account_id, OAuthConsumerId::kSyncPreview,
      base::BindOnce(&AccountPreviewDataFetcher::OnAccessTokenReceived,
                     weak_ptr_factory_.GetWeakPtr()),
      AccessTokenFetcher::Mode::kImmediate);
}

AccountPreviewDataFetcher::~AccountPreviewDataFetcher() = default;

void AccountPreviewDataFetcher::OnAccessTokenReceived(
    GoogleServiceAuthError error,
    AccessTokenInfo token_info) {
  token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(callback_).Run(gaia_id_, std::nullopt);
    return;
  }

  StartNetworkRequests(token_info.token);
}

void AccountPreviewDataFetcher::StartNetworkRequests(
    const std::string& access_token) {
  barrier_closure_ = base::BarrierClosure(
      2, base::BindOnce(&AccountPreviewDataFetcher::OnFetchCompleted,
                        weak_ptr_factory_.GetWeakPtr()));

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("chrome_sync_preview_fetcher", R"(
        semantics {
          sender: "Chrome Sync Preview Fetcher"
          description:
            "Fetches preview data (statistics and entities previews) for "
            "signed-in Google accounts to personalize sign-in promotions."
          trigger:
            "Triggered once every 24 hours for each signed-in account, or on "
            "startup."
          data:
            "OAuth2 access token for the account."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "chrome-signin-team@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          last_reviewed: "2026-05-22"
        }
        policy {
          cookies_allowed: NO
          setting:
            "The fetch is only performed for accounts that have valid cookies."
          chrome_policy {
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
          }
        })");

  // 1. Stats Request
  auto stats_request = std::make_unique<network::ResourceRequest>();
  stats_request->url = GURL(kAccountPreviewStatsUrl);
  stats_request->method = "GET";
  stats_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  stats_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                   base::StrCat({"Bearer ", access_token}));

  stats_url_loader_ = network::SimpleURLLoader::Create(std::move(stats_request),
                                                       traffic_annotation);

  stats_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&AccountPreviewDataFetcher::OnStatsFetchCompleted,
                     weak_ptr_factory_.GetWeakPtr()));

  // 2. Previews Request
  auto previews_request = std::make_unique<network::ResourceRequest>();
  previews_request->url = GURL(kAccountPreviewPreviewsUrl);
  previews_request->method = "GET";
  previews_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  previews_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      base::StrCat({"Bearer ", access_token}));

  previews_url_loader_ = network::SimpleURLLoader::Create(
      std::move(previews_request), traffic_annotation);

  previews_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&AccountPreviewDataFetcher::OnPreviewsFetchCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccountPreviewDataFetcher::OnStatsFetchCompleted(
    std::optional<std::string> response_body) {
  stats_url_loader_.reset();
  fetched_data_ = ParseStatsResponse(response_body, std::move(fetched_data_));
  barrier_closure_.Run();
}

void AccountPreviewDataFetcher::OnPreviewsFetchCompleted(
    std::optional<std::string> response_body) {
  previews_url_loader_.reset();
  fetched_data_ =
      ParsePreviewsResponse(response_body, std::move(fetched_data_));
  barrier_closure_.Run();
}

void AccountPreviewDataFetcher::OnFetchCompleted() {
  // PostTask is required here because `barrier_closure_` is owned by `this`
  // and is triggering this callback (`OnFetchCompleted()`). If `callback_`
  // causes `this` to be deleted, the destruction of `barrier_closure_` could
  // result in a use-after-free.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), gaia_id_, std::move(fetched_data_)));
}

}  // namespace signin
