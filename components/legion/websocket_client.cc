// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/websocket_client.h"

#include <limits>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "net/http/http_request_headers.h"
#include "net/storage_access_api/status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace legion {
namespace {

constexpr size_t kMaxIncomingMessageSize = 1 << 20;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("legion_client", R"(
        semantics {
          sender: "Legion Client"
          description:
            "This traffic creates an encrypted session with the "
            "legion service and carries the request and response over that "
            "session. "
            "The feature is under development and behind a feature flag."
          trigger:
            "A feature that uses the legion component is triggered. "
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

WebSocketClient::WebSocketClient(const GURL& service_url,
                                 NetworkContextFactory network_context_factory)
    : service_url_(service_url),
      network_context_factory_(std::move(network_context_factory)),
      readable_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL) {}

WebSocketClient::~WebSocketClient() = default;

void WebSocketClient::SetResponseCallback(ResponseCallback callback) {
  CHECK_EQ(state_, State::kInitialized);
  response_callback_ = std::move(callback);
}

void WebSocketClient::Send(const oak::session::v1::SessionRequest& request) {
  CHECK(response_callback_);
  std::string binary_proto;
  if (!request.SerializeToString(&binary_proto)) {
    LOG(ERROR) << "Failed to serialize proto request into a string. Check all "
                  "required fields are set";
    response_callback_.Run(
        base::unexpected(TransportError::kSerializationError));
    return;
  }

  Send(Request(binary_proto.begin(), binary_proto.end()));
}

void WebSocketClient::Send(Request request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kDisconnected ||
      request.size() > std::numeric_limits<uint32_t>::max()) {
    ClosePipe(TransportError::kError);
    return;
  }

  if (state_ == State::kInitialized) {
    Connect();
  }

  if (state_ != State::kOpen) {
    pending_write_data_.push(std::move(request));
    return;
  }

  InternalWrite(request);
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

void WebSocketClient::Connect() {
  // A disconnect handler is used so that the request can be completed in the
  // event of an unexpected disconnection from the network service.
  auto handshake_remote = handshake_receiver_.BindNewPipeAndPassRemote();
  // base::Unretained(this) is safe because client_receiver_ is owned by
  // |this|.
  handshake_receiver_.set_disconnect_handler(base::BindOnce(
      &WebSocketClient::OnMojoPipeDisconnect, base::Unretained(this)));

  state_ = State::kConnecting;

  std::vector<std::string> requested_protocols;

  std::vector<network::mojom::HttpHeaderPtr> additional_headers{};
  additional_headers.push_back(network::mojom::HttpHeader::New(
      "X-WebChannel-Content-Type", "application/x-protobuf"));

  network_context_factory_.Run()->CreateWebSocket(
      service_url_, requested_protocols, net::SiteForCookies(),
      net::StorageAccessApiStatus::kNone,
      net::IsolationInfo::CreateForInternalRequest(
          url::Origin::Create(service_url_)),
      std::move(additional_headers), network::mojom::kBrowserProcessId,
      url::Origin::Create(service_url_),
      network::mojom::ClientSecurityState::New(),
      network::mojom::kWebSocketOptionBlockAllCookies,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      std::move(handshake_remote),
      /*url_loader_network_observer=*/mojo::NullRemote(),
      /*auth_handler=*/mojo::NullRemote(),
      /*header_client=*/mojo::NullRemote(),
      /*throttling_profile_id=*/std::nullopt);
}

void WebSocketClient::InternalWrite(base::span<const uint8_t> data) {
  CHECK(state_ == State::kOpen);

  // Use the BINARY message type because the message is a binary-encoded
  // protobuf. The TEXT message type would be used for JSON.
  websocket_->SendMessage(network::mojom::WebSocketMessageType::BINARY,
                          data.size());
  MojoResult result = writable_->WriteAllData(data);
  if (result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Failed to write to WebSocket.";
    ClosePipe(TransportError::kError);
  }
}

void WebSocketClient::OnOpeningHandshakeStarted(
    network::mojom::WebSocketHandshakeRequestPtr request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebSocketClient::OnFailure(const std::string& message,
                                int net_error,
                                int response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "Legion service connection failed " << message << ", "
             << net_error << ", " << response_code;

  ClosePipe(TransportError::kError);
}

void WebSocketClient::OnConnectionEstablished(
    mojo::PendingRemote<network::mojom::WebSocket> socket,
    mojo::PendingReceiver<network::mojom::WebSocketClient> client_receiver,
    network::mojom::WebSocketHandshakeResponsePtr response,
    mojo::ScopedDataPipeConsumerHandle readable,
    mojo::ScopedDataPipeProducerHandle writable) {
  CHECK(!websocket_.is_bound());
  CHECK(state_ == State::kConnecting);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  websocket_.Bind(std::move(socket));
  readable_ = std::move(readable);
  // base::Unretained(this) is safe because readable_watcher_ is owned by
  // |this|.
  CHECK_EQ(readable_watcher_.Watch(
               readable_.get(), MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
               base::BindRepeating(&WebSocketClient::ReadFromDataPipe,
                                   base::Unretained(this))),
           MOJO_RESULT_OK);
  writable_ = std::move(writable);
  client_receiver_.Bind(std::move(client_receiver));

  // |handshake_receiver_| will disconnect soon. In order to catch network
  // process crashes, we switch to watching |client_receiver_|.
  handshake_receiver_.set_disconnect_handler(base::DoNothing());
  // base::Unretained(this) is safe because client_receiver_ is owned by
  // |this|.
  client_receiver_.set_disconnect_handler(base::BindOnce(
      &WebSocketClient::OnMojoPipeDisconnect, base::Unretained(this)));

  websocket_->StartReceiving();

  state_ = State::kOpen;

  while (!pending_write_data_.empty()) {
    InternalWrite(pending_write_data_.front());
    // Writing might fail which will close the socket.
    if (state_ != State::kOpen) {
      return;
    }
    pending_write_data_.pop();
  }
}

void WebSocketClient::OnDataFrame(bool finish,
                                  network::mojom::WebSocketMessageType type,
                                  uint64_t data_len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kOpen);
  CHECK_EQ(pending_read_data_index_, pending_read_data_.size());
  CHECK(!pending_read_finished_);
  if (data_len == 0) {
    if (finish) {
      ProcessCompletedResponse();
    }
    return;
  }

  const size_t old_size = pending_read_data_index_;
  const size_t new_size = old_size + data_len;
  if ((type != network::mojom::WebSocketMessageType::BINARY &&
       type != network::mojom::WebSocketMessageType::CONTINUATION) ||
      data_len > std::numeric_limits<uint32_t>::max() || new_size < old_size ||
      new_size > kMaxIncomingMessageSize) {
    LOG(ERROR) << "Invalid WebSocket frame (type: " << static_cast<int>(type)
               << ", len: " << data_len << ")";
    ClosePipe(TransportError::kError);
    return;
  }

  pending_read_data_.resize(new_size);
  pending_read_finished_ = finish;
  client_receiver_.Pause();
  ReadFromDataPipe(MOJO_RESULT_OK, mojo::HandleSignalsState());
}

void WebSocketClient::OnDropChannel(bool was_clean,
                                    uint16_t code,
                                    const std::string& reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(state_ == State::kOpen || state_ == State::kConnecting);
  LOG(ERROR) << "OnDropChannel: " << code << " - " << reason;

  ClosePipe(TransportError::kSocketClosed);
}

void WebSocketClient::OnClosingHandshake() {}

void WebSocketClient::ReadFromDataPipe(MojoResult,
                                       const mojo::HandleSignalsState&) {
  CHECK_LT(pending_read_data_index_, pending_read_data_.size());

  size_t actually_read_bytes = 0;
  const MojoResult result = readable_->ReadData(
      MOJO_READ_DATA_FLAG_NONE,
      base::span(pending_read_data_).subspan(pending_read_data_index_),
      actually_read_bytes);
  if (result == MOJO_RESULT_OK) {
    pending_read_data_index_ += actually_read_bytes;
    DCHECK_LE(pending_read_data_index_, pending_read_data_.size());

    if (pending_read_data_index_ < pending_read_data_.size()) {
      readable_watcher_.ArmOrNotify();
    } else {
      client_receiver_.Resume();
      if (pending_read_finished_) {
        ProcessCompletedResponse();
      }
    }
  } else if (result == MOJO_RESULT_SHOULD_WAIT) {
    readable_watcher_.ArmOrNotify();
  } else {
    LOG(ERROR) << "Reading WebSocket frame failed: "
               << static_cast<int>(result);
    ClosePipe(TransportError::kError);
  }
}

void WebSocketClient::ProcessCompletedResponse() {
  std::vector<uint8_t> pending_read_data;
  pending_read_data.swap(pending_read_data_);
  pending_read_data_index_ = 0;
  pending_read_finished_ = false;

  // Call OnResponse asynchronously since this object may be destroyed during
  // the callback.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WebSocketClient::OnResponse,
                                weak_ptr_factory_.GetWeakPtr(),
                                base::ok(std::move(pending_read_data))));
}

void WebSocketClient::ClosePipe(TransportError status) {
  if (state_ == State::kDisconnected) {
    return;
  }
  state_ = State::kDisconnected;
  client_receiver_.reset();
  pending_write_data_ = {};
  pending_read_data_index_ = 0;
  pending_read_finished_ = false;
  pending_read_data_.clear();

  // Call OnResponse asynchronously since this object may be destroyed during
  // the callback.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebSocketClient::OnResponse,
                     weak_ptr_factory_.GetWeakPtr(), base::unexpected(status)));
}

void WebSocketClient::OnMojoPipeDisconnect() {
  ClosePipe(TransportError::kSocketClosed);
}

}  // namespace legion
