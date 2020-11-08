// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/item_suggest_cache.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/drive/drive_pref_names.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
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

// Maximum accepted size of an ItemSuggest response. 1MB.
constexpr int kMaxResponseSize = 1024 * 1024;

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
          "This cannot be disabled except by policy."
        chrome_policy {
          DriveDisabled {
            DriveDisabled: true
          }
        }
      })");

// The scope required for an access token in order to query ItemSuggest.
constexpr char kDriveScope[] = "https://www.googleapis.com/auth/drive.readonly";

constexpr char kRequestBody[] = R"({
      'client_info': {
        'platform_type': 'CHROME_OS',
        'scenario_type': 'CHROME_OS_ZSS_FILES',
        'request_type': 'BACKGROUND_REQUEST'
      },
      'max_suggestions': 10,
      'type_detail_fields': 'drive_item.title,justification.display_text'
    })";

bool IsDisabledByPolicy(const Profile* profile) {
  return profile->GetPrefs()->GetBoolean(drive::prefs::kDisableDrive);
}

//------------------
// Metrics utilities
//------------------

// TODO(crbug.com/1034842): Add unit tests for histograms

// Possible outcomes of a call to the ItemSuggest API. These values persist to
// logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class Status {
  kOk = 0,
  kDisabledByExperiment = 1,
  kDisabledByPolicy = 2,
  kInvalidServerUrl = 3,
  kNoIdentityManager = 4,
  kGoogleAuthError = 5,
  kNetError = 6,
  kResponseTooLarge = 7,
  k3xxStatus = 8,
  k4xxStatus = 9,
  k5xxStatus = 10,
  kEmptyResponse = 11,
  kNoResultsInResponse = 12,
  kJsonParseFailure = 13,
  kJsonConversionFailure = 14,
  kMaxValue = kJsonConversionFailure,
};

void LogStatus(Status status) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.ItemSuggestCache.Status", status);
}

void LogResponseSize(const int size) {
  UMA_HISTOGRAM_COUNTS_100000("Apps.AppList.ItemSuggestCache.ResponseSize",
                              size);
}

//---------------
// JSON utilities
//---------------

base::Optional<base::Value::ConstListView> GetList(const base::Value* value,
                                                   const std::string& key) {
  if (!value->is_dict())
    return base::nullopt;
  const base::Value* field = value->FindListKey(key);
  if (!field)
    return base::nullopt;
  return field->GetList();
}

base::Optional<std::string> GetString(const base::Value* value,
                                      const std::string& key) {
  if (!value->is_dict())
    return base::nullopt;
  const std::string* field = value->FindStringKey(key);
  if (!field)
    return base::nullopt;
  return *field;
}

//----------------------
// JSON response parsing
//----------------------

base::Optional<ItemSuggestCache::Result> ConvertResult(
    const base::Value* value) {
  const auto& item_id = GetString(value, "itemId");
  const auto& display_text = GetString(value, "displayText");

  if (!item_id || !display_text)
    return base::nullopt;

  return ItemSuggestCache::Result(item_id.value(), display_text.value());
}

base::Optional<ItemSuggestCache::Results> ConvertResults(
    const base::Value* value) {
  const auto& suggestion_id = GetString(value, "suggestionSessionId");
  if (!suggestion_id)
    return base::nullopt;

  ItemSuggestCache::Results results(suggestion_id.value());

  const auto items = GetList(value, "item");
  if (!items)
    return base::nullopt;

  for (const auto& result_value : items.value()) {
    auto result = ConvertResult(&result_value);
    // If any result fails conversion, fail completely and return base::nullopt,
    // rather than just skipping this result. This makes clear the distinction
    // between a response format issue and the response containing no results.
    if (!result)
      return base::nullopt;
    results.results.push_back(std::move(result.value()));
  }

  return results;
}

}  // namespace

// static
const base::Feature ItemSuggestCache::kExperiment{
    "LauncherItemSuggest", base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::FeatureParam<bool> ItemSuggestCache::kEnabled;
constexpr base::FeatureParam<std::string> ItemSuggestCache::kServerUrl;
constexpr base::FeatureParam<int> ItemSuggestCache::kMinMinutesBetweenUpdates;

ItemSuggestCache::Result::Result(const std::string& id,
                                 const std::string& title)
    : id(id), title(title) {}

ItemSuggestCache::Result::Result(const Result& other)
    : id(other.id), title(other.title) {}

ItemSuggestCache::Result::~Result() = default;

ItemSuggestCache::Results::Results(const std::string& suggestion_id)
    : suggestion_id(suggestion_id) {}

ItemSuggestCache::Results::Results(const Results& other)
    : suggestion_id(other.suggestion_id), results(other.results) {}

ItemSuggestCache::Results::~Results() = default;

ItemSuggestCache::ItemSuggestCache(
    Profile* profile,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : enabled_(kEnabled.Get()),
      server_url_(kServerUrl.Get()),
      min_time_between_updates_(
          base::TimeDelta::FromMinutes(kMinMinutesBetweenUpdates.Get())),
      profile_(profile),
      url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ItemSuggestCache::~ItemSuggestCache() = default;

base::Optional<ItemSuggestCache::Results> ItemSuggestCache::GetResults() {
  // Return a copy because a pointer to |results_| will become invalid whenever
  // the cache is updated.
  return results_;
}

void ItemSuggestCache::UpdateCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& now = base::Time::Now();
  if (now - time_of_last_update_ < min_time_between_updates_)
    return;
  time_of_last_update_ = now;

  // Make no requests and exit in these cases:
  // - another request is in-flight (url_loader_ is non-null)
  // - item suggest has been disabled via experiment
  // - item suggest has been disabled by policy
  // - the server url is not https or not trusted by Google
  if (url_loader_) {
    return;
  } else if (!enabled_) {
    LogStatus(Status::kDisabledByExperiment);
    return;
  } else if (IsDisabledByPolicy(profile_)) {
    LogStatus(Status::kDisabledByPolicy);
    return;
  } else if (!server_url_.SchemeIs(url::kHttpsScheme) ||
             !google_util::IsGoogleAssociatedDomainUrl(server_url_)) {
    LogStatus(Status::kInvalidServerUrl);
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager) {
    LogStatus(Status::kNoIdentityManager);
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
    LogStatus(Status::kGoogleAuthError);
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
      if (net_error == net::ERR_INSUFFICIENT_RESOURCES) {
        LogStatus(Status::kResponseTooLarge);
      } else {
        LogStatus(Status::kNetError);
      }
    } else {
      const int status = url_loader_->ResponseInfo()->headers->response_code();
      if (status >= 500) {
        LogStatus(Status::k5xxStatus);
      } else if (status >= 400) {
        LogStatus(Status::k4xxStatus);
      } else if (status >= 300) {
        LogStatus(Status::k3xxStatus);
      }
    }

    return;
  } else if (!json_response || json_response->empty()) {
    LogStatus(Status::kEmptyResponse);
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
    LogStatus(Status::kJsonParseFailure);
    return;
  }

  // Convert the JSON value into a Results object. If the conversion fails, or
  // if the conversion contains no results, we shouldn't update the stored
  // results.
  const auto& results = ConvertResults(&result.value.value());
  if (!results) {
    LogStatus(Status::kJsonConversionFailure);
  } else if (results->results.empty()) {
    LogStatus(Status::kNoResultsInResponse);
  } else {
    LogStatus(Status::kOk);
    results_ = std::move(results.value());
  }
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

// static
base::Optional<ItemSuggestCache::Results> ItemSuggestCache::ConvertJsonForTest(
    const base::Value* value) {
  return ConvertResults(value);
}

}  // namespace app_list
