// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_fetcher.h"

#include <algorithm>
#include <utility>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/time.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace signin {

namespace {

constexpr char kStablePreviewUrl[] =
    "https://chromesyncpreview.pa.googleapis.com/v1";
constexpr char kStagingPreviewUrl[] =
    "https://alpha-chromesyncpreview-googleapis.pa.sandbox.google.com/v1";

constexpr char kFetchStateHistogram[] = "Signin.AccountPreviewData.FetchState";
constexpr char kFetchHit429Histogram[] =
    "Signin.AccountPreviewData.FetchHit429";
constexpr char kFetchDurationSuccessHistogram[] =
    "Signin.AccountPreviewData.FetchDuration.Success";
constexpr char kFetchDurationFailureHistogram[] =
    "Signin.AccountPreviewData.FetchDuration.Failure";
constexpr char kFetchDurationTokenFailureHistogram[] =
    "Signin.AccountPreviewData.FetchDuration.TokenFailure";

// Parses the specifics field number (data type ID) from the stats name string.
// Returns std::nullopt if the format doesn't match or cannot be parsed.
std::optional<int> ParseDataTypeId(std::string_view name) {
  // Expected format: "dataTypes/{data_type_number}/dataTypeStatistics"
  static constexpr std::string_view kPrefix = "dataTypes/";
  static constexpr std::string_view kSuffix = "/dataTypeStatistics";
  if (!name.starts_with(kPrefix) || !name.ends_with(kSuffix)) {
    return std::nullopt;
  }
  std::string_view number_str = name.substr(
      kPrefix.size(), name.size() - kPrefix.size() - kSuffix.size());
  int id = 0;
  if (base::StringToInt(number_str, &id)) {
    return id;
  }
  return std::nullopt;
}

// Parses the response from the stats endpoint. Returns true if the
// response format is as expected (or if the data is properly structured
// but empty).
bool ParseStatsResponse(const std::string& response_body,
                        AccountPreviewData& data) {
  std::optional<base::Value> value =
      base::JSONReader::Read(response_body, base::JSON_PARSE_RFC);
  if (!value || !value->is_dict()) {
    return false;
  }
  const auto& dict = value->GetDict();
  const auto* list = dict.FindList("dataTypeStatistics");
  if (!list) {
    // An empty valid result is still considered a success.
    return true;
  }
  for (const auto& item : *list) {
    if (!item.is_dict()) {
      continue;
    }

    const base::DictValue& data_type_statistic = item.GetDict();
    const std::string* data_type_name = data_type_statistic.FindString("name");
    const std::string* count_str = data_type_statistic.FindString("count");
    if (!data_type_name || !count_str) {
      continue;
    }
    std::optional<int> type_id = ParseDataTypeId(*data_type_name);
    if (!type_id) {
      continue;
    }
    syncer::DataType type =
        syncer::GetDataTypeFromSpecificsFieldNumber(*type_id);
    if (!syncer::IsRealDataType(type)) {
      continue;
    }

    int64_t count_int64 = 0;
    base::StringToInt64(*count_str, &count_int64);
    // Counts should always be non-negative.
    size_t count = count_int64 >= 0 ? static_cast<size_t>(count_int64) : 0;
    data.counts[type] = count;
  }
  return true;
}

// Parses the response from the previews endpoint (ProtoJSON format). Returns
// true if the response format is as expected (or if the data is properly
// structured but empty).
bool ParsePreviewsResponse(
    const std::string& response_body,
    AccountPreviewData& data,
    const base::flat_set<std::string>& current_device_cache_guids) {
  std::optional<base::Value> value =
      base::JSONReader::Read(response_body, base::JSON_PARSE_RFC);
  if (!value || !value->is_dict()) {
    return false;
  }
  const auto& dict = value->GetDict();
  const auto* list = dict.FindList("entitiesPreviews");
  if (!list) {
    // An empty valid result is still considered a success.
    return true;
  }

  data.devices.clear();
  for (const auto& item : *list) {
    if (!item.is_dict()) {
      continue;
    }
    const auto* specifics_preview = item.GetDict().FindDict("specificsPreview");
    if (!specifics_preview) {
      continue;
    }
    const auto* device_info_preview =
        specifics_preview->FindDict("deviceInfoPreview");
    if (!device_info_preview) {
      continue;
    }

    const std::string* cache_guid =
        device_info_preview->FindString("cacheGuid");
    if (!cache_guid) {
      continue;
    }

    // Filter out current device.
    if (current_device_cache_guids.contains(*cache_guid)) {
      continue;
    }

    const std::string* sync_user_agent =
        device_info_preview->FindString("syncUserAgent");
    // Filter out non-Chrome devices (e.g. Google Play Services or iGSA).
    if (!device_info_preview->FindDict("chromeVersionInfo") ||
        (sync_user_agent && sync_user_agent->starts_with("iGSA"))) {
      continue;
    }

    DevicePreview device;
    device.cache_guid = *cache_guid;

    // ProtoJSON serializes uint64/int64 values as strings (or numbers).
    int64_t timestamp = 0;
    if (const std::string* ts_str =
            device_info_preview->FindString("lastUpdatedTimestamp")) {
      base::StringToInt64(*ts_str, &timestamp);
    } else if (std::optional<double> ts_double =
                   device_info_preview->FindDouble("lastUpdatedTimestamp")) {
      timestamp = static_cast<int64_t>(*ts_double);
    }
    device.last_updated = syncer::ProtoTimeToTime(timestamp);

    int os_type_int = device_info_preview->FindInt("osType").value_or(0);
    device.os_type = static_cast<sync_pb::SyncEnums_OsType>(os_type_int);

    int form_factor_int =
        device_info_preview->FindInt("deviceFormFactor").value_or(0);
    device.form_factor =
        static_cast<sync_pb::SyncEnums_DeviceFormFactor>(form_factor_int);

    data.devices.push_back(std::move(device));
  }
  return true;
}

std::string_view GetBaseUrl(version_info::Channel channel) {
  if (channel == version_info::Channel::STABLE ||
      channel == version_info::Channel::BETA) {
    return kStablePreviewUrl;
  }
  // This URL is also used for testing.
  return kStagingPreviewUrl;
}

}  // namespace

// The list of data types to fetch statistics for. This list explicitly requests
// only the data types currently used by metrics.
// static
GURL AccountPreviewDataFetcher::GetStatsUrlForChannel(
    version_info::Channel channel) {
  GURL url(
      base::StrCat({GetBaseUrl(channel), "/dataTypes/-/dataTypesStatistics"}));
  for (syncer::DataType data_type : signin::kRequestedDataTypes) {
    url = net::AppendQueryParameter(
        url, "dataTypes",
        base::NumberToString(
            syncer::GetSpecificsFieldNumberFromDataType(data_type)));
  }
  return url;
}

// static
GURL AccountPreviewDataFetcher::GetPreviewsUrlForChannel(
    version_info::Channel channel) {
  // Requesting only DEVICE_INFO.
  return GURL(base::StrCat(
      {GetBaseUrl(channel), "/dataTypes/",
       base::NumberToString(
           syncer::GetSpecificsFieldNumberFromDataType(syncer::DEVICE_INFO)),
       "/entitiesPreviews"}));
}

AccountPreviewDataFetcher::AccountPreviewDataFetcher(
    const GaiaId& gaia_id,
    IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel,
    base::flat_set<std::string> current_device_cache_guids,
    FetchCompleteCallback callback)
    : gaia_id_(gaia_id),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      channel_(channel),
      current_device_cache_guids_(std::move(current_device_cache_guids)),
      callback_(std::move(callback)) {
  CHECK(identity_manager_);
}

AccountPreviewDataFetcher::~AccountPreviewDataFetcher() = default;

void AccountPreviewDataFetcher::Start() {
  if (is_started_) {
    return;
  }
  is_started_ = true;
  fetch_timer_ = base::ElapsedTimer();

  AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByGaiaId(gaia_id_);
  if (account_info.IsEmpty()) {
    fetched_data_ = std::nullopt;
    CompleteFetch();
    return;
  }

  token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      account_info.GetAccountId(), OAuthConsumerId::kSyncPreview,
      base::BindOnce(&AccountPreviewDataFetcher::OnAccessTokenReceived,
                     weak_ptr_factory_.GetWeakPtr()),
      AccessTokenFetcher::Mode::kImmediate);
}

void AccountPreviewDataFetcher::OnAccessTokenReceived(
    GoogleServiceAuthError error,
    AccessTokenInfo token_info) {
  token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    CHECK(fetch_timer_.has_value());
    base::UmaHistogramMediumTimes(kFetchDurationTokenFailureHistogram,
                                  fetch_timer_->Elapsed());
    fetched_data_ = std::nullopt;
    CompleteFetch();
    return;
  }

  StartNetworkRequests(token_info.token);
}

void AccountPreviewDataFetcher::SetOnFetchCompletedForTesting(
    base::OnceClosure closure) {
  on_fetch_completed_for_testing_ = std::move(closure);
}

void AccountPreviewDataFetcher::StartNetworkRequests(
    const std::string& access_token) {
  const bool fetch_previews = base::FeatureList::IsEnabled(
      switches::kEnableAccountPreviewEntityPreviews);

  barrier_callback_ = base::BarrierCallback<bool>(
      fetch_previews ? 2 : 1,
      base::BindOnce(&AccountPreviewDataFetcher::OnFetchCompleted,
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
  stats_request->url = GetStatsUrlForChannel(channel_);
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

  if (fetch_previews) {
    // 2. Previews Request
    auto previews_request = std::make_unique<network::ResourceRequest>();
    previews_request->url = GetPreviewsUrlForChannel(channel_);
    previews_request->method = "GET";
    previews_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    previews_request->headers.SetHeader(
        net::HttpRequestHeaders::kAuthorization,
        base::StrCat({"Bearer ", access_token}));

    previews_url_loader_ = network::SimpleURLLoader::Create(
        std::move(previews_request), traffic_annotation);

    previews_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&AccountPreviewDataFetcher::OnPreviewsFetchCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  base::UmaHistogramEnumeration(kFetchStateHistogram, FetchState::kRequested);
}

void AccountPreviewDataFetcher::OnStatsFetchCompleted(
    std::optional<std::string> response_body) {
  int response_code =
      stats_url_loader_->ResponseInfo() &&
              stats_url_loader_->ResponseInfo()->headers
          ? stats_url_loader_->ResponseInfo()->headers->response_code()
          : -1;
  if (response_code == net::HTTP_TOO_MANY_REQUESTS) {
    hit_429_error_ = true;
  }
  stats_url_loader_.reset();
  bool is_http_success = (response_code == net::HTTP_OK);
  base::UmaHistogramEnumeration(kFetchStateHistogram,
                                response_body.has_value() && is_http_success
                                    ? FetchState::kStatisticsHasResult
                                    : FetchState::kStatisticsEmptyResult);
  bool success = is_http_success && response_body.has_value() &&
                 ParseStatsResponse(*response_body, *fetched_data_);
  barrier_callback_.Run(success);
}

void AccountPreviewDataFetcher::OnPreviewsFetchCompleted(
    std::optional<std::string> response_body) {
  int response_code =
      previews_url_loader_->ResponseInfo() &&
              previews_url_loader_->ResponseInfo()->headers
          ? previews_url_loader_->ResponseInfo()->headers->response_code()
          : -1;
  if (response_code == net::HTTP_TOO_MANY_REQUESTS) {
    hit_429_error_ = true;
  }
  previews_url_loader_.reset();
  bool is_http_success = (response_code == net::HTTP_OK);
  base::UmaHistogramEnumeration(kFetchStateHistogram,
                                response_body.has_value() && is_http_success
                                    ? FetchState::kEntityPreviewHasResult
                                    : FetchState::kEntityPreviewEmptyResult);
  bool success = is_http_success && response_body.has_value() &&
                 ParsePreviewsResponse(*response_body, *fetched_data_,
                                       current_device_cache_guids_);
  barrier_callback_.Run(success);
}

void AccountPreviewDataFetcher::OnFetchCompleted(std::vector<bool> results) {
  // If all requests failed, clear the fetched data.
  if (std::ranges::none_of(results, [](bool success) { return success; })) {
    fetched_data_ = std::nullopt;
  }

  base::UmaHistogramEnumeration(kFetchStateHistogram,
                                fetched_data_.has_value()
                                    ? FetchState::kCompletedWithResults
                                    : FetchState::kCompletedWithoutResults);

  base::UmaHistogramBoolean(kFetchHit429Histogram, hit_429_error_);

  CHECK(fetch_timer_.has_value());
  base::UmaHistogramMediumTimes(fetched_data_.has_value()
                                    ? kFetchDurationSuccessHistogram
                                    : kFetchDurationFailureHistogram,
                                fetch_timer_->Elapsed());

  CompleteFetch();
}

void AccountPreviewDataFetcher::CompleteFetch() {
  // `PostTask` + `WeakPtr` prevents re-entrancy and stack-unwinding UAFs, and a
  // subtle race where `this` is destroyed after posting but prior to execution
  // (see crbug.com/533927599, crbug.com/542550030).
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AccountPreviewDataFetcher::RunCallback,
                                weak_ptr_factory_.GetWeakPtr()));
  if (on_fetch_completed_for_testing_) {
    std::move(on_fetch_completed_for_testing_).Run();
  }
}

void AccountPreviewDataFetcher::RunCallback() {
  CHECK(callback_);
  std::move(callback_).Run(gaia_id_, std::move(fetched_data_), hit_429_error_);
}

}  // namespace signin
