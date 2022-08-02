// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_server_proxy.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// For creating endpoint fetcher.
const char kOAuthScope[] = "https://www.googleapis.com/auth/chromememex";
const char kOAuthName[] = "subscriptions_svc";
const char kGetHttpMethod[] = "GET";
const char kPostHttpMethod[] = "POST";
const char kContentType[] = "application/json; charset=UTF-8";
const char kEmptyPostData[] = "";
const int64_t kTimeoutMs = 1000;

const char kDefaultServiceBaseUrl[] =
    "https://memex-pa.googleapis.com/v1/shopping/subscriptions";
const char kBaseUrlParam[] = "subscriptions_service_base_url";
constexpr base::FeatureParam<std::string> kServiceBaseUrl{
    &commerce::kCommercePriceTracking, kBaseUrlParam, kDefaultServiceBaseUrl};

const char kGetQueryParams[] = "?requestParams.subscriptionType=";
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
  if (subscriptions->size() == 0) {
    std::move(callback).Run(true);
    return;
  }

  base::Value subscriptions_list(base::Value::Type::LIST);
  for (const auto& subscription : *subscriptions) {
    subscriptions_list.Append(Serialize(subscription));
  }
  base::Value subscriptions_json(base::Value::Type::DICTIONARY);
  subscriptions_json.SetKey(kSubscriptionsKey, std::move(subscriptions_list));
  base::Value request_json(base::Value::Type::DICTIONARY);
  request_json.SetKey(kCreateRequestParamsKey, std::move(subscriptions_json));
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

  auto fetcher =
      CreateEndpointFetcher(GURL(kServiceBaseUrl.Get()), kPostHttpMethod,
                            post_data, traffic_annotation);
  auto* const fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(
      &SubscriptionsServerProxy::HandleManageSubscriptionsResponses,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SubscriptionsServerProxy::Delete(
    std::unique_ptr<std::vector<CommerceSubscription>> subscriptions,
    ManageSubscriptionsFetcherCallback callback) {
  if (subscriptions->size() == 0) {
    std::move(callback).Run(true);
    return;
  }

  base::Value deletions_list(base::Value::Type::LIST);
  for (const auto& subscription : *subscriptions) {
    if (subscription.timestamp != kUnknownSubscriptionTimestamp)
      deletions_list.Append(base::Int64ToValue(subscription.timestamp));
  }
  base::Value deletions_json(base::Value::Type::DICTIONARY);
  deletions_json.SetKey(kEventTimestampsKey, std::move(deletions_list));
  base::Value request_json(base::Value::Type::DICTIONARY);
  request_json.SetKey(kDeleteRequestParamsKey, std::move(deletions_json));
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

  auto fetcher =
      CreateEndpointFetcher(GURL(kServiceBaseUrl.Get()), kPostHttpMethod,
                            post_data, traffic_annotation);
  auto* const fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(
      &SubscriptionsServerProxy::HandleManageSubscriptionsResponses,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SubscriptionsServerProxy::Get(SubscriptionType type,
                                   GetSubscriptionsFetcherCallback callback) {
  std::string service_url = kServiceBaseUrl.Get() + kGetQueryParams;
  if (type == SubscriptionType::kPriceTrack) {
    service_url += kPriceTrackGetParam;
  } else {
    VLOG(1) << "Unsupported type for Get query";
    std::move(callback).Run(
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
  fetcher_ptr->Fetch(
      base::BindOnce(&SubscriptionsServerProxy::HandleGetSubscriptionsResponses,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

std::unique_ptr<EndpointFetcher>
SubscriptionsServerProxy::CreateEndpointFetcher(
    const GURL& url,
    const std::string& http_method,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  return std::make_unique<EndpointFetcher>(
      url_loader_factory_, kOAuthName, url, http_method, kContentType,
      std::vector<std::string>{kOAuthScope}, kTimeoutMs, post_data,
      annotation_tag, identity_manager_);
}

void SubscriptionsServerProxy::HandleManageSubscriptionsResponses(
    ManageSubscriptionsFetcherCallback callback,
    std::unique_ptr<EndpointResponse> responses) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      responses->response,
      base::BindOnce(&SubscriptionsServerProxy::OnManageSubscriptionsJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SubscriptionsServerProxy::OnManageSubscriptionsJsonParsed(
    ManageSubscriptionsFetcherCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (result.has_value() && result->is_dict()) {
    if (auto* status_value = result->FindKey(kStatusKey)) {
      if (auto status_code = status_value->FindIntKey(kStatusCodeKey)) {
        std::move(callback).Run(*status_code == kBackendCanonicalCodeSuccess);
        return;
      }
    }
  }

  // TODO(crbug.com/1348024): Record metrics for failed parse.
  VLOG(1) << "Fail to get status code from response";
  std::move(callback).Run(false);
}

void SubscriptionsServerProxy::HandleGetSubscriptionsResponses(
    GetSubscriptionsFetcherCallback callback,
    std::unique_ptr<EndpointResponse> responses) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      responses->response,
      base::BindOnce(&SubscriptionsServerProxy::OnGetSubscriptionsJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SubscriptionsServerProxy::OnGetSubscriptionsJsonParsed(
    GetSubscriptionsFetcherCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  auto subscriptions = std::make_unique<std::vector<CommerceSubscription>>();
  if (result.has_value() && result->is_dict()) {
    // TODO(crbug.com/1346107): Check whether the request succeeds. If not, we
    // may have to fetch again.
    if (auto* subscriptions_json = result->FindListKey(kSubscriptionsKey)) {
      for (const auto& subscription_json : subscriptions_json->GetList()) {
        if (auto subscription = Deserialize(subscription_json))
          subscriptions->push_back(*subscription);
      }
      std::move(callback).Run(std::move(subscriptions));
      return;
    }
  }

  VLOG(1) << "Fail to get subscriptions from response";
  std::move(callback).Run(std::move(subscriptions));
}

base::Value SubscriptionsServerProxy::Serialize(
    const CommerceSubscription& subscription) {
  base::Value subscription_json(base::Value::Type::DICTIONARY);
  subscription_json.SetIntKey(kSubscriptionTypeKey,
                              static_cast<int>(subscription.type));
  subscription_json.SetIntKey(kSubscriptionIdTypeKey,
                              static_cast<int>(subscription.id_type));
  subscription_json.SetStringKey(kSubscriptionIdKey, subscription.id);
  subscription_json.SetIntKey(kSubscriptionManagementTypeKey,
                              static_cast<int>(subscription.management_type));
  if (auto seen_offer = subscription.user_seen_offer) {
    base::Value seen_offer_json(base::Value::Type::DICTIONARY);
    seen_offer_json.SetStringKey(kSeenOfferIdKey, seen_offer->offer_id);
    seen_offer_json.SetStringKey(
        kSeenOfferPriceKey, base::NumberToString(seen_offer->user_seen_price));
    seen_offer_json.SetStringKey(kSeenOfferCountryKey,
                                 seen_offer->country_code);
    subscription_json.SetKey(kSubscriptionSeenOfferKey,
                             std::move(seen_offer_json));
  }
  return subscription_json;
}

absl::optional<CommerceSubscription> SubscriptionsServerProxy::Deserialize(
    const base::Value& value) {
  if (value.is_dict()) {
    auto type = value.FindIntKey(kSubscriptionTypeKey);
    auto id_type = value.FindIntKey(kSubscriptionIdTypeKey);
    auto* id = value.FindStringKey(kSubscriptionIdKey);
    auto management_type = value.FindIntKey(kSubscriptionManagementTypeKey);
    auto timestamp =
        base::ValueToInt64(value.FindKey(kSubscriptionTimestampKey));
    if (type && id_type && id && management_type && timestamp) {
      return absl::make_optional<CommerceSubscription>(
          SubscriptionType(*type), IdentifierType(*id_type), *id,
          ManagementType(*management_type), *timestamp);
    }
  }

  VLOG(1) << "Subscription in response is not valid";
  return absl::nullopt;
}

}  // namespace commerce
