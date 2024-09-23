// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <string>
#include <unordered_map>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_server_proxy.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// For creating endpoint fetcher.
const int kDefaultTimeoutMs = 5000;
const char kTimeoutParam[] = "subscriptions_server_request_timeout";
constexpr base::FeatureParam<int> kTimeoutMs{&commerce::kShoppingList,
                                             kTimeoutParam, kDefaultTimeoutMs};

const char kDefaultServiceBaseUrl[] =
    "https://memex-pa.googleapis.com/v1/shopping/subscriptions";
const char kBaseUrlParam[] = "subscriptions_service_base_url";
constexpr base::FeatureParam<std::string> kServiceBaseUrl{
    &commerce::kShoppingList, kBaseUrlParam, kDefaultServiceBaseUrl};

const char kGetQueryParams[] = "?requestParams.subscriptionType=";
const char kManageQueryParams[] = "?requestSnapshotParams.subscriptionType=";
const char kPriceTrackGetParam[] = "PRICE_TRACK";

// For generating server requests and deserializing the responses.
const char kSubscriptionsKey[] = "subscriptions";
const char kCreateRequestParamsKey[] = "createShoppingSubscriptionsParams";
const char kEventTimestampsKey[] = "eventTimestampMicros";
const char kDeleteRequestParamsKey[] = "removeShoppingSubscriptionsParams";
const char kStatusKey[] = "status";
const char kStatusCodeKey[] = "code";
const int kBackendCanonicalCodeSuccess = 0;

// For (de)serializing subscription.
const char kSubscriptionTypeKey[] = "type";
const char kSubscriptionIdTypeKey[] = "identifierType";
const char kSubscriptionIdKey[] = "identifier";
const char kSubscriptionManagementTypeKey[] = "managementType";
const char kSubscriptionTimestampKey[] = "eventTimestampMicros";
const char kSubscriptionSeenOfferKey[] = "userSeenOffer";
const char kSeenOfferIdKey[] = "offerId";
const char kSeenOfferPriceKey[] = "seenPriceMicros";
const char kSeenOfferCountryKey[] = "countryCode";
const char kSeenOfferLocaleKey[] = "languageCode";

}  // namespace

namespace commerce {

SubscriptionsServerProxy::SubscriptionsServerProxy(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager),
      weak_ptr_factory_(this) {}
SubscriptionsServerProxy::~SubscriptionsServerProxy() = default;

void SubscriptionsServerProxy::Create(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    ManageSubscriptionsFetcherCallback callback) {
  CHECK(subscriptions->size() > 0);
  std::string service_url = kServiceBaseUrl.Get() + kManageQueryParams;
  if ((*subscriptions)[0].type == SubscriptionType::kPriceTrack) {
    service_url += kPriceTrackGetParam;
  } else {
    VLOG(1) << "Unsupported type for Create query";
    std::move(callback).Run(
        SubscriptionsRequestStatus::kInvalidArgument,
        std::make_unique<std::vector<CommerceSubscription>>());
    return;
  }

  base::Value::List subscriptions_list;
  for (const auto& subscription : *subscriptions) {
    subscriptions_list.Append(Serialize(subscription));
  }
  base::Value::Dict subscriptions_json;
  subscriptions_json.Set(kSubscriptionsKey, std::move(subscriptions_list));
  base::Value::Dict request_json;
  request_json.Set(kCreateRequestParamsKey, std::move(subscriptions_json));
  std::string post_data;
  base::JSONWriter::Write(request_json, &post_data);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "chrome_commerce_subscriptions_create", R"(
        semantics {
          sender: "Chrome Shopping"
          description:
            "Create new shopping subscriptions containing the product offers "
            "for tracking prices. These subscriptions will be stored on the"
            "server."
          trigger:
            "A user-initiated request is sent when the user explicitly tracks "
            "the product via the product page menu. A Chrome-initiated request "
            "is automatically sent on Chrome startup after the user has opted "
            "in to the tab-based price tracking feature from the tab switcher "
            "menu."
          data:
            "The list of subscriptions to be added, each of which contains a "
            "subscription type, a subscription id, the user seen offer price "
            "and offer locale. The request also includes an OAuth2 token "
            "authenticating the user."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is only enabled for signed-in users. User-initiated "
            "subscriptions can be managed in the user's Bookmarks. "
            "Chrome-initiated subscriptions can be removed when the user opts "
            "out of the tab-based price tracking feature from the tab switcher "
            "menu."
          chrome_policy {
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
          }
        })");

  auto fetcher = CreateEndpointFetcher(GURL(service_url), kPostHttpMethod,
                                       post_data, traffic_annotation);
  auto* const fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(
      &SubscriptionsServerProxy::HandleManageSubscriptionsResponses,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), std::move(fetcher)));
}

void SubscriptionsServerProxy::Delete(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    ManageSubscriptionsFetcherCallback callback) {
  CHECK(subscriptions->size() > 0);
  std::string service_url = kServiceBaseUrl.Get() + kManageQueryParams;
  if ((*subscriptions)[0].type == SubscriptionType::kPriceTrack) {
    service_url += kPriceTrackGetParam;
  } else {
    VLOG(1) << "Unsupported type for Delete query";
    std::move(callback).Run(
        SubscriptionsRequestStatus::kInvalidArgument,
        std::make_unique<std::vector<CommerceSubscription>>());
    return;
  }

  base::Value::List deletions_list;
  for (const auto& subscription : *subscriptions) {
    if (subscription.timestamp != kUnknownSubscriptionTimestamp)
      deletions_list.Append(base::Int64ToValue(subscription.timestamp));
  }
  base::Value::Dict deletions_json;
  deletions_json.Set(kEventTimestampsKey, std::move(deletions_list));
  base::Value::Dict request_json;
  request_json.Set(kDeleteRequestParamsKey, std::move(deletions_json));
  std::string post_data;
  base::JSONWriter::Write(request_json, &post_data);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "chrome_commerce_subscriptions_delete", R"(
        semantics {
          sender: "Chrome Shopping"
          description:
            "Delete one or more shopping subscriptions. These subscriptions "
            "were stored on the server previously for tracking prices."
          trigger:
            "A user-initiated request is sent when the user explicitly "
            "untracks the product via the product page menu. A "
            "Chrome-initiated request is automatically sent when the user "
            "navigates away from product pages if the user has opted in to the "
            "tab-based price tracking feature from the tab switcher menu."
          data:
            "The list of subscriptions to be deleted, each of which contains "
            "the subscription's creation timestamp. The request also includes "
            "an OAuth2 token authenticating the user."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is only enabled for signed-in users. User-initiated "
            "subscriptions can be managed in the user's Bookmarks. "
            "Chrome-initiated subscriptions can be removed when the user opts "
            "out of the tab-based price tracking feature from the tab switcher "
            "menu."
          chrome_policy {
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
          }
        })");

  auto fetcher = CreateEndpointFetcher(GURL(service_url), kPostHttpMethod,
                                       post_data, traffic_annotation);
  auto* const fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(
      &SubscriptionsServerProxy::HandleManageSubscriptionsResponses,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), std::move(fetcher)));
}

void SubscriptionsServerProxy::Get(SubscriptionType type,
                                   GetSubscriptionsFetcherCallback callback) {
  std::string service_url = kServiceBaseUrl.Get() + kGetQueryParams;
  if (type == SubscriptionType::kPriceTrack) {
    service_url += kPriceTrackGetParam;
  } else {
    VLOG(1) << "Unsupported type for Get query";
    std::move(callback).Run(
        SubscriptionsRequestStatus::kInvalidArgument,
        std::make_unique<std::vector<CommerceSubscription>>());
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("chrome_commerce_subscriptions_get",
                                          R"(
        semantics {
          sender: "Chrome Shopping"
          description:
            "Retrieve all shopping subscriptions of a user for a specified "
            "type. These subscriptions will be stored locally for later query."
          trigger:
            "On Chrome startup, or after the user changes their primary "
            "account."
          data:
            "The request includes a subscription type to be retrieved and an "
            "OAuth2 token authenticating the user. The response includes a "
            "list of subscriptions, each of which contains a subscription type,"
            " a subscription id, and the subscription's creation timestamp."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is only enabled for signed-in users. User-initiated "
            "subscriptions can be managed in the user's Bookmarks. "
            "Chrome-initiated subscriptions can be removed when the user opts "
            "out of the tab-based price tracking feature from the tab switcher "
            "menu."
          chrome_policy {
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
          }
        })");

  auto fetcher = CreateEndpointFetcher(GURL(service_url), kGetHttpMethod,
                                       kEmptyPostData, traffic_annotation);
  auto* const fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(
      &SubscriptionsServerProxy::HandleGetSubscriptionsResponses,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), std::move(fetcher)));
}

std::unique_ptr<EndpointFetcher>
SubscriptionsServerProxy::CreateEndpointFetcher(
    const GURL& url,
    const std::string& http_method,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  // If ReplaceSyncPromosWithSignInPromos is enabled - ConsentLevel::kSync is no
  // longer attainable. See crbug.com/1503156 for details.
  signin::ConsentLevel consent_level =
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
          ? signin::ConsentLevel::kSignin
          : signin::ConsentLevel::kSync;
  return std::make_unique<EndpointFetcher>(
      url_loader_factory_, kOAuthName, url, http_method, kContentType,
      std::vector<std::string>{kOAuthScope},
      base::Milliseconds(kTimeoutMs.Get()), post_data, annotation_tag,
      identity_manager_, consent_level);
}

void SubscriptionsServerProxy::HandleManageSubscriptionsResponses(
    ManageSubscriptionsFetcherCallback callback,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> responses) {
  if (responses->http_status_code != net::HTTP_OK || responses->error_type) {
    VLOG(1) << "Server failed to parse manage-subscriptions request";
    std::move(callback).Run(
        SubscriptionsRequestStatus::kServerParseError,
        std::make_unique<std::vector<CommerceSubscription>>());
    return;
  }
  data_decoder::DataDecoder::ParseJsonIsolated(
      responses->response,
      base::BindOnce(&SubscriptionsServerProxy::OnManageSubscriptionsJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SubscriptionsServerProxy::OnManageSubscriptionsJsonParsed(
    ManageSubscriptionsFetcherCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (result.has_value() && result->is_dict()) {
    if (auto* status_value = result->GetDict().FindDict(kStatusKey)) {
      if (auto status_code = status_value->FindInt(kStatusCodeKey)) {
        if (*status_code == kBackendCanonicalCodeSuccess) {
          std::move(callback).Run(SubscriptionsRequestStatus::kSuccess,
                                  GetSubscriptionsFromParsedJson(result));
        } else {
          std::move(callback).Run(
              SubscriptionsRequestStatus::kServerInternalError,
              std::make_unique<std::vector<CommerceSubscription>>());
        }
        return;
      }
    }
  }

  VLOG(1) << "Fail to get status code from response";
  std::move(callback).Run(
      SubscriptionsRequestStatus::kServerInternalError,
      std::make_unique<std::vector<CommerceSubscription>>());
}

void SubscriptionsServerProxy::HandleGetSubscriptionsResponses(
    GetSubscriptionsFetcherCallback callback,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> responses) {
  if (responses->http_status_code != net::HTTP_OK || responses->error_type) {
    VLOG(1) << "Server failed to parse get-subscriptions request";
    std::move(callback).Run(
        SubscriptionsRequestStatus::kServerParseError,
        std::make_unique<std::vector<CommerceSubscription>>());
    return;
  }
  data_decoder::DataDecoder::ParseJsonIsolated(
      responses->response,
      base::BindOnce(&SubscriptionsServerProxy::OnGetSubscriptionsJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SubscriptionsServerProxy::OnGetSubscriptionsJsonParsed(
    GetSubscriptionsFetcherCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  auto subscriptions = GetSubscriptionsFromParsedJson(result);
  if (subscriptions->size() == 0) {
    VLOG(1) << "User has no subscriptions";
  }
  std::move(callback).Run(SubscriptionsRequestStatus::kSuccess,
                          std::move(subscriptions));
}

std::unique_ptr<std::vector<CommerceSubscription>>
SubscriptionsServerProxy::GetSubscriptionsFromParsedJson(
    const data_decoder::DataDecoder::ValueOrError& result) {
  auto subscriptions = std::make_unique<std::vector<CommerceSubscription>>();
  if (result.has_value() && result->is_dict()) {
    if (auto* subscriptions_json =
            result->GetDict().FindList(kSubscriptionsKey)) {
      for (const auto& subscription_json : *subscriptions_json) {
        if (auto subscription = Deserialize(subscription_json))
          subscriptions->push_back(*subscription);
      }
    }
  }
  return subscriptions;
}

bool SubscriptionsServerProxy::IsPriceTrackingLocaleKeyEnabled() {
  return base::FeatureList::IsEnabled(kPriceTrackingSubscriptionServiceLocaleKey);
}

base::Value::Dict SubscriptionsServerProxy::Serialize(
    const CommerceSubscription& subscription) {
  base::Value::Dict subscription_json;
  subscription_json.Set(kSubscriptionTypeKey,
                        SubscriptionTypeToString(subscription.type));
  subscription_json.Set(kSubscriptionIdTypeKey,
                        SubscriptionIdTypeToString(subscription.id_type));
  subscription_json.Set(kSubscriptionIdKey, subscription.id);
  subscription_json.Set(
      kSubscriptionManagementTypeKey,
      SubscriptionManagementTypeToString(subscription.management_type));
  if (auto seen_offer = subscription.user_seen_offer) {
    base::Value::Dict seen_offer_json;
    seen_offer_json.Set(kSeenOfferIdKey, seen_offer->offer_id);
    seen_offer_json.Set(kSeenOfferPriceKey,
                        base::NumberToString(seen_offer->user_seen_price));
    seen_offer_json.Set(kSeenOfferCountryKey, seen_offer->country_code);
    if (IsPriceTrackingLocaleKeyEnabled()) {
      seen_offer_json.Set(kSeenOfferLocaleKey, seen_offer->locale);
    }
    subscription_json.Set(kSubscriptionSeenOfferKey,
                          std::move(seen_offer_json));
  }
  return subscription_json;
}

std::optional<CommerceSubscription> SubscriptionsServerProxy::Deserialize(
    const base::Value& value) {
  if (value.is_dict()) {
    const base::Value::Dict& value_dict = value.GetDict();
    auto* type = value_dict.FindString(kSubscriptionTypeKey);
    auto* id_type = value_dict.FindString(kSubscriptionIdTypeKey);
    auto* id = value_dict.FindString(kSubscriptionIdKey);
    auto* management_type =
        value_dict.FindString(kSubscriptionManagementTypeKey);
    auto timestamp =
        base::ValueToInt64(value_dict.Find(kSubscriptionTimestampKey));
    if (type && id_type && id && management_type && timestamp) {
      return std::make_optional<CommerceSubscription>(
          StringToSubscriptionType(*type), StringToSubscriptionIdType(*id_type),
          *id, StringToSubscriptionManagementType(*management_type),
          *timestamp);
    }
  }

  VLOG(1) << "Subscription in response is not valid";
  return std::nullopt;
}

}  // namespace commerce
