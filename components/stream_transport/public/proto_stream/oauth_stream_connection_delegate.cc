// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/public/proto_stream/oauth_stream_connection_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "components/stream_transport/internal/proto_stream/oauth_stream_connection_delegate.h"

namespace stream_transport {

std::unique_ptr<StreamConnectionDelegate> CreateOAuthStreamConnectionDelegate(
    std::unique_ptr<StreamConnectionDelegate> inner,
    signin::IdentityManager* identity_manager,
    signin::OAuthConsumerId oauth_consumer_id,
    std::string token_fetch_latency_histogram) {
  if (!inner) {
    inner = std::make_unique<DefaultStreamConnectionDelegate>();
  }
  return std::make_unique<OAuthStreamConnectionDelegate>(
      std::move(inner), identity_manager, oauth_consumer_id,
      std::move(token_fetch_latency_histogram));
}

}  // namespace stream_transport
