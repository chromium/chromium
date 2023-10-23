// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/manta_service_callbacks.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/proto/rpc_status.pb.h"
#include "net/http/http_status_code.h"

namespace manta {

constexpr char kTypeUrlRpcErrorInfo[] =
    "type.googleapis.com/google.rpc.ErrorInfo";
constexpr char kTypeUrlRpcLocalizedMessage[] =
    "type.googleapis.com/google.rpc.LocalizedMessage";

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

    if (!rpc_status.ParseFromString(responses->response)) {
      DVLOG(1) << "Got unexpected failed response but failed to parse a "
                  "RpcStatus proto";
      std::move(callback).Run(nullptr,
                              {MantaStatusCode::kBackendFailure, message});
      return;
    }

    // Extracts clearer error code and user-friendly messages from details.
    for (const proto::Proto3Any& detail : rpc_status.details()) {
      if (detail.type_url() == kTypeUrlRpcErrorInfo) {
        // TODO(b/288019728): map error_info.reason to a proper MantaStatusCode.
        proto::RpcErrorInfo error_info;
        error_info.ParseFromString(detail.value());
        DVLOG(1) << "Parse details to ErrorInfo proto with reasons = "
                 << error_info.reason() << ", domain = " << error_info.domain();
      } else if (detail.type_url() == kTypeUrlRpcLocalizedMessage) {
        proto::RpcLocalizedMessage localize_message;
        localize_message.ParseFromString(detail.value());
        message = localize_message.message();
      }
    }

    std::move(callback).Run(nullptr,
                            {MantaStatusCode::kBackendFailure, message});

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
