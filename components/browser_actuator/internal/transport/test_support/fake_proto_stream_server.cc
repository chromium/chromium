// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/test_support/fake_proto_stream_server.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "components/browser_actuator/internal/transport/test_support/wait_for.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace browser_actuator {
namespace {

// Protobuf length-delimited field helper: writes `tag` followed by the varint
// length of `payload` and `payload` itself.
std::string EncodeLengthDelimitedField(uint32_t field_number,
                                       std::string_view payload) {
  std::string out;
  google::protobuf::io::StringOutputStream string_stream(&out);
  google::protobuf::io::CodedOutputStream coded_stream(&string_stream);
  constexpr uint32_t kLengthDelimitedWireType = 2;
  coded_stream.WriteTag((field_number << 3) | kLengthDelimitedWireType);
  coded_stream.WriteVarint32(static_cast<uint32_t>(payload.size()));
  coded_stream.WriteRaw(payload.data(), static_cast<int>(payload.size()));
  return out;
}

// Encodes an `rpc.Status` proto (`int32 code = 1; string message = 2;`).
std::string EncodeRpcStatus(int32_t code, std::string_view message) {
  std::string out;
  {
    google::protobuf::io::StringOutputStream string_stream(&out);
    google::protobuf::io::CodedOutputStream coded_stream(&string_stream);
    if (code != 0) {
      constexpr uint32_t kVarintWireType = 0;
      coded_stream.WriteTag((1u << 3) | kVarintWireType);
      coded_stream.WriteVarint64(static_cast<uint64_t>(code));
    }
    if (!message.empty()) {
      constexpr uint32_t kLengthDelimitedWireType = 2;
      coded_stream.WriteTag((2u << 3) | kLengthDelimitedWireType);
      coded_stream.WriteVarint32(static_cast<uint32_t>(message.size()));
      coded_stream.WriteRaw(message.data(), static_cast<int>(message.size()));
    }
  }
  return out;
}

}  // namespace

FakeProtoStreamServer::FakeProtoStreamServer(
    network::TestURLLoaderFactory* url_loader_factory,
    const GURL& endpoint)
    : url_loader_factory_(url_loader_factory), endpoint_(endpoint) {}

FakeProtoStreamServer::~FakeProtoStreamServer() = default;

void FakeProtoStreamServer::WaitForConnection() {
  bool connected = WaitFor([&]() {
    auto* pending_requests = url_loader_factory_->pending_requests();
    for (auto it = pending_requests->begin(); it != pending_requests->end();
         ++it) {
      if (it->request.url == endpoint_) {
        network::TestURLLoaderFactory::PendingRequest pending = std::move(*it);
        pending_requests->erase(it);

        ++connection_count_;
        last_resource_request_ = pending.request;
        last_watch_request_.Clear();
        std::string request_body = network::GetUploadData(pending.request);
        CHECK(last_watch_request_.ParseFromString(request_body));

        client_remote_ = std::move(pending.client);

        auto head = network::mojom::URLResponseHead::New();
        std::string raw_headers =
            "HTTP/1.1 200 OK\r\nContent-Type: application/x-protobuf\r\n\r\n";
        head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
            net::HttpUtil::AssembleRawHeaders(raw_headers));
        head->mime_type = "application/x-protobuf";

        mojo::ScopedDataPipeProducerHandle producer;
        mojo::ScopedDataPipeConsumerHandle consumer;
        CHECK_EQ(mojo::CreateDataPipe(256 * 1024, producer, consumer),
                 MOJO_RESULT_OK);

        producer_handle_ = std::move(producer);
        client_remote_->OnReceiveResponse(std::move(head), std::move(consumer),
                                          std::nullopt);
        client_remote_.FlushForTesting();
        return true;
      }
    }
    return false;
  });
  CHECK(connected) << "Timed out waiting for stream connection to "
                   << endpoint_;
}

void FakeProtoStreamServer::PushMessage(
    const ActuatorDownstreamMessage& message) {
  CHECK(producer_handle_.is_valid());
  WatchSessionsResponse response;
  *response.mutable_actuator_downstream_message() = message;
  std::string serialized;
  CHECK(response.SerializeToString(&serialized));
  // StreamBody field 1: `repeated bytes message = 1;`
  WriteRawBytes(EncodeLengthDelimitedField(1, serialized));
}

void FakeProtoStreamServer::PushKeepAlive(std::string_view noop_payload) {
  CHECK(producer_handle_.is_valid());
  // StreamBody field 15: `repeated bytes noop = 15;`
  WriteRawBytes(EncodeLengthDelimitedField(15, noop_payload));
}

void FakeProtoStreamServer::PushTrailersAndFinish(
    int32_t rpc_status_code,
    std::string_view rpc_status_message) {
  CHECK(producer_handle_.is_valid());
  // StreamBody field 2: `rpc.Status status = 2;`
  std::string encoded_status =
      EncodeRpcStatus(rpc_status_code, rpc_status_message);
  WriteRawBytes(EncodeLengthDelimitedField(2, encoded_status));

  producer_handle_.reset();
  client_remote_->OnComplete(network::URLLoaderCompletionStatus(net::OK));
  client_remote_.FlushForTesting();
  client_remote_.reset();
}

void FakeProtoStreamServer::AbortWithNetworkError(int net_error) {
  producer_handle_.reset();
  if (client_remote_.is_bound()) {
    client_remote_->OnComplete(network::URLLoaderCompletionStatus(net_error));
    client_remote_.FlushForTesting();
    client_remote_.reset();
  }
}

void FakeProtoStreamServer::WriteRawBytes(std::string_view bytes) {
  CHECK_EQ(producer_handle_->WriteAllData(base::as_byte_span(bytes)),
           MOJO_RESULT_OK);
}

}  // namespace browser_actuator
