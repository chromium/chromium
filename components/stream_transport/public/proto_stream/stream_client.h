// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_PUBLIC_PROTO_STREAM_STREAM_CLIENT_H_
#define COMPONENTS_STREAM_TRANSPORT_PUBLIC_PROTO_STREAM_STREAM_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/stream_transport/public/proto_stream/message_stream_client.h"
#include "components/stream_transport/public/proto_stream/stream_connection_delegate.h"
#include "components/stream_transport/public/proto_stream/stream_framer.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace stream_transport {

// Timing configuration for a `MessageStreamClient` created by
// `CreateProtoStreamClient()`.
struct ProtoStreamClientConfig {
  // Wait before the first reconnect attempt after a failed or dropped stream.
  // Each further consecutive failure doubles the wait up to
  // `max_reconnection_time`.
  base::TimeDelta base_reconnection_time = base::Seconds(3);

  // Upper bound on exponential reconnection backoff.
  base::TimeDelta max_reconnection_time = base::Minutes(5);

  // How long an open stream may go without receiving any bytes (including
  // server-sent `StreamBody.noop` keep-alive frames) before it is treated as
  // half-open, torn down, and reconnected. A zero or negative duration
  // disables the watchdog.
  base::TimeDelta stall_timeout = base::Minutes(2);
};

// Creates a `StreamFramer` that incrementally parses OnePlatform `alt=proto`
// `google.rpc.StreamBody` framing in memory-safe Rust
// (`#![forbid(unsafe_code)]` per Chromium's Rule of 2).
std::unique_ptr<StreamFramer> CreateRustStreamFramer();

// Creates a `MessageStreamClient` for a OnePlatform-style server-streaming RPC
// over `alt=proto` (`Content-Type: application/x-protobuf`), framing incoming
// response chunks with `CreateRustStreamFramer()`.
//
// `endpoint` must be a valid, potentially trustworthy URL (typically `https:`
// or `http://localhost` for local testing) carrying `alt=proto`. `delegate`
// must be non-null; pass `std::make_unique<DefaultStreamConnectionDelegate>()`
// or a composed chain of `CreateOAuthStreamConnectionDelegate()` and
// `CreateResumeBodyConnectionDelegate()`.
//
// All methods on the returned client must be called on the sequence that
// created it.
std::unique_ptr<MessageStreamClient> CreateProtoStreamClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GURL endpoint,
    std::unique_ptr<StreamConnectionDelegate> delegate,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    ProtoStreamClientConfig config = {});

// Overload accepting a custom `framer_factory`, minted once per established
// HTTP 200 stream connection. Both `delegate` and `framer_factory` must be
// non-null.
std::unique_ptr<MessageStreamClient> CreateProtoStreamClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GURL endpoint,
    std::unique_ptr<StreamConnectionDelegate> delegate,
    StreamFramerFactory framer_factory,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    ProtoStreamClientConfig config = {});

// Creates a `StreamConnectionDelegate` decorator that invokes `body_provider`
// on each connection or reconnection attempt and attaches the returned
// serialized protobuf bytes (`Content-Type: application/x-protobuf`) as an HTTP
// `POST` request body.
//
// `body_provider` must be non-null. If `inner` is null, a
// `DefaultStreamConnectionDelegate` is used as the inner delegate.
std::unique_ptr<StreamConnectionDelegate> CreateResumeBodyConnectionDelegate(
    base::RepeatingCallback<std::string()> body_provider,
    std::unique_ptr<StreamConnectionDelegate> inner = nullptr);

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_PUBLIC_PROTO_STREAM_STREAM_CLIENT_H_
