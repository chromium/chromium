// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/websocket_client.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/private_ai/attestation/server_verification_key.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/proto_utils/google_rpc_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"
#include "url/gurl.h"

namespace private_ai {
namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("private_ai_client", R"(
        semantics {
          sender: "PrivateAI Client"
          description:
            "This traffic creates an encrypted session with the "
            "PrivateAI service and carries the request and response over that "
            "session. "
            "The feature is under development and behind a feature flag."
          trigger:
            "A feature that uses the PrivateAI component is triggered. "
            "The feature determines which data to send."
          user_data {
            type: PROFILE_DATA
          }
          data: "This contains an encrypted request."
          internal {
            contacts {
                email: "dullweber@chromium.org"
            }
          }
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2025-09-12"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Still in development. Setting may be added later."
          policy_exception_justification:
            "Still in development. Policy may be added later."
        })");

}  // namespace

WebSocketClient::WebSocketClient(
    const GURL& service_url,
    network::mojom::NetworkContext* network_context,
    PrivateAiLogger* logger)
    : logger_(logger),
      streaming_client_(service_url,
                        network_context,
                        kTrafficAnnotation,
                        /*delegate=*/this) {
  CHECK(logger_);
}

WebSocketClient::~WebSocketClient() = default;

void WebSocketClient::SetResponseCallback(ResponseCallback callback) {
  CHECK_EQ(state_, State::kInitialized);
  response_callback_ = std::move(callback);
}

void WebSocketClient::Send(const oak::session::v1::SessionRequest& request) {
  CHECK(response_callback_);
  std::string binary_proto;
  if (!request.SerializeToString(&binary_proto)) {
    logger_->LogError(FROM_HERE,
                      "Failed to serialize proto request into a string. Check "
                      "all required fields are set");
    response_callback_.Run(
        base::unexpected(TransportError::kSerializationError));
    return;
  }

  Send(Request(binary_proto.begin(), binary_proto.end()));
}

void WebSocketClient::Send(Request request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kDisconnected) {
    ClosePipe(TransportError::kError);
    return;
  }

  if (state_ == State::kInitialized) {
    state_ = State::kActive;
  }

  streaming_client_.Send(std::move(request));
}

void WebSocketClient::OnResponse(
    base::expected<std::vector<uint8_t>, TransportError> response) {
  if (!response_callback_) {
    return;
  }

  if (!response.has_value()) {
    response_callback_.Run(base::unexpected(response.error()));
    return;
  }

  std::string response_str = std::string(response->begin(), response->end());
  oak::session::v1::SessionResponse session_response;
  if (!session_response.ParseFromString(response_str)) {
    response_callback_.Run(
        base::unexpected(TransportError::kDeserializationError));
    return;
  }

  response_callback_.Run(base::ok(std::move(session_response)));
}

void WebSocketClient::OnMessage(std::vector<uint8_t> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Call OnResponse asynchronously since this object may be destroyed during
  // the callback.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WebSocketClient::OnResponse,
                                weak_ptr_factory_.GetWeakPtr(),
                                base::ok(std::move(message))));
}

void WebSocketClient::OnConnectionError(const std::string& message,
                                        int net_error,
                                        int response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->LogError(
      FROM_HERE, base::StrCat({"PrivateAI service connection failed ", message,
                               " (net error:", base::NumberToString(net_error),
                               ", response code:",
                               base::NumberToString(response_code), ")"}));

  ClosePipe(TransportError::kError);
}

void WebSocketClient::OnDropChannel(bool was_clean,
                                    uint16_t code,
                                    const std::string& reason,
                                    std::optional<base::TimeDelta> elapsed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->LogError(FROM_HERE, base::StrCat({"Websocket Channel dropped (code:",
                                             base::NumberToString(code),
                                             ", reason:", reason, ")"}));

  base::UmaHistogramSparse("PrivateAi.Client.WebSocketCloseCode", code);

  if (elapsed.has_value()) {
    base::UmaHistogramLongTimes(
        "PrivateAi.Client.WebSocketSessionDuration.ClosedByServer", *elapsed);
  }

  // If there is a reason, it indicates an error from the server.
  if (!reason.empty()) {
    base::UmaHistogramEnumeration(
        "PrivateAi.Client.ServerStatusCode", ParseGoogleRpcCode(reason),
        static_cast<rpc::GoogleRpcCode>(rpc::GoogleRpcCode_MAX + 1));
  }
  ClosePipe(TransportError::kSocketClosed);
}

void WebSocketClient::OnError(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!message.empty()) {
    logger_->LogError(FROM_HERE, message);
  }
  ClosePipe(TransportError::kError);
}

void WebSocketClient::OnClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClosePipe(TransportError::kSocketClosed);
}

std::vector<network::mojom::HttpHeaderPtr>
WebSocketClient::GetAdditionalHeaders() {
  std::vector<network::mojom::HttpHeaderPtr> additional_headers;
  if (IsNonProdServerVerificationKey(streaming_client_.service_url())) {
    additional_headers.push_back(network::mojom::HttpHeader::New(
        "X-Client-Verification-Key-Variant", "nonprod"));
  }
  return additional_headers;
}

void WebSocketClient::ClosePipe(TransportError status) {
  if (state_ == State::kDisconnected) {
    return;
  }
  state_ = State::kDisconnected;
  streaming_client_.Close();

  // Call OnResponse asynchronously since this object may be destroyed during
  // the callback.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebSocketClient::OnResponse,
                     weak_ptr_factory_.GetWeakPtr(), base::unexpected(status)));
}

}  // namespace private_ai
