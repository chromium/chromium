// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/fake_fcm_server.h"

#include <stdint.h>

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace fakedms {

namespace {

constexpr size_t kReadBufferSize = 4096;

enum class ParseResult {
  kSuccess,
  kNeedMoreData,
  kError,
};

struct Frame {
  uint8_t tag = 0;
  size_t header_size = 0;
  size_t total_size = 0;
};

}  // namespace

std::string EncodeFcmPacket(uint8_t tag,
                            const google::protobuf::MessageLite& message,
                            bool with_version) {
  std::string packet;
  google::protobuf::io::StringOutputStream string_output_stream(&packet);
  {
    google::protobuf::io::CodedOutputStream coded_output_stream(
        &string_output_stream);
    if (with_version) {
      coded_output_stream.WriteRaw(&kFcmProtocolVersion, 1);
    }
    coded_output_stream.WriteRaw(&tag, 1);
    coded_output_stream.WriteVarint32(message.ByteSizeLong());
    message.SerializeToCodedStream(&coded_output_stream);
  }
  return packet;
}

class FakeFcmServer::ClientConnection {
 public:
  using CloseCallback = base::OnceCallback<void(ClientConnection*)>;

  ClientConnection(std::unique_ptr<net::StreamSocket> socket,
                   base::OnceClosure login_cb,
                   CloseCallback close_cb)
      : socket_(std::move(socket)),
        login_cb_(std::move(login_cb)),
        close_cb_(std::move(close_cb)),
        temp_read_buffer_(
            base::MakeRefCounted<net::IOBufferWithSize>(kReadBufferSize)) {
    DoRead();
  }

  ClientConnection(const ClientConnection&) = delete;
  ClientConnection& operator=(const ClientConnection&) = delete;

  ~ClientConnection() = default;

  bool handshake_complete() const { return handshake_complete_; }

  void SendDataMessage(const mcs_proto::DataMessageStanza& message) {
    if (closed_ || !handshake_complete_) {
      return;
    }
    SendPacket(gcm::kDataMessageStanzaTag, message);
  }

 private:
  void DoRead() {
    if (closed_) {
      return;
    }
    int result =
        socket_->Read(temp_read_buffer_.get(), temp_read_buffer_->size(),
                      base::BindOnce(&ClientConnection::OnReadComplete,
                                     weak_ptr_factory_.GetWeakPtr()));
    if (result != net::ERR_IO_PENDING) {
      OnReadComplete(result);
    }
  }

  void OnReadComplete(int result) {
    if (result <= 0) {
      NotifyClose();
      return;
    }
    auto bytes = temp_read_buffer_->span().first(static_cast<size_t>(result));
    read_buffer_.insert(read_buffer_.end(), bytes.begin(), bytes.end());
    if (!ProcessIncomingData()) {
      NotifyClose();
      return;
    }
    DoRead();
  }

  bool ProcessIncomingData() {
    while (!closed_ && !read_buffer_.empty()) {
      ParseResult result =
          handshake_complete_ ? ProcessPacket() : ProcessLoginRequest();
      if (result != ParseResult::kSuccess) {
        return result == ParseResult::kNeedMoreData;
      }
    }
    return true;
  }

  // Parses the frame header at `offset` in `read_buffer_`. Returns success
  // when the whole payload is buffered.
  ParseResult ParseFrame(size_t offset, Frame* frame) {
    if (read_buffer_.size() < offset + 2) {
      return ParseResult::kNeedMoreData;
    }
    frame->tag = read_buffer_[offset];
    auto rest = base::span(read_buffer_).subspan(offset + 1);
    google::protobuf::io::CodedInputStream input(rest.data(),
                                                 static_cast<int>(rest.size()));
    uint32_t payload_size = 0;
    if (!input.ReadVarint32(&payload_size)) {
      if (rest.size() >= 5) {
        LOG(ERROR) << "Malformed Varint32 size for packet tag "
                   << static_cast<int>(frame->tag);
        return ParseResult::kError;
      }
      return ParseResult::kNeedMoreData;
    }
    frame->header_size = offset + 1 + input.CurrentPosition();
    frame->total_size = frame->header_size + payload_size;
    return read_buffer_.size() < frame->total_size ? ParseResult::kNeedMoreData
                                                   : ParseResult::kSuccess;
  }

  ParseResult ProcessLoginRequest() {
    uint8_t version = read_buffer_[0];
    if (version < 38) {
      LOG(ERROR) << "Unsupported MCS version: " << static_cast<int>(version);
      return ParseResult::kError;
    }
    Frame frame;
    if (ParseResult result = ParseFrame(1, &frame);
        result != ParseResult::kSuccess) {
      return result;
    }
    if (frame.tag != gcm::kLoginRequestTag) {
      LOG(ERROR) << "Expected LoginRequest tag, got: "
                 << static_cast<int>(frame.tag);
      return ParseResult::kError;
    }

    mcs_proto::LoginRequest login_request;
    auto payload =
        base::span(read_buffer_)
            .subspan(frame.header_size, frame.total_size - frame.header_size);
    if (!login_request.ParseFromArray(payload.data(),
                                      static_cast<int>(payload.size()))) {
      LOG(ERROR) << "Failed to parse LoginRequest proto";
      return ParseResult::kError;
    }

    mcs_proto::LoginResponse login_response;
    login_response.set_id(login_request.id());
    login_response.set_last_stream_id_received(1);
    SendPacket(gcm::kLoginResponseTag, login_response,
               /*include_version=*/true);

    read_buffer_.erase(read_buffer_.begin(),
                       read_buffer_.begin() + frame.total_size);
    handshake_complete_ = true;
    std::move(login_cb_).Run();
    return ParseResult::kSuccess;
  }

  ParseResult ProcessPacket() {
    Frame frame;
    if (ParseResult result = ParseFrame(0, &frame);
        result != ParseResult::kSuccess) {
      return result;
    }
    if (frame.tag == gcm::kHeartbeatPingTag) {
      SendPacket(gcm::kHeartbeatAckTag, mcs_proto::HeartbeatAck());
    }
    read_buffer_.erase(read_buffer_.begin(),
                       read_buffer_.begin() + frame.total_size);
    return ParseResult::kSuccess;
  }

  void SendPacket(uint8_t tag,
                  const google::protobuf::MessageLite& message,
                  bool include_version = false) {
    std::string packet = EncodeFcmPacket(tag, message, include_version);
    const size_t packet_size = packet.size();
    write_queue_.push_back(base::MakeRefCounted<net::DrainableIOBuffer>(
        base::MakeRefCounted<net::StringIOBuffer>(std::move(packet)),
        packet_size));
    if (!is_writing_) {
      DoWrite();
    }
  }

  void DoWrite() {
    if (closed_ || write_queue_.empty()) {
      is_writing_ = false;
      return;
    }
    is_writing_ = true;
    scoped_refptr<net::DrainableIOBuffer> buffer = write_queue_.front();
    int result =
        socket_->Write(buffer.get(), buffer->BytesRemaining(),
                       base::BindOnce(&ClientConnection::OnWriteComplete,
                                      weak_ptr_factory_.GetWeakPtr()),
                       TRAFFIC_ANNOTATION_FOR_TESTS);
    if (result != net::ERR_IO_PENDING) {
      OnWriteComplete(result);
    }
  }

  void OnWriteComplete(int result) {
    if (result <= 0) {
      NotifyClose();
      return;
    }
    write_queue_.front()->DidConsume(result);
    if (write_queue_.front()->BytesRemaining() == 0) {
      write_queue_.pop_front();
    }
    DoWrite();
  }

  // Marks the connection as closed. The owner deletes it in a posted task,
  // so callers on the stack never use a deleted connection.
  void NotifyClose() {
    if (std::exchange(closed_, true)) {
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ClientConnection::RunCloseCallback,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  void RunCloseCallback() { std::move(close_cb_).Run(this); }

  std::unique_ptr<net::StreamSocket> socket_;
  base::OnceClosure login_cb_;
  CloseCallback close_cb_;
  scoped_refptr<net::IOBufferWithSize> temp_read_buffer_;
  std::vector<uint8_t> read_buffer_;
  std::deque<scoped_refptr<net::DrainableIOBuffer>> write_queue_;
  bool handshake_complete_ = false;
  bool is_writing_ = false;
  bool closed_ = false;

  base::WeakPtrFactory<ClientConnection> weak_ptr_factory_{this};
};

FakeFcmServer::FakeFcmServer()
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

FakeFcmServer::~FakeFcmServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

bool FakeFcmServer::Start(int port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
  server_socket_ = std::make_unique<net::TCPServerSocket>(
      /*net_log=*/nullptr, net::NetLogSource());
  net::IPEndPoint bind_address(net::IPAddress::IPv4Localhost(), port);
  int result = server_socket_->Listen(bind_address, /*backlog=*/10,
                                      /*ipv6_only=*/std::nullopt);
  if (result != net::OK) {
    LOG(ERROR) << "FakeFcmServer failed to listen: "
               << net::ErrorToString(result);
    server_socket_.reset();
    return false;
  }

  net::IPEndPoint local_address;
  result = server_socket_->GetLocalAddress(&local_address);
  if (result != net::OK) {
    LOG(ERROR) << "FakeFcmServer failed to get local address: "
               << net::ErrorToString(result);
    server_socket_.reset();
    return false;
  }
  port_ = local_address.port();
  AcceptNextConnection();
  return true;
}

void FakeFcmServer::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connections_.clear();
  logged_in_count_ = 0;
  server_socket_.reset();
  accepted_socket_.reset();
  port_ = 0;
}

int FakeFcmServer::port() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return port_;
}

void FakeFcmServer::BroadcastDataMessage(
    const mcs_proto::DataMessageStanza& message) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&FakeFcmServer::BroadcastDataMessage,
                                          weak_ptr_, message));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& conn : connections_) {
    conn->SendDataMessage(message);
  }
}

bool FakeFcmServer::HasLoggedInClients() const {
  return logged_in_count_.load() > 0;
}

void FakeFcmServer::AcceptNextConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (server_socket_) {
    int result = server_socket_->Accept(
        &accepted_socket_, base::BindOnce(&FakeFcmServer::OnAcceptComplete,
                                          weak_ptr_factory_.GetWeakPtr()));
    if (result == net::ERR_IO_PENDING) {
      return;
    }
    if (!HandleAcceptResult(result)) {
      return;
    }
  }
}

void FakeFcmServer::OnAcceptComplete(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (HandleAcceptResult(result)) {
    AcceptNextConnection();
  }
}

bool FakeFcmServer::HandleAcceptResult(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != net::OK) {
    LOG(ERROR) << "FakeFcmServer failed to accept connection: "
               << net::ErrorToString(result);
    return false;
  }
  if (accepted_socket_) {
    auto conn = std::make_unique<ClientConnection>(
        std::move(accepted_socket_),
        base::BindOnce(&FakeFcmServer::OnClientLoggedIn,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&FakeFcmServer::RemoveConnection,
                       weak_ptr_factory_.GetWeakPtr()));
    connections_.insert(std::move(conn));
  }
  return true;
}

void FakeFcmServer::RemoveConnection(ClientConnection* connection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = connections_.find(connection);
  if (it == connections_.end()) {
    return;
  }
  if ((*it)->handshake_complete()) {
    --logged_in_count_;
  }
  connections_.erase(it);
}

void FakeFcmServer::OnClientLoggedIn() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++logged_in_count_;
}

}  // namespace fakedms
