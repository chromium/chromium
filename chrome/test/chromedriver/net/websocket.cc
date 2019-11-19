// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/websocket.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/hash/sha1.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/sys_addrinfo.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/log/net_log_source.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/websockets/websocket_frame.h"

#if defined(OS_WIN)
#include <Winsock2.h>
#endif

namespace {

bool ResolveHost(const std::string& host,
                 uint16_t port,
                 net::AddressList* address_list) {
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo* result;
  if (getaddrinfo(host.c_str(), NULL, &hints, &result))
    return false;

  auto list = net::AddressList::CreateFromAddrinfo(result);
  *address_list = net::AddressList::CopyWithPort(list, port);
  freeaddrinfo(result);
  return !address_list->empty();
}

}  // namespace

WebSocket::WebSocket(const GURL& url,
                     WebSocketListener* listener,
                     size_t read_buffer_size)
    : url_(url),
      listener_(listener),
      state_(INITIALIZED),
      write_buffer_(base::MakeRefCounted<net::DrainableIOBuffer>(
          base::MakeRefCounted<net::IOBuffer>(0),
          0)),
      read_buffer_(
          base::MakeRefCounted<net::IOBufferWithSize>(read_buffer_size)) {}

WebSocket::~WebSocket() {
  CHECK(thread_checker_.CalledOnValidThread());
}

void WebSocket::Connect(net::CompletionOnceCallback callback) {
  CHECK(thread_checker_.CalledOnValidThread());
  CHECK_EQ(INITIALIZED, state_);

  net::IPAddress address;
  net::AddressList addresses;
  uint16_t port = static_cast<uint16_t>(url_.EffectiveIntPort());
  if (ParseURLHostnameToAddress(url_.host(), &address)) {
    addresses = net::AddressList::CreateFromIPAddress(address, port);
  } else {
    if (!ResolveHost(url_.HostNoBrackets(), port, &addresses)) {
      std::move(callback).Run(net::ERR_ADDRESS_UNREACHABLE);
      return;
    }
    base::ListValue endpoints;
    for (auto endpoint : addresses)
      endpoints.AppendString(endpoint.ToStringWithoutPort());
    std::string json;
    CHECK(base::JSONWriter::Write(endpoints, &json));
    VLOG(0) << "resolved " << url_.HostNoBracketsPiece() << " to " << json;
  }

  net::NetLogSource source;
  socket_.reset(new net::TCPClientSocket(addresses, NULL, NULL, source));

  state_ = CONNECTING;
  connect_callback_ = std::move(callback);
  int code = socket_->Connect(
      base::BindOnce(&WebSocket::OnSocketConnect, base::Unretained(this)));
  VLOG(4) << "WebSocket::Connect code=" << net::ErrorToShortString(code);
  if (code != net::ERR_IO_PENDING)
    OnSocketConnect(code);
}

bool WebSocket::Send(const std::string& message) {
  VLOG(4) << "WebSocket::Send " << message;
  CHECK(thread_checker_.CalledOnValidThread());
  if (state_ != OPEN)
    return false;

  net::WebSocketFrameHeader header(net::WebSocketFrameHeader::kOpCodeText);
  header.final = true;
  header.masked = true;
  header.payload_length = message.length();
  int header_size = net::GetWebSocketFrameHeaderSize(header);
  net::WebSocketMaskingKey masking_key = net::GenerateWebSocketMaskingKey();
  std::string header_str;
  header_str.resize(header_size);
  CHECK_EQ(header_size, net::WriteWebSocketFrameHeader(
      header, &masking_key, &header_str[0], header_str.length()));

  std::string masked_message = message;
  net::MaskWebSocketFramePayload(
      masking_key, 0, &masked_message[0], masked_message.length());
  Write(header_str + masked_message);
  return true;
}

void WebSocket::OnSocketConnect(int code) {
  VLOG(4) << "WebSocket::OnSocketConnect code="
          << net::ErrorToShortString(code);

  if (code != net::OK) {
    VLOG(1) << "failed to connect to " << url_.HostNoBracketsPiece()
            << " (error " << code << ")";
    Close(code);
    return;
  }

  base::Base64Encode(base::RandBytesAsString(16), &sec_key_);
  std::string handshake = base::StringPrintf(
      "GET %s HTTP/1.1\r\n"
      "Host: %s\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: %s\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "Pragma: no-cache\r\n"
      "Cache-Control: no-cache\r\n"
      "\r\n",
      url_.path().c_str(),
      url_.host().c_str(),
      sec_key_.c_str());
  VLOG(4) << "WebSocket::OnSocketConnect handshake\n" << handshake;
  Write(handshake);
  if (state_ == CLOSED) {
    // The call to Write() above would call Close() if it encounters an error,
    // in which case it's no longer safe to do anything else. Close() has
    // already called the callback function, if any.
    return;
  }
  Read();
}

void WebSocket::Write(const std::string& data) {
  pending_write_ += data;
  if (!write_buffer_->BytesRemaining())
    ContinueWritingIfNecessary();
}

void WebSocket::OnWrite(int code) {
  if (!socket_->IsConnected()) {
    // Supposedly if |StreamSocket| is closed, the error code may be undefined.
    Close(net::ERR_FAILED);
    return;
  }
  if (code < 0) {
    Close(code);
    return;
  }

  write_buffer_->DidConsume(code);
  ContinueWritingIfNecessary();
}

void WebSocket::ContinueWritingIfNecessary() {
  if (!write_buffer_->BytesRemaining()) {
    if (pending_write_.empty())
      return;
    write_buffer_ = base::MakeRefCounted<net::DrainableIOBuffer>(
        base::MakeRefCounted<net::StringIOBuffer>(pending_write_),
        pending_write_.length());
    pending_write_.clear();
  }
  int code = socket_->Write(
      write_buffer_.get(), write_buffer_->BytesRemaining(),
      base::BindOnce(&WebSocket::OnWrite, base::Unretained(this)),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  if (code != net::ERR_IO_PENDING)
    OnWrite(code);
}

void WebSocket::Read() {
  while (true) {
    int code = socket_->Read(
        read_buffer_.get(), read_buffer_->size(),
        base::BindOnce(&WebSocket::OnRead, base::Unretained(this), true));
    if (code == net::ERR_IO_PENDING)
      break;

    OnRead(false, code);
    if (state_ == CLOSED)
      break;
  }
}

void WebSocket::OnRead(bool read_again, int code) {
  if (code <= 0) {
    VLOG(4) << "WebSocket::OnRead error " << net::ErrorToShortString(code);
    Close(code ? code : net::ERR_FAILED);
    return;
  }

  if (state_ == CONNECTING)
    OnReadDuringHandshake(read_buffer_->data(), code);
  else if (state_ == OPEN)
    OnReadDuringOpen(read_buffer_->data(), code);

  // If we were called by the event loop due to arrival of data, call Read()
  // again to read more data. If we were called by Read(), however, simply
  // return to Read() and let it call socket_->Read() to read more data, and
  // potentially call OnRead() again. This is necessary to avoid mutual
  // recursion between Read and OnRead, which can cause stack overflow (e.g.,
  // see https://crbug.com/877105).
  if (read_again && state_ != CLOSED)
    Read();
}

void WebSocket::OnReadDuringHandshake(const char* data, int len) {
  VLOG(4) << "WebSocket::OnReadDuringHandshake\n" << std::string(data, len);
  handshake_response_ += std::string(data, len);
  size_t headers_end = net::HttpUtil::LocateEndOfHeaders(
      handshake_response_.data(), handshake_response_.size(), 0);
  if (headers_end == std::string::npos)
    return;

  const char kMagicKey[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  std::string websocket_accept;
  base::Base64Encode(base::SHA1HashString(sec_key_ + kMagicKey),
                     &websocket_accept);
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          base::StringPiece(handshake_response_.data(), headers_end)));
  if (headers->response_code() != 101 ||
      !headers->HasHeaderValue("Upgrade", "WebSocket") ||
      !headers->HasHeaderValue("Connection", "Upgrade") ||
      !headers->HasHeaderValue("Sec-WebSocket-Accept", websocket_accept)) {
    Close(net::ERR_FAILED);
    return;
  }
  std::string leftover_message = handshake_response_.substr(headers_end);
  handshake_response_.clear();
  sec_key_.clear();
  state_ = OPEN;
  InvokeConnectCallback(net::OK);
  if (!leftover_message.empty())
    OnReadDuringOpen(leftover_message.c_str(), leftover_message.length());
}

void WebSocket::OnReadDuringOpen(const char* data, int len) {
  std::vector<std::unique_ptr<net::WebSocketFrameChunk>> frame_chunks;
  CHECK(parser_.Decode(data, len, &frame_chunks));
  for (size_t i = 0; i < frame_chunks.size(); ++i) {
    const auto& header = frame_chunks[i]->header;
    if (header) {
      DCHECK_EQ(0u, current_frame_offset_);
      is_current_frame_masked_ = header->masked;
      current_masking_key_ = header->masking_key;
    }

    auto& buffer = frame_chunks[i]->payload;
    std::vector<char> payload(buffer.begin(), buffer.end());
    if (is_current_frame_masked_) {
      MaskWebSocketFramePayload(current_masking_key_, current_frame_offset_,
                                payload.data(), payload.size());
    }
    next_message_ += std::string(payload.data(), payload.size());
    current_frame_offset_ += payload.size();

    if (frame_chunks[i]->final_chunk) {
      VLOG(4) << "WebSocket::OnReadDuringOpen " << next_message_;
      listener_->OnMessageReceived(next_message_);
      next_message_.clear();
      current_frame_offset_ = 0;
    }
  }
}

void WebSocket::InvokeConnectCallback(int code) {
  CHECK(!connect_callback_.is_null());
  std::move(connect_callback_).Run(code);
}

void WebSocket::Close(int code) {
  socket_->Disconnect();
  if (!connect_callback_.is_null())
    InvokeConnectCallback(code);
  if (state_ == OPEN)
    listener_->OnClose();

  state_ = CLOSED;
}
