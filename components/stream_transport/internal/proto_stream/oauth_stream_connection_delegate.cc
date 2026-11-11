// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/internal/proto_stream/oauth_stream_connection_delegate.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"

namespace stream_transport {

namespace {

// One retry with a freshly minted token; if that also gets rejected, the
// server really means it.
constexpr int kMaxConsecutiveAuthFailures = 2;

}  // namespace

OAuthStreamConnectionDelegate::OAuthStreamConnectionDelegate(
    std::unique_ptr<StreamConnectionDelegate> inner,
    signin::IdentityManager* identity_manager,
    signin::OAuthConsumerId oauth_consumer_id,
    std::string token_fetch_latency_histogram)
    : inner_(std::move(inner)),
      identity_manager_(identity_manager),
      oauth_consumer_id_(oauth_consumer_id),
      token_fetch_latency_histogram_(std::move(token_fetch_latency_histogram)) {
  CHECK(inner_);
  CHECK(identity_manager_);
}

OAuthStreamConnectionDelegate::~OAuthStreamConnectionDelegate() = default;

void OAuthStreamConnectionDelegate::OnMessageDispatched(
    const std::string& message) {
  inner_->OnMessageDispatched(message);
}

void OAuthStreamConnectionDelegate::OnConnectionEstablished() {
  consecutive_auth_failures_ = 0;
  inner_->OnConnectionEstablished();
}

std::optional<StreamUploadBody>
OAuthStreamConnectionDelegate::GetConnectionRequestBody() {
  // Auth adds no body of its own; pass through whatever the inner delegate
  // wants to upload.
  return inner_->GetConnectionRequestBody();
}

void OAuthStreamConnectionDelegate::PrepareRequest(
    std::unique_ptr<network::ResourceRequest> request,
    PrepareRequestCallback callback) {
  // Mode::kImmediate: if the primary account (or its refresh token) is not
  // available right now, fail this attempt instead of waiting — the stream
  // client's backoff schedule provides the retry cadence, and the service
  // layer tears the client down on signout.
  // base::Unretained is safe: `this` owns `token_fetcher_`, which cancels the
  // fetch and callback upon destruction.
  CoreAccountId account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      oauth_consumer_id_, identity_manager_,
      base::BindOnce(&OAuthStreamConnectionDelegate::OnTokenFetched,
                     base::Unretained(this), base::TimeTicks::Now(),
                     std::move(account_id), std::move(request),
                     std::move(callback)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSignin);
}

bool OAuthStreamConnectionDelegate::ShouldRetryOnHttpFailure(
    int response_code) {
  if (response_code != net::HTTP_UNAUTHORIZED) {
    return inner_->ShouldRetryOnHttpFailure(response_code);
  }
  // The server rejected the token this connection attempt used; drop it
  // from the cache unconditionally — access tokens live for about an hour,
  // and a cached rejected token would otherwise be served again to every
  // attempt in that window, including a later Connect() session's first
  // one.
  if (!last_access_token_.empty()) {
    identity_manager_->RemoveAccessTokenFromCache(
        last_token_account_id_, oauth_consumer_id_, last_access_token_);
    last_access_token_.clear();
    last_token_account_id_ = CoreAccountId();
  }
  if (++consecutive_auth_failures_ >= kMaxConsecutiveAuthFailures) {
    // Permanent failure: the client stops, so the next 401 can only belong
    // to a later Connect() session, which per MessageStreamClient::Connect()
    // starts over — give it the full retry budget back.
    consecutive_auth_failures_ = 0;
    return false;
  }
  return true;
}

void OAuthStreamConnectionDelegate::OnTokenFetched(
    base::TimeTicks start_time,
    CoreAccountId account_id,
    std::unique_ptr<network::ResourceRequest> request,
    PrepareRequestCallback callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(callback).Run(nullptr);
    return;
  }
  // Only successful fetches are recorded: Mode::kImmediate failures (e.g. no
  // primary account) complete near-instantly and would skew the distribution.
  if (!token_fetch_latency_histogram_.empty()) {
    base::UmaHistogramTimes(token_fetch_latency_histogram_,
                            base::TimeTicks::Now() - start_time);
  }
  last_token_account_id_ = std::move(account_id);
  last_access_token_ = access_token_info.token;
  request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({"Bearer ", access_token_info.token}));
  inner_->PrepareRequest(std::move(request), std::move(callback));
}

}  // namespace stream_transport
