// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_PUBLIC_PROTO_STREAM_OAUTH_STREAM_CONNECTION_DELEGATE_H_
#define COMPONENTS_STREAM_TRANSPORT_PUBLIC_PROTO_STREAM_OAUTH_STREAM_CONNECTION_DELEGATE_H_

#include <memory>
#include <string>

#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/stream_transport/public/proto_stream/stream_connection_delegate.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace stream_transport {

// Creates a `StreamConnectionDelegate` decorator that authenticates each stream
// connection attempt as the profile's primary GAIA account
// (`signin::ConsentLevel::kSignin`) using `oauth_consumer_id`.
//
// On `PrepareRequest()`, fetches an OAuth2 access token in
// `PrimaryAccountAccessTokenFetcher::Mode::kImmediate` and attaches
// `Authorization: Bearer <token>` before forwarding to `inner`. On HTTP 401,
// invalidates the rejected token in `identity_manager` and requests one retry
// per fresh token.
//
// `identity_manager` must be non-null and must outlive the returned delegate.
// If `inner` is null, a `DefaultStreamConnectionDelegate` is used as the inner
// delegate. If `token_fetch_latency_histogram` is non-empty, the duration of
// each successful access-token fetch is recorded to that `UmaHistogramTimes`
// metric.
std::unique_ptr<StreamConnectionDelegate> CreateOAuthStreamConnectionDelegate(
    std::unique_ptr<StreamConnectionDelegate> inner,
    signin::IdentityManager* identity_manager,
    signin::OAuthConsumerId oauth_consumer_id,
    std::string token_fetch_latency_histogram = "");

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_PUBLIC_PROTO_STREAM_OAUTH_STREAM_CONNECTION_DELEGATE_H_
