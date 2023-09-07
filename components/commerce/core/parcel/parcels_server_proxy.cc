// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/parcel/parcels_server_proxy.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/lazy_instance.h"
#include "base/task/single_thread_task_runner.h"
#include "components/commerce/core/account_checker.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace commerce {

namespace {
// For creating endpoint fetcher.
const int kDefaultTimeoutMs = 5000;
const char kTimeoutParam[] = "parcel_server_request_timeout";
constexpr base::FeatureParam<int> kTimeoutMs{&kParcelTracking, kTimeoutParam,
                                             kDefaultTimeoutMs};

// URL to get parcel status.
const char kDefaultServiceBaseUrl[] =
    "https://memex-pa.googleapis.com/v1/shopping/parcels";
const char kBaseUrlParam[] = "parcel_service_base_url";
constexpr base::FeatureParam<std::string> kServiceBaseUrl{
    &commerce::kParcelTracking, kBaseUrlParam, kDefaultServiceBaseUrl};
const char kGetStatusPath[] = ":status";

// lowerCamelCase JSON proto message keys.
const char kTrackingIdKey[] = "trackingId";
const char kCarrierKey[] = "carrier";
const char kGetStatusRequestParamKey[] = "parcelIds";
const char kParcelStatusKey[] = "parcelStatus";
const char kParcelIdentifierKey[] = "parcelIdentifier";
const char kParcelStateKey[] = "parcelState";
const char kTrackingUrlKey[] = "trackingUrl";
const char kEstimatedDeliveryTimeUsecKey[] = "estimatedDeliveryTimeUsec";

constexpr net::NetworkTrafficAnnotationTag kGetParcelStatusTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("chrome_commerce_parcels_status", R"(
      semantics {
        sender: "Chrome Shopping"
        description:
          "Fetches parcel status from the server for parcel tracking."
        trigger:
          "A Chrome-initiated request after user has opted in to the "
          "parcel tracking feature. The request will be sent every a "
          "few hours to refresh UI with the latest parcel status, until "
          "the parcel is delivered or ends with an ending state, or user "
          "turns the tracking off for a parcel. The actual time interval "
          "between the requests is determined by the delivery status and "
          "ranges between half an hour to 12 hours."
        user_data {
          type: OTHER
        }
        data:
          "Parcel carrier and tracking ID is included in the request. "
          "No user information is sent."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts { email: "chrome-shopping-eng@google.com" }
        }
        last_reviewed: "2023-09-05"
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature is only avaliable for signed-in users. Users can "
          "enable or disable parcel tracking through settings."
        chrome_policy {
          BrowserSignin {
            policy_options {mode: MANDATORY}
            BrowserSignin: 0
          }
        }
      }
    )");

// Serializes a list of ParcelIdentifier .
base::Value::List Serialize(
    const std::vector<ParcelIdentifier>& parcel_identifiers) {
  base::Value::List parcel_list;
  for (const auto& identifier : parcel_identifiers) {
    if (identifier.tracking_id().empty() ||
        identifier.carrier() == ParcelIdentifier::UNKNOWN) {
      continue;
    }
    base::Value::Dict dict;
    dict.Set(kTrackingIdKey, identifier.tracking_id());
    dict.Set(kCarrierKey, identifier.carrier());
    parcel_list.Append(std::move(dict));
  }
  return parcel_list;
}

// Deserializes a ParcelStatus from JSON.
absl::optional<ParcelStatus> Deserialize(const base::Value& value) {
  if (!value.is_dict()) {
    return absl::nullopt;
  }

  const base::Value::Dict& value_dict = value.GetDict();
  auto* parcel_identifier = value_dict.FindDict(kParcelIdentifierKey);
  if (!parcel_identifier) {
    return absl::nullopt;
  }

  auto* tracking_id = parcel_identifier->FindString(kTrackingIdKey);
  auto carrier = parcel_identifier->FindInt(kCarrierKey);
  auto parcel_state = value_dict.FindInt(kParcelStateKey);
  if (!tracking_id || !carrier || !parcel_state) {
    return absl::nullopt;
  }

  auto* tracking_url = value_dict.FindString(kTrackingUrlKey);
  auto estimated_delivery_time_usec =
      base::ValueToInt64(value_dict.Find(kEstimatedDeliveryTimeUsecKey));
  ParcelStatus status;
  auto* identifier = status.mutable_parcel_identifier();
  identifier->set_tracking_id(*tracking_id);
  identifier->set_carrier(
      static_cast<ParcelIdentifier::Carrier>(carrier.value()));
  status.set_parcel_state(
      static_cast<ParcelStatus::ParcelState>(parcel_state.value()));
  if (tracking_url) {
    status.set_tracking_url(*tracking_url);
  }
  if (estimated_delivery_time_usec) {
    status.set_estimated_delivery_time_usec(
        estimated_delivery_time_usec.value());
  }
  return absl::make_optional<ParcelStatus>(status);
}

}  // namespace

ParcelsServerProxy::ParcelsServerProxy(
    signin::IdentityManager* identity_manager,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager) {}

ParcelsServerProxy::~ParcelsServerProxy() = default;

void ParcelsServerProxy::GetParcelStatus(
    const std::vector<ParcelIdentifier>& parcel_identifiers,
    GetParcelStatusCallback callback) {
  CHECK_GT(parcel_identifiers.size(), 0u);

  base::Value::List parcel_list = Serialize(parcel_identifiers);
  if (parcel_list.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       ParcelStatusRequestStatus::kInvalidParcelIdentifiers,
                       std::make_unique<std::vector<ParcelStatus>>()));
    return;
  }
  base::Value::Dict request_json;
  request_json.Set(kGetStatusRequestParamKey, std::move(parcel_list));
  std::string post_data;
  base::JSONWriter::Write(request_json, &post_data);
  auto fetcher = CreateEndpointFetcher(
      GURL(kServiceBaseUrl.Get() + kGetStatusPath), post_data);

  auto* const fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(
      &ParcelsServerProxy::ProcessGetParcelStatusResponse,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), std::move(fetcher)));
}

std::unique_ptr<EndpointFetcher> ParcelsServerProxy::CreateEndpointFetcher(
    const GURL& url,
    const std::string& post_data) {
  return std::make_unique<EndpointFetcher>(
      url_loader_factory_, kOAuthName, url, kPostHttpMethod, kContentType,
      std::vector<std::string>{kOAuthScope}, kTimeoutMs.Get(), post_data,
      kGetParcelStatusTrafficAnnotation, identity_manager_,
      signin::ConsentLevel::kSync);
}

void ParcelsServerProxy::ProcessGetParcelStatusResponse(
    GetParcelStatusCallback callback,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> responses) {
  if (responses->http_status_code != net::HTTP_OK || responses->error_type) {
    std::move(callback).Run(ParcelStatusRequestStatus::kServerError,
                            std::make_unique<std::vector<ParcelStatus>>());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      responses->response,
      base::BindOnce(&ParcelsServerProxy::OnGetParcelStatusJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ParcelsServerProxy::OnGetParcelStatusJsonParsed(
    GetParcelStatusCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  auto parcel_status = std::make_unique<std::vector<ParcelStatus>>();
  if (result.has_value() && result->is_dict()) {
    if (auto* response_json = result->GetDict().FindList(kParcelStatusKey)) {
      for (const auto& status_json : *response_json) {
        if (auto status = Deserialize(status_json)) {
          parcel_status->push_back(*status);
        }
      }
      std::move(callback).Run(ParcelStatusRequestStatus::kSuccess,
                              std::move(parcel_status));
      return;
    }
  }
  std::move(callback).Run(ParcelStatusRequestStatus::kServerReponseParsingError,
                          std::make_unique<std::vector<ParcelStatus>>());
}

}  // namespace commerce
