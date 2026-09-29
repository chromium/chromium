// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TEST_SUPPORT_FAKE_PROTO_STREAM_SERVER_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TEST_SUPPORT_FAKE_PROTO_STREAM_SERVER_H_

#include <cstdint>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "url/gurl.h"

namespace browser_actuator {

// Simulates a server-streaming protobuf endpoint (`StreamBody` framing) backed
// by a `network::TestURLLoaderFactory`.
//
// Intercepts stream requests targeting `endpoint`, parses the request body into
// `WatchSessionsRequest`, holds the response body pipe open across multiple
// `PushMessage()` / `PushKeepAlive()` calls, and terminates the stream with
// `PushTrailersAndFinish()`.
//
// Each `Push*()` call encodes a self-contained outer `StreamBody` message onto
// the response data pipe using protobuf wire tags matching
// `RustStreamFramer`'s streaming decoder (`field 1` = message bytes,
// `field 2` = `rpc.Status` trailers, `field 15` = `noop` keep-alive).
class FakeProtoStreamServer {
 public:
  FakeProtoStreamServer(network::TestURLLoaderFactory* url_loader_factory,
                        const GURL& endpoint);
  ~FakeProtoStreamServer();

  FakeProtoStreamServer(const FakeProtoStreamServer&) = delete;
  FakeProtoStreamServer& operator=(const FakeProtoStreamServer&) = delete;

  // Blocks until `ProtoStreamClient` issues a stream request to `endpoint_`
  // and automatically responds with HTTP 200 OK headers and an open Mojo data
  // pipe.
  void WaitForConnection();

  // Encodes `message` into `StreamBody.message` (field 1, wire tag `0x0a`) and
  // writes the chunk to the active stream pipe.
  void PushMessage(const ActuatorDownstreamMessage& message);

  // Encodes `noop_payload` into `StreamBody.noop` (field 15, wire tag `0x7a`)
  // and writes the chunk to the active stream pipe.
  void PushKeepAlive(std::string_view noop_payload = "ping");

  // Encodes `rpc.Status` trailers (`code`, `message`) into `StreamBody.status`
  // (field 2, wire tag `0x12`), writes the chunk, closes the producer pipe, and
  // signals `OnComplete(net::OK)` to the `URLLoaderClient`.
  void PushTrailersAndFinish(int32_t rpc_status_code,
                             std::string_view rpc_status_message = "");

  // Closes the stream with a network-level error without sending trailers.
  void AbortWithNetworkError(int net_error);

  bool is_connected() const { return producer_handle_.is_valid(); }

  int connection_count() const { return connection_count_; }

  const WatchSessionsRequest& last_watch_request() const {
    return last_watch_request_;
  }

  const network::ResourceRequest& last_resource_request() const {
    return last_resource_request_;
  }

 private:
  void WriteRawBytes(std::string_view bytes);

  const raw_ptr<network::TestURLLoaderFactory> url_loader_factory_;
  const GURL endpoint_;

  int connection_count_ = 0;
  network::ResourceRequest last_resource_request_;
  WatchSessionsRequest last_watch_request_;

  mojo::Remote<network::mojom::URLLoaderClient> client_remote_;
  mojo::ScopedDataPipeProducerHandle producer_handle_;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TEST_SUPPORT_FAKE_PROTO_STREAM_SERVER_H_
