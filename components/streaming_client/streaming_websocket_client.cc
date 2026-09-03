// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/streaming_client/streaming_websocket_client.h"

#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/isolation_info.h"
#include "net/storage_access_api/status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace streaming_client {
namespace {

constexpr size_t kMaxIncomingMessageSize = 1 << 20;

}  // namespace

std::vector<network::mojom::HttpHeaderPtr>
StreamingWebSocketClient::Delegate::GetAdditionalHeaders() {
  return {};
}

void StreamingWebSocketClient::Delegate::OnConnected() {}

StreamingWebSocketClient::StreamingWebSocketClient(
    const GURL& service_url,
    network::mojom::NetworkContext* network_context,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    Delegate* delegate)
    : service_url_(service_url),
      network_context_(network_context),
      traffic_annotation_(traffic_annotation),
      delegate_(delegate),
      readable_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL) {}

StreamingWebSocketClient::~StreamingWebSocketClient() = default;

void StreamingWebSocketClient::Send(std::vector<uint8_t> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kDisconnected ||
      request.size() > std::numeric_limits<uint32_t>::max()) {
    OnError("");
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

void StreamingWebSocketClient::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClosePipe();
}

void StreamingWebSocketClient::Connect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(network_context_);
  CHECK(delegate_);

  if (state_ == State::kConnecting || state_ == State::kOpen) {
    return;
  }

  // A disconnect handler is used so that the request can be completed in the
  // event of an unexpected disconnection from the network service.
  auto handshake_remote = handshake_receiver_.BindNewPipeAndPassRemote();
  // base::Unretained(this) is safe because client_receiver_ is owned by
  // `this`.
  handshake_receiver_.set_disconnect_handler(base::BindOnce(
      &StreamingWebSocketClient::OnMojoPipeDisconnect, base::Unretained(this)));

  state_ = State::kConnecting;

  std::vector<std::string> requested_protocols;

  std::vector<network::mojom::HttpHeaderPtr> additional_headers =
      delegate_->GetAdditionalHeaders();
  additional_headers.push_back(network::mojom::HttpHeader::New(
      "X-WebChannel-Content-Type", "application/x-protobuf"));

  network_context_->CreateWebSocket(
      service_url_, requested_protocols, net::StorageAccessApiStatus::kNone,
      net::IsolationInfo::CreateForInternalRequest(
          url::Origin::Create(service_url_)),
      std::move(additional_headers), network::OriginatingProcessId::browser(),
      url::Origin::Create(service_url_),
      network::mojom::ClientSecurityState::New(),
      network::mojom::kWebSocketOptionBlockAllCookies,
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation_),
      std::move(handshake_remote),
      /*url_loader_network_observer=*/mojo::NullRemote(),
      /*auth_handler=*/mojo::NullRemote(),
      /*header_client=*/mojo::NullRemote(),
      /*throttling_profile_id=*/std::nullopt,
      // WebSocket connections are browser-wide operations not associated with
      // any page/frame, so no Connection Allowlist restrictions should apply.
      network::GetNoOpNetworkRestrictionsId(),
      /*target_address_space=*/network::mojom::IPAddressSpace::kUnknown);
}

void StreamingWebSocketClient::InternalWrite(base::span<const uint8_t> data) {
  CHECK(state_ == State::kOpen);

  // Use the BINARY message type because the message is a binary-encoded
  // protobuf. The TEXT message type would be used for JSON.
  websocket_->SendMessage(network::mojom::WebSocketMessageType::BINARY,
                          data.size());
  MojoResult result = writable_->WriteAllData(data);
  if (result != MOJO_RESULT_OK) {
    OnError("Failed to write to WebSocket.");
  }
}

void StreamingWebSocketClient::OnOpeningHandshakeStarted(
    network::mojom::WebSocketHandshakeRequestPtr request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void StreamingWebSocketClient::OnFailure(const std::string& message,
                                         int net_error,
                                         int response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClosePipe();
  delegate_->OnConnectionError(message, net_error, response_code);
}

void StreamingWebSocketClient::OnConnectionEstablished(
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
  // `this`.
  CHECK_EQ(readable_watcher_.Watch(
               readable_.get(), MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
               base::BindRepeating(&StreamingWebSocketClient::ReadFromDataPipe,
                                   base::Unretained(this))),
           MOJO_RESULT_OK);
  writable_ = std::move(writable);
  client_receiver_.Bind(std::move(client_receiver));

  // `handshake_receiver_` will disconnect soon. In order to catch network
  // process crashes, we switch to watching `client_receiver_`.
  handshake_receiver_.set_disconnect_handler(base::DoNothing());
  // base::Unretained(this) is safe because client_receiver_ is owned by
  // `this`.
  client_receiver_.set_disconnect_handler(base::BindOnce(
      &StreamingWebSocketClient::OnMojoPipeDisconnect, base::Unretained(this)));

  websocket_->StartReceiving();

  state_ = State::kOpen;
  connection_open_time_ = base::TimeTicks::Now();

  while (!pending_write_data_.empty()) {
    InternalWrite(pending_write_data_.front());
    // Writing might fail which will close the socket.
    if (state_ != State::kOpen) {
      return;
    }
    pending_write_data_.pop();
  }

  if (state_ == State::kOpen) {
    delegate_->OnConnected();
  }
}

void StreamingWebSocketClient::OnDataFrame(
    bool finish,
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
    OnError(base::StrCat({"Invalid WebSocket frame (type: ",
                          base::NumberToString(static_cast<int>(type)),
                          ", len: ", base::NumberToString(data_len), ")"}));
    return;
  }

  pending_read_data_.resize(new_size);
  pending_read_finished_ = finish;
  client_receiver_.Pause();
  ReadFromDataPipe(MOJO_RESULT_OK, mojo::HandleSignalsState());
}

void StreamingWebSocketClient::OnDropChannel(bool was_clean,
                                             uint16_t code,
                                             const std::string& reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(state_ == State::kOpen || state_ == State::kConnecting);
  std::optional<base::TimeDelta> elapsed;
  if (state_ == State::kOpen) {
    elapsed = base::TimeTicks::Now() - connection_open_time_;
  }
  ClosePipe();
  delegate_->OnDropChannel(was_clean, code, reason, elapsed);
}

void StreamingWebSocketClient::OnClosingHandshake() {}

void StreamingWebSocketClient::ReadFromDataPipe(
    MojoResult,
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
    OnError(base::StrCat({"Reading WebSocket frame failed: ",
                          base::NumberToString(static_cast<int>(result))}));
  }
}

void StreamingWebSocketClient::ProcessCompletedResponse() {
  std::vector<uint8_t> pending_read_data;
  pending_read_data.swap(pending_read_data_);
  pending_read_data_index_ = 0;
  pending_read_finished_ = false;

  delegate_->OnMessage(std::move(pending_read_data));
}

void StreamingWebSocketClient::ClosePipe() {
  if (state_ == State::kDisconnected) {
    return;
  }
  state_ = State::kDisconnected;
  websocket_.reset();
  readable_watcher_.Cancel();
  readable_.reset();
  writable_.reset();
  client_receiver_.reset();
  handshake_receiver_.reset();
  pending_write_data_ = {};
  pending_read_data_index_ = 0;
  pending_read_finished_ = false;
  pending_read_data_.clear();
}

void StreamingWebSocketClient::OnError(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClosePipe();
  delegate_->OnError(message);
}

void StreamingWebSocketClient::OnMojoPipeDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClosePipe();
  delegate_->OnClose();
}

}  // namespace streaming_client
