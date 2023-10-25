// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/manta_service_callbacks.h"

#include <memory>
#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/callback.h"
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
absl::optional<MantaStatusCode> MapServerFailureReasonToMantaStatusCode(
    const std::string& reason) {
  static constexpr auto reason_map =
      base::MakeFixedFlatMap<base::StringPiece, MantaStatusCode>({
          {"MISSING_INPUT", MantaStatusCode::kInvalidInput},
          {"INVALID_INPUT", MantaStatusCode::kInvalidInput},
          {"UNSUPPORTED_LANGUAGE", MantaStatusCode::kUnsupportedLanguage},
          {"RESTRICTED_COUNTRY", MantaStatusCode::kRestrictedCountry},
          {"RESOURCE_EXHAUSTED", MantaStatusCode::kResourceExhausted},
      });
  const auto* iter = reason_map.find(reason);

  return iter != reason_map.end()
             ? absl::optional<MantaStatusCode>(iter->second)
             : absl::nullopt;
}

// Maps the RpcStatus.code to MantaStatusCode.
// The RpcStatus.code is an enum value of google.rpc.Code.
absl::optional<MantaStatusCode> MapServerStatusCodeToMantaStatusCode(
    const int32_t server_status_code) {
  // TODO(b/288019728): add more items when needed.
  static constexpr auto code_map =
      base::MakeFixedFlatMap<int32_t, MantaStatusCode>({
          {3 /*INVALID_ARGUMENT*/, MantaStatusCode::kInvalidInput},
          {8 /*RESOURCE_EXHAUSTED*/, MantaStatusCode::kResourceExhausted},
      });
  const auto* iter = code_map.find(server_status_code);

  return iter != code_map.end() ? absl::optional<MantaStatusCode>(iter->second)
                                : absl::nullopt;
}
}  // namespace

void OnEndpointFetcherComplete(MantaProtoResponseCallback callback,
                               std::unique_ptr<EndpointFetcher> fetcher,
                               std::unique_ptr<EndpointResponse> responses) {
  // TODO(b/301185733): Log error code to UMA.
  // Tries to parse the response as a Response proto and return to the
  // `callback` together with a OK status, or capture the errors and return a
  // proper error status.

  std::string message = std::string();
  if (!responses || responses->error_type.has_value() ||
      responses->http_status_code != net::HTTP_OK) {
    // Parses the unexpected response as RpcStatus and passes back the error
    // messages.
    proto::RpcStatus rpc_status;

    MantaStatusCode manta_status_code = MantaStatusCode::kBackendFailure;

    if (!rpc_status.ParseFromString(responses->response)) {
      DVLOG(1) << "Got unexpected failed response but failed to parse a "
                  "RpcStatus proto";
      std::move(callback).Run(nullptr, {manta_status_code, message});
      return;
    }

    // Tries to map RpcStatus.code to a more specific manta status code.
    auto maybe_updated_status_code =
        MapServerStatusCodeToMantaStatusCode(rpc_status.code());
    if (maybe_updated_status_code != absl::nullopt) {
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
            maybe_updated_status_code != absl::nullopt) {
          manta_status_code = maybe_updated_status_code.value();
        }
      } else if (detail.type_url() == kTypeUrlRpcLocalizedMessage) {
        proto::RpcLocalizedMessage localize_message;
        localize_message.ParseFromString(detail.value());
        message = localize_message.message();
      }
    }

    std::move(callback).Run(nullptr, {manta_status_code, message});

    return;
  }

  auto manta_response = std::make_unique<proto::Response>();
  if (!manta_response->ParseFromString(responses->response)) {
    DVLOG(1) << "Failed to parse MantaResponse as a Response proto";
    std::move(callback).Run(nullptr,
                            {MantaStatusCode::kMalformedResponse, message});
    return;
  }

  std::move(callback).Run(std::move(manta_response),
                          {MantaStatusCode::kOk, message});
}

}  // namespace manta
