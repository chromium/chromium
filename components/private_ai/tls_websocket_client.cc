// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/tls_websocket_client.h"

#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/proto/private_tls_service.pb.h"
#include "components/private_ai/proto_utils/google_rpc_code.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace private_ai {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("private_ai_tls_client", R"(
        semantics {
          sender: "PrivateAI TLS Client"
          description:
            "This connection creates a TLS 1.3 encrypted session with the "
            "PrivateAI service and carries encrypted requests and responses."
          trigger:
            "A feature that uses the PrivateAI component is triggered."
          user_data {
            type: PROFILE_DATA
          }
          data: "This contains an encrypted request."
          internal {
            contacts {
              email: "georgeseb@google.com"
            }
          }
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2026-09-24"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by users directly, but is only "
            "active when Private AI features (such as ContextualCues for "
            "Gemini) are enabled."
          policy_exception_justification:
            "Essential transport for Private AI."
        })");

}  // namespace

TlsWebSocketClient::TlsWebSocketClient(
    const GURL& service_url,
    network::NetworkContextGetter network_context_getter,
    PrivateAiLogger* logger,
    std::optional<bool> has_chrome_proxy)
    : logger_(logger),
      streaming_client_(service_url,
                        std::move(network_context_getter),
                        kTrafficAnnotation,
                        /*delegate=*/this),
      has_chrome_proxy_(has_chrome_proxy) {
  CHECK(logger_);
}

TlsWebSocketClient::~TlsWebSocketClient() = default;

void TlsWebSocketClient::SetFrameCallback(FrameCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!disconnected_);
  CHECK(!frame_callback_);
  frame_callback_ = std::move(callback);
}

void TlsWebSocketClient::SendFrame(const std::vector<uint8_t>& frame_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(frame_callback_);

  if (disconnected_) {
    return;
  }

  proto::TlsSessionRequest request;
  request.set_frame(frame_bytes.data(), frame_bytes.size());

  std::vector<uint8_t> payload(request.ByteSizeLong());
  if (!base::IsValueInRangeForNumericType<int>(payload.size()) ||
      !request.SerializeToArray(payload.data(),
                                base::checked_cast<int>(payload.size()))) {
    logger_->LogError(FROM_HERE, "Failed to serialize TlsSessionRequest");
    frame_callback_.Run(base::unexpected(Error::kSerializationError));
    return;
  }

  streaming_client_.Send(std::move(payload));
}

void TlsWebSocketClient::OnResponse(
    base::expected<std::vector<uint8_t>, Error> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!frame_callback_) {
    return;
  }

  if (!response.has_value()) {
    frame_callback_.Run(base::unexpected(response.error()));
    return;
  }

  proto::TlsSessionResponse session_response;
  if (!base::IsValueInRangeForNumericType<int>(response->size()) ||
      !session_response.ParseFromArray(
          response->data(), base::checked_cast<int>(response->size()))) {
    logger_->LogError(FROM_HERE, "Failed to parse TlsSessionResponse");
    frame_callback_.Run(base::unexpected(Error::kDeserializationError));
    return;
  }

  frame_callback_.Run(
      base::ok(base::ToVector(base::as_byte_span(session_response.frame()))));
}

void TlsWebSocketClient::OnConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordConnectionResult(net::OK);
}

void TlsWebSocketClient::OnMessage(std::vector<uint8_t> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TlsWebSocketClient::OnResponse,
                                weak_ptr_factory_.GetWeakPtr(),
                                base::ok(std::move(message))));
}

void TlsWebSocketClient::OnConnectionError(const std::string& message,
                                           int net_error,
                                           int response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordConnectionResult(net_error);
  logger_->LogError(
      FROM_HERE, base::StrCat({"PrivateAI TLS connection failed ", message,
                               " (net error:", base::NumberToString(net_error),
                               ", response code:",
                               base::NumberToString(response_code), ")"}));

  base::UmaHistogramSparse("PrivateAi.Client.WebSocketConnectionError.NetError",
                           -net_error);
  if (response_code > 0) {
    base::UmaHistogramSparse(
        "PrivateAi.Client.WebSocketConnectionError.ResponseCode",
        response_code);
  }

  ClosePipe(Error::kConnectionFailed);
}

void TlsWebSocketClient::OnDropChannel(bool was_clean,
                                       uint16_t code,
                                       const std::string& reason,
                                       std::optional<base::TimeDelta> elapsed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->LogError(
      FROM_HERE,
      base::StrCat({"TLS Websocket dropped (code:", base::NumberToString(code),
                    ", reason:", reason, ")"}));
  base::UmaHistogramSparse("PrivateAi.Client.WebSocketCloseCode", code);
  if (elapsed.has_value()) {
    base::UmaHistogramLongTimes(
        "PrivateAi.Client.WebSocketSessionDuration.ClosedByServer", *elapsed);
  }
  if (!reason.empty()) {
    base::UmaHistogramEnumeration(
        "PrivateAi.Client.ServerStatusCode", ParseGoogleRpcCode(reason),
        static_cast<rpc::GoogleRpcCode>(rpc::GoogleRpcCode_MAX + 1));
  }
  ClosePipe(Error::kSocketClosed);
}

void TlsWebSocketClient::OnError(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!message.empty()) {
    logger_->LogError(FROM_HERE, message);
  }
  ClosePipe(Error::kError);
}

void TlsWebSocketClient::OnClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClosePipe(Error::kSocketClosed);
}

void TlsWebSocketClient::ClosePipe(Error status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (disconnected_) {
    return;
  }
  disconnected_ = true;
  streaming_client_.Close();

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TlsWebSocketClient::OnResponse,
                     weak_ptr_factory_.GetWeakPtr(), base::unexpected(status)));
}

void TlsWebSocketClient::RecordConnectionResult(int net_error) {
  if (has_recorded_connection_result_ || !has_chrome_proxy_.has_value()) {
    return;
  }
  has_recorded_connection_result_ = true;
  const char* variant = *has_chrome_proxy_ ? "WithProxy" : "WithoutProxy";
  base::UmaHistogramSparse(
      base::StrCat({"PrivateAi.Client.ConnectionResult.", variant}),
      net_error == net::OK ? net::OK : -net_error);
}

}  // namespace private_ai
