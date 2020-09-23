// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/item_suggest_cache.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace app_list {
namespace {

// Maximum accepted size of an ItemSuggest response. 10 KB.
constexpr int kMaxResponseSize = 10 * 1024;

// TODO(crbug.com/1034842): Investigate:
//  - enterprise policies that should limit this traffic.
//  - settings that should disable drive results.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("launcher_item_suggest", R"(
      semantics {
        sender: "Launcher suggested drive files"
        description:
          "The Chrome OS launcher requests suggestions for Drive files from "
          "the Drive ItemSuggest API. These are displayed in the launcher."
        trigger:
          "Once on login after Drive FS is mounted. Afterwards, whenever the "
          "Chrome OS launcher is opened."
        data:
          "OAuth2 access token."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "This cannot be disabled."
      })");

// The scope required for an access token in order to query ItemSuggest.
constexpr char kDriveScope[] = "https://www.googleapis.com/auth/drive.readonly";

// TODO(crbug.com/1034842): Check this is correct. Also consider:
//  - controlling at least the scenario type by experiment param.
//  - whether we can filter the response to certain fields
constexpr char kRequestBody[] = R"({
      'max_suggestions': 5,
      'client_info': {
          'platform_type': 'CHROMEOS',
          'application_type': 'GOOGLE_DRIVE',
          'scenario_type': 'QUICK_ACCESS'
      }})";

//----------------
// Error utilities
//----------------

// Possible error states of the item suggest cache. These values persist to
// logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class Error {
  kDisabled = 1,
  kInvalidServerUrl = 2,
  kNoIdentityManager = 3,
  kGoogleAuthError = 4,
  kNetError = 5,
  k3xxError = 6,
  k4xxError = 7,
  k5xxError = 8,
  kEmptyResponse = 9,
  kNoResultsInResponse = 10,
  kJsonParseFailure = 11,
  kJsonConversionFailure = 12,
  kMaxValue = kJsonConversionFailure,
};

void LogError(Error error) {
  // TODO(crbug.com/1034842): Implement.
}

void LogResponseSize(const int size) {
  // TODO(crbug.com/1034842): Implement.
}

}  // namespace

// static
const base::Feature ItemSuggestCache::kExperiment{
    "LauncherItemSuggest", base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::FeatureParam<bool> ItemSuggestCache::kEnabled;
constexpr base::FeatureParam<std::string> ItemSuggestCache::kServerUrl;

ItemSuggestCache::ItemSuggestCache(
    Profile* profile,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : enabled_(kEnabled.Get()),
      server_url_(kServerUrl.Get()),
      profile_(profile),
      url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ItemSuggestCache::~ItemSuggestCache() = default;

void ItemSuggestCache::UpdateCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1034842): Add rate-limiting for cache updates.

  // Make no requests and exit in four cases:
  // - item suggest has been disabled via experiment
  // - the server url is not https
  // - the server url is not trusted by Google
  // - another request is in-flight (url_loader_ is non-null)
  if (url_loader_) {
    return;
  } else if (!enabled_) {
    LogError(Error::kDisabled);
    return;
  } else if (!server_url_.SchemeIs(url::kHttpsScheme) ||
             !google_util::IsGoogleAssociatedDomainUrl(server_url_)) {
    LogError(Error::kInvalidServerUrl);
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager) {
    LogError(Error::kNoIdentityManager);
    return;
  }

  signin::ScopeSet scopes({kDriveScope});

  // Fetch an OAuth2 access token.
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "launcher_item_suggest", identity_manager, scopes,
      base::BindOnce(&ItemSuggestCache::OnTokenReceived,
                     weak_factory_.GetWeakPtr()),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSync);
}

void ItemSuggestCache::OnTokenReceived(GoogleServiceAuthError error,
                                       signin::AccessTokenInfo token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    LogError(Error::kGoogleAuthError);
    return;
  }

  // Make a new request.
  url_loader_ = MakeRequestLoader(token_info.token);
  url_loader_->SetRetryOptions(0, network::SimpleURLLoader::RETRY_NEVER);
  url_loader_->AttachStringForUpload(kRequestBody, "application/json");

  // Perform the request.
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ItemSuggestCache::OnSuggestionsReceived,
                     weak_factory_.GetWeakPtr()),
      kMaxResponseSize);
}

void ItemSuggestCache::OnSuggestionsReceived(
    const std::unique_ptr<std::string> json_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int net_error = url_loader_->NetError();
  if (net_error != net::OK) {
    if (!url_loader_->ResponseInfo() || !url_loader_->ResponseInfo()->headers) {
      LogError(Error::kNetError);
    } else {
      const int status = url_loader_->ResponseInfo()->headers->response_code();
      if (status >= 500) {
        LogError(Error::k5xxError);
      } else if (status >= 400) {
        LogError(Error::k4xxError);
      } else if (status >= 300) {
        LogError(Error::k3xxError);
      }
    }

    return;
  } else if (!json_response || json_response->empty()) {
    LogError(Error::kEmptyResponse);
    return;
  }

  LogResponseSize(json_response->size());

  // Parse the JSON response from ItemSuggest.
  data_decoder::DataDecoder::ParseJsonIsolated(
      *json_response, base::BindOnce(&ItemSuggestCache::OnJsonParsed,
                                     weak_factory_.GetWeakPtr()));
}

void ItemSuggestCache::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.value) {
    LogError(Error::kJsonParseFailure);
    return;
  }

  // TODO(crbug.com/1034842): Convert json to result objects.
}

std::unique_ptr<network::SimpleURLLoader> ItemSuggestCache::MakeRequestLoader(
    const std::string& token) {
  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->method = "POST";
  resource_request->url = server_url_;
  // Do not allow cookies.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // Ignore the cache because we always want fresh results.
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;

  DCHECK(resource_request->url.is_valid());

  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + token);

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          kTrafficAnnotation);
}

}  // namespace app_list
