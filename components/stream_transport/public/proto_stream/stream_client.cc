// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/public/proto_stream/stream_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "components/stream_transport/internal/proto_stream/proto_stream_client.h"
#include "components/stream_transport/internal/proto_stream/resume_body_connection_delegate.h"
#include "components/stream_transport/internal/proto_stream/rust_stream_framer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace stream_transport {

std::unique_ptr<StreamFramer> CreateRustStreamFramer() {
  return std::make_unique<RustStreamFramer>();
}

std::unique_ptr<MessageStreamClient> CreateProtoStreamClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GURL endpoint,
    std::unique_ptr<StreamConnectionDelegate> delegate,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    ProtoStreamClientConfig config) {
  return CreateProtoStreamClient(
      std::move(url_loader_factory), std::move(endpoint), std::move(delegate),
      base::BindRepeating(&CreateRustStreamFramer), traffic_annotation, config);
}

std::unique_ptr<MessageStreamClient> CreateProtoStreamClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GURL endpoint,
    std::unique_ptr<StreamConnectionDelegate> delegate,
    StreamFramerFactory framer_factory,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    ProtoStreamClientConfig config) {
  return std::make_unique<ProtoStreamClient>(
      std::move(url_loader_factory), std::move(endpoint), std::move(delegate),
      std::move(framer_factory), traffic_annotation,
      config.base_reconnection_time, config.max_reconnection_time,
      config.stall_timeout);
}

std::unique_ptr<StreamConnectionDelegate> CreateResumeBodyConnectionDelegate(
    base::RepeatingCallback<std::string()> body_provider,
    std::unique_ptr<StreamConnectionDelegate> inner) {
  if (!inner) {
    inner = std::make_unique<DefaultStreamConnectionDelegate>();
  }
  return std::make_unique<ResumeBodyConnectionDelegate>(
      std::move(body_provider), std::move(inner));
}

}  // namespace stream_transport
