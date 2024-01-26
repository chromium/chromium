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
#include "base/time/time.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
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
const char kUntrackPath[] = ":untrack";

// lowerCamelCase JSON proto message keys.
const char kTrackingIdKey[] = "trackingId";
const char kCarrierKey[] = "carrier";
const char kParcelIdsKey[] = "parcelIds";
const char kParcelStatusKey[] = "parcelStatus";
const char kParcelIdentifierKey[] = "parcelIdentifier";
const char kParcelStateKey[] = "parcelState";
const char kTrackingUrlKey[] = "trackingUrl";
const char kEstimatedDeliveryDateKey[] = "estimatedDeliveryDate";
const char kSourcePageDomainKey[] = "sourcePageDomain";

constexpr net::NetworkTrafficAnnotationTag
    kStartTrackingParcelTrafficAnnotation = net::DefineNetworkTrafficAnnotation(
        "chrome_commerce_start_tracking_parcel",
        R"(
          semantics {
            sender: "Chrome Shopping"
            description:
              "Inform the server to start tracking the status for some parcels."
            trigger:
              "A Chrome-initiated request that requires user enabling the "
              "parcel tracking feature. The request is sent after Chrome "
              "detects a page has carrier and parcels information, and user "
              "choose to enable tracking for those parcels."
            user_data {
              type: OTHER
              type: ACCESS_TOKEN
            }
            data:
              "Carrier, tracking ID and the page domain that Chrome detects "
              "the tracking information is included in the request."
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts { email: "chrome-shopping-eng@google.com" }
            }
            last_reviewed: "2023-09-07"
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

constexpr net::NetworkTrafficAnnotationTag
    kStopTrackingParcelTrafficAnnotation = net::DefineNetworkTrafficAnnotation(
        "chrome_commerce_stop_tracking_parcel",
        R"(
          semantics {
            sender: "Chrome Shopping"
            description:
              "Inform the server to stop tracking the status for some parcels."
            trigger:
              "A Chrome-initiated http DELETE request that requires user "
              "enabling the parcel tracking feature. The request is sent after "
              "user choose to disabling tracking for some parcels."
            user_data {
              type: OTHER
              type: ACCESS_TOKEN
            }
            data:
              "Tracking ID is included in the request."
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts { email: "chrome-shopping-eng@google.com" }
            }
            last_reviewed: "2023-09-07"
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

constexpr net::NetworkTrafficAnnotationTag
    kStopTrackingAllParcelTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation(
            "chrome_commerce_stop_tracking_all_parcel",
            R"(
              semantics {
                sender: "Chrome Shopping"
                description:
                  "Inform the server to stop tracking the status for all "
                  "parcels."
                trigger:
                  "A Chrome-initiated http DELETE request that is sent when "
                  "user disables the parcel tracking feature."
                user_data {
                  type: NONE
                  type: ACCESS_TOKEN
                }
                data:
                  "No data is sent in the request."
                destination: GOOGLE_OWNED_SERVICE
                internal {
                  contacts { email: "chrome-shopping-eng@google.com" }
                }
                last_reviewed: "2023-09-07"
              }
              policy {
                cookies_allowed: NO
                setting:
                  "This feature is only avaliable for signed-in users. Users "
                  "can enable or disable parcel tracking through settings."
                chrome_policy {
                  BrowserSignin {
                    policy_options {mode: MANDATORY}
                    BrowserSignin: 0
                  }
                }
              }
            )");

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
          type: ACCESS_TOKEN
        }
        data:
          "Parcel carrier and tracking ID is included in the request. "
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
std::optional<ParcelStatus> Deserialize(const base::Value& value) {
  if (!value.is_dict()) {
    return std::nullopt;
  }

  const base::Value::Dict& value_dict = value.GetDict();
  auto* parcel_identifier = value_dict.FindDict(kParcelIdentifierKey);
  if (!parcel_identifier) {
    return std::nullopt;
  }

  auto* tracking_id = parcel_identifier->FindString(kTrackingIdKey);
  auto* carrier_str = parcel_identifier->FindString(kCarrierKey);
  auto* parcel_state_str = value_dict.FindString(kParcelStateKey);
  if (!tracking_id || !carrier_str || !parcel_state_str) {
    return std::nullopt;
  }

  ParcelIdentifier::Carrier carrier;
  ParcelStatus::ParcelState parcel_state;
  if (!ParcelIdentifier::Carrier_Parse(*carrier_str, &carrier) ||
      !ParcelStatus::ParcelState_Parse(*parcel_state_str, &parcel_state)) {
    return std::nullopt;
  }

  auto* tracking_url = value_dict.FindString(kTrackingUrlKey);
  auto* estimated_delivery_date =
      value_dict.FindString(kEstimatedDeliveryDateKey);
  ParcelStatus status;
  auto* identifier = status.mutable_parcel_identifier();
  identifier->set_tracking_id(*tracking_id);
  identifier->set_carrier(carrier);
  status.set_parcel_state(parcel_state);
  if (tracking_url) {
    status.set_tracking_url(*tracking_url);
  }
  if (estimated_delivery_date) {
    // Serialize the string to base::Time so that it is easier for UI to format
    // localized string.
    base::Time time_to_deliver;
    if (base::Time::FromString(estimated_delivery_date->c_str(),
                               &time_to_deliver)) {
      status.set_estimated_delivery_time_usec(
          time_to_deliver.ToDeltaSinceWindowsEpoch().InMicroseconds());
    }
  }
  return std::make_optional<ParcelStatus>(status);
}

void OnInvalidParcelIdentifierError(
    ParcelsServerProxy::GetParcelStatusCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false,
                                std::make_unique<std::vector<ParcelStatus>>()));
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
    RecordParcelsRequestMetrics(ParcelRequestType::kGetParcelStatus,
                                ParcelRequestStatus::kInvalidParcelIdentifiers);
    OnInvalidParcelIdentifierError(std::move(callback));
    return;
  }

  base::Value::Dict request_json;
  request_json.Set(kParcelIdsKey, std::move(parcel_list));
  SendJsonRequestToServer(
      std::move(request_json), GURL(kServiceBaseUrl.Get() + kGetStatusPath),
      kGetParcelStatusTrafficAnnotation,
      base::BindOnce(&ParcelsServerProxy::ProcessGetParcelStatusResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     ParcelRequestType::kGetParcelStatus, std::move(callback)));
}

void ParcelsServerProxy::StartTrackingParcels(
    const std::vector<ParcelIdentifier>& parcel_identifiers,
    const std::string& source_page_domain,
    GetParcelStatusCallback callback) {
  CHECK_GT(parcel_identifiers.size(), 0u);
  base::Value::List parcel_list = Serialize(parcel_identifiers);
  if (parcel_list.empty()) {
    RecordParcelsRequestMetrics(ParcelRequestType::kStartTrackingParcels,
                                ParcelRequestStatus::kInvalidParcelIdentifiers);
    OnInvalidParcelIdentifierError(std::move(callback));
    return;
  }
  base::Value::Dict request_json;
  request_json.Set(kParcelIdsKey, std::move(parcel_list));
  request_json.Set(kSourcePageDomainKey, source_page_domain);

  SendJsonRequestToServer(
      std::move(request_json), GURL(kServiceBaseUrl.Get()),
      kStartTrackingParcelTrafficAnnotation,
      base::BindOnce(&ParcelsServerProxy::ProcessGetParcelStatusResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     ParcelRequestType::kStartTrackingParcels,
                     std::move(callback)));
}

void ParcelsServerProxy::StopTrackingParcel(
    const std::string& tracking_id,
    StopParcelTrackingCallback callback) {
  // This call is deprecated, using kUnknown as the request type.
  SendStopTrackingRequestToServer(
      ParcelRequestType::kUnknown,
      GURL(kServiceBaseUrl.Get() + "/" + tracking_id),
      kStopTrackingParcelTrafficAnnotation, std::move(callback));
}

void ParcelsServerProxy::StopTrackingParcels(
    const std::vector<ParcelIdentifier>& parcel_identifiers,
    StopParcelTrackingCallback callback) {
  CHECK_GT(parcel_identifiers.size(), 0u);
  base::Value::List parcel_list = Serialize(parcel_identifiers);
  if (parcel_list.empty()) {
    RecordParcelsRequestMetrics(ParcelRequestType::kStopTrackingParcels,
                                ParcelRequestStatus::kInvalidParcelIdentifiers);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  base::Value::Dict request_json;
  request_json.Set(kParcelIdsKey, std::move(parcel_list));

  SendJsonRequestToServer(
      std::move(request_json), GURL(kServiceBaseUrl.Get() + kUntrackPath),
      kStopTrackingParcelTrafficAnnotation,
      base::BindOnce(&ParcelsServerProxy::OnStopTrackingResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     ParcelRequestType::kStopTrackingParcels,
                     std::move(callback)));
}

void ParcelsServerProxy::StopTrackingAllParcels(
    StopParcelTrackingCallback callback) {
  SendStopTrackingRequestToServer(
      ParcelRequestType::kStopTrackingAllParcels, GURL(kServiceBaseUrl.Get()),
      kStopTrackingAllParcelTrafficAnnotation, std::move(callback));
}

std::unique_ptr<EndpointFetcher> ParcelsServerProxy::CreateEndpointFetcher(
    const GURL& url,
    const std::string& http_method,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag traffic_annotation) {
  return std::make_unique<EndpointFetcher>(
      url_loader_factory_, kOAuthName, url, http_method, kContentType,
      std::vector<std::string>{kOAuthScope},
      base::Milliseconds(kTimeoutMs.Get()), post_data, traffic_annotation,
      identity_manager_, signin::ConsentLevel::kSignin);
}

void ParcelsServerProxy::ProcessGetParcelStatusResponse(
    ParcelRequestType request_type,
    GetParcelStatusCallback callback,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> response) {
  if (response->http_status_code != net::HTTP_OK || response->error_type) {
    RecordParcelsRequestMetrics(request_type,
                                ParcelRequestStatus::kServerError);
    std::move(callback).Run(false,
                            std::make_unique<std::vector<ParcelStatus>>());
    return;
  }
  data_decoder::DataDecoder::ParseJsonIsolated(
      response->response,
      base::BindOnce(&ParcelsServerProxy::OnGetParcelStatusJsonParsed,
                     weak_ptr_factory_.GetWeakPtr(), request_type,
                     std::move(callback)));
}

void ParcelsServerProxy::OnGetParcelStatusJsonParsed(
    ParcelRequestType request_type,
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
      RecordParcelsRequestMetrics(request_type, ParcelRequestStatus::kSuccess);
      std::move(callback).Run(true, std::move(parcel_status));
      return;
    }
  }
  RecordParcelsRequestMetrics(request_type,
                              ParcelRequestStatus::kServerReponseParsingError);
  std::move(callback).Run(false, std::make_unique<std::vector<ParcelStatus>>());
}

void ParcelsServerProxy::OnStopTrackingResponse(
    ParcelRequestType request_type,
    StopParcelTrackingCallback callback,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> response) {
  if (response->http_status_code != net::HTTP_OK || response->error_type) {
    RecordParcelsRequestMetrics(request_type,
                                ParcelRequestStatus::kServerError);
    std::move(callback).Run(false);
  } else {
    RecordParcelsRequestMetrics(request_type, ParcelRequestStatus::kSuccess);
    std::move(callback).Run(true);
  }
}

void ParcelsServerProxy::OnServerResponse(
    EndpointCallback callback,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> response) {
  std::move(callback).Run(std::move(endpoint_fetcher), std::move(response));
}

void ParcelsServerProxy::SendJsonRequestToServer(
    base::Value::Dict request_json,
    const GURL& server_url,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    EndpointCallback callback) {
  std::string post_data;
  base::JSONWriter::Write(request_json, &post_data);
  auto fetcher = CreateEndpointFetcher(GURL(server_url), kPostHttpMethod,
                                       post_data, network_traffic_annotation);
  auto* const fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(&ParcelsServerProxy::OnServerResponse,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback), std::move(fetcher)));
}

void ParcelsServerProxy::SendStopTrackingRequestToServer(
    ParcelRequestType request_type,
    const GURL& server_url,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    StopParcelTrackingCallback callback) {
  auto fetcher =
      CreateEndpointFetcher(server_url, kDeleteHttpMethod, kEmptyPostData,
                            network_traffic_annotation);

  auto* const fetcher_ptr = fetcher.get();
  fetcher_ptr->Fetch(base::BindOnce(&ParcelsServerProxy::OnStopTrackingResponse,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    request_type, std::move(callback),
                                    std::move(fetcher)));
}

}  // namespace commerce
