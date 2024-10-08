// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/manta_service_callbacks.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/proto/rpc_status.pb.h"
#include "net/http/http_status_code.h"

namespace manta {

namespace {
constexpr char kTypeUrlRpcErrorInfo[] =
    "type.googleapis.com/google.rpc.ErrorInfo";
constexpr char kTypeUrlRpcLocalizedMessage[] =
    "type.googleapis.com/google.rpc.LocalizedMessage";
constexpr char kExpectedEndPointDomain[] = "aratea-pa.googleapis.com";

// Maps the RpcErrorInfo.reason to MantaStatusCode.
std::optional<MantaStatusCode> MapServerFailureReasonToMantaStatusCode(
    const std::string& reason) {
  static constexpr auto reason_map =
      base::MakeFixedFlatMap<std::string_view, MantaStatusCode>({
          {"MISSING_INPUT", MantaStatusCode::kInvalidInput},
          {"INVALID_INPUT", MantaStatusCode::kInvalidInput},
          {"UNSUPPORTED_LANGUAGE", MantaStatusCode::kUnsupportedLanguage},
          {"RESTRICTED_COUNTRY", MantaStatusCode::kRestrictedCountry},
          {"RESOURCE_EXHAUSTED", MantaStatusCode::kResourceExhausted},
          {"PER_USER_QUOTA_EXCEEDED", MantaStatusCode::kPerUserQuotaExceeded},
      });
  const auto iter = reason_map.find(reason);

  return iter != reason_map.end() ? std::optional<MantaStatusCode>(iter->second)
                                  : std::nullopt;
}

// Maps the RpcStatus.code to MantaStatusCode.
// The RpcStatus.code is an enum value of google.rpc.Code.
std::optional<MantaStatusCode> MapServerStatusCodeToMantaStatusCode(
    const int32_t server_status_code) {
  // TODO(b/288019728): add more items when needed.
  static constexpr auto code_map =
      base::MakeFixedFlatMap<int32_t, MantaStatusCode>({
          {3 /*INVALID_ARGUMENT*/, MantaStatusCode::kInvalidInput},
          {8 /*RESOURCE_EXHAUSTED*/, MantaStatusCode::kResourceExhausted},
      });
  const auto iter = code_map.find(server_status_code);

  return iter != code_map.end() ? std::optional<MantaStatusCode>(iter->second)
                                : std::nullopt;
}

void LogTimeCost(const MantaMetricType request_type,
                 const base::TimeDelta& time_cost) {
  switch (request_type) {
    case MantaMetricType::kOrca:
      base::UmaHistogramTimes("Ash.MantaService.OrcaProvider.TimeCost",
                              time_cost);
      break;
    case MantaMetricType::kScanner:
      base::UmaHistogramTimes("Ash.MantaService.ScannerProvider.TimeCost",
                              time_cost);
      break;
    case MantaMetricType::kSnapper:
      base::UmaHistogramTimes("Ash.MantaService.SnapperProvider.TimeCost",
                              time_cost);
      break;
    case MantaMetricType::kMahiSummary:
      base::UmaHistogramTimes("Ash.MantaService.MahiProvider.Summary.TimeCost",
                              time_cost);
      break;
    case MantaMetricType::kMahiQA:
      base::UmaHistogramTimes("Ash.MantaService.MahiProvider.QA.TimeCost",
                              time_cost);
      break;
    case manta::MantaMetricType::kSparky:
      base::UmaHistogramTimes("Ash.MantaService.SparkyProvider.TimeCost",
                              time_cost);
      break;
    case MantaMetricType::kAnchovy:
      base::UmaHistogramTimes("Ash.MantaService.AnchovyProvider.TimeCost",
                              time_cost);
      break;
    case MantaMetricType::kWalrus:
      base::UmaHistogramTimes("Ash.MantaService.WalrusProvider.TimeCost",
                              time_cost);
      break;
  }
}

void LogMantaStatusCode(const MantaMetricType request_type,
                        const MantaStatusCode status_code) {
  switch (request_type) {
    case MantaMetricType::kOrca:
      base::UmaHistogramEnumeration("Ash.MantaService.OrcaProvider.StatusCode",
                                    status_code);
      break;
    case MantaMetricType::kScanner:
      base::UmaHistogramEnumeration(
          "Ash.MantaService.ScannerProvider.StatusCode", status_code);
      break;
    case MantaMetricType::kSnapper:
      base::UmaHistogramEnumeration(
          "Ash.MantaService.SnapperProvider.StatusCode", status_code);
      break;
    case MantaMetricType::kMahiSummary:
      base::UmaHistogramEnumeration(
          "Ash.MantaService.MahiProvider.Summary.StatusCode", status_code);
      break;
    case MantaMetricType::kMahiQA:
      base::UmaHistogramEnumeration(
          "Ash.MantaService.MahiProvider.QA.StatusCode", status_code);
      break;
    case manta::MantaMetricType::kSparky:
      base::UmaHistogramEnumeration(
          "Ash.MantaService.SparkyProvider.StatusCode", status_code);
      break;
    case MantaMetricType::kAnchovy:
      base::UmaHistogramEnumeration(
          "Ash.MantaService.AnchovyProvider.StatusCode", status_code);
      break;
    case MantaMetricType::kWalrus:
      base::UmaHistogramEnumeration(
          "Ash.MantaService.WalrusProvider.StatusCode", status_code);
      break;
  }
}
}  // namespace

void OnEndpointFetcherComplete(MantaProtoResponseCallback callback,
                               const base::Time& start_time,
                               const MantaMetricType request_type,
                               std::unique_ptr<EndpointFetcher> fetcher,
                               std::unique_ptr<EndpointResponse> responses) {
  // Tries to parse the response as a Response proto and return to the
  // `callback` together with a OK status, or capture the errors and return a
  // proper error status.

  base::TimeDelta time_cost = base::Time::Now() - start_time;

  std::string message, locale;

  if (!responses) {
    LogMantaStatusCode(request_type, MantaStatusCode::kBackendFailure);
    std::move(callback).Run(nullptr,
                            {MantaStatusCode::kBackendFailure, message});
    return;
  }

  // Tries to parse the unexpected response as RpcStatus and passes back the
  // error messages.
  if (responses->error_type.has_value() ||
      responses->http_status_code != net::HTTP_OK) {
    MantaStatusCode manta_status_code = MantaStatusCode::kBackendFailure;

    if (responses->error_type.has_value() &&
        responses->error_type.value() == FetchErrorType::kNetError) {
      manta_status_code = MantaStatusCode::kNoInternetConnection;
    }

    proto::RpcStatus rpc_status;

    if (!rpc_status.ParseFromString(responses->response)) {
      DVLOG(1) << "Got unexpected failed response but failed to parse a "
                  "RpcStatus proto";
      LogMantaStatusCode(request_type, manta_status_code);
      std::move(callback).Run(nullptr, {manta_status_code, message});
      return;
    }

    // Tries to map RpcStatus.code to a more specific manta status code.
    auto maybe_updated_status_code =
        MapServerStatusCodeToMantaStatusCode(rpc_status.code());
    if (maybe_updated_status_code != std::nullopt) {
      manta_status_code = maybe_updated_status_code.value();
    }

    // Extracts clearer error code and user-friendly messages from details.
    for (const proto::Proto3Any& detail : rpc_status.details()) {
      if (detail.type_url() == kTypeUrlRpcErrorInfo) {
        proto::RpcErrorInfo error_info;
        error_info.ParseFromString(detail.value());

        // Tries to map ErrorInfo.reason to a more specific manta status code.
        maybe_updated_status_code =
            MapServerFailureReasonToMantaStatusCode(error_info.reason());
        if (error_info.domain() == kExpectedEndPointDomain &&
            maybe_updated_status_code != std::nullopt) {
          manta_status_code = maybe_updated_status_code.value();
        }
      } else if (detail.type_url() == kTypeUrlRpcLocalizedMessage) {
        proto::RpcLocalizedMessage localize_message;
        localize_message.ParseFromString(detail.value());
        message = localize_message.message();
        locale = localize_message.locale();
      }
    }

    LogMantaStatusCode(request_type, manta_status_code);
    std::move(callback).Run(nullptr, {manta_status_code, message, locale});

    return;
  }

  auto manta_response = std::make_unique<proto::Response>();
  if (!manta_response->ParseFromString(responses->response)) {
    DVLOG(1) << "Failed to parse MantaResponse as a Response proto";
    LogMantaStatusCode(request_type, MantaStatusCode::kMalformedResponse);
    std::move(callback).Run(nullptr,
                            {MantaStatusCode::kMalformedResponse, message});
    return;
  }

  LogTimeCost(request_type, time_cost);
  LogMantaStatusCode(request_type, MantaStatusCode::kOk);
  std::move(callback).Run(std::move(manta_response),
                          {MantaStatusCode::kOk, message});
}

}  // namespace manta
