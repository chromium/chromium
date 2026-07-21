// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_OAUTH_OAUTH_STREAM_CONNECTION_DELEGATE_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_OAUTH_OAUTH_STREAM_CONNECTION_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace browser_actuator {

// StreamConnectionDelegate decorator that authenticates the stream
// connection as the profile's primary GAIA account
// (ConsentLevel::kSignin), wrapping an inner delegate that handles resume
// state.
//
// Flow:
//  * PrepareRequest mints an OAuth2 access token for the primary account
//    and attaches it as an `Authorization: Bearer` header before forwarding
//    to the inner delegate. The fetch uses Mode::kImmediate: if the account
//    or its refresh token is not currently available, the attempt is
//    aborted, and the stream client's backoff schedule drives further
//    tries. The service layer is expected to Disconnect() on signout.
//  * An HTTP 401 rejection invalidates the (possibly stale) cached access
//    token and asks the client to retry, once per fresh token: a second
//    consecutive 401 means the server genuinely rejects this identity, and
//    the connection fails permanently. Permanent failure ends that
//    connection session only — a later Connect() starts over with the same
//    one-retry budget.
//
// `identity_manager` must outlive this delegate (in practice: the delegate
// is owned by a MessageStreamClient owned by a KeyedService depending on
// IdentityManager).
//
// TODO(crbug.com/535696266): Register a dedicated OAuthConsumerId (and its
// OAuth scope) for browser actuation instead of taking one as a constructor
// parameter; requires the server-side scope to be finalized.
class OAuthStreamConnectionDelegate : public StreamConnectionDelegate {
 public:
  OAuthStreamConnectionDelegate(std::unique_ptr<StreamConnectionDelegate> inner,
                                signin::IdentityManager* identity_manager,
                                signin::OAuthConsumerId oauth_consumer_id);
  OAuthStreamConnectionDelegate(const OAuthStreamConnectionDelegate&) = delete;
  OAuthStreamConnectionDelegate& operator=(
      const OAuthStreamConnectionDelegate&) = delete;
  ~OAuthStreamConnectionDelegate() override;

  // StreamConnectionDelegate:
  void OnMessageDispatched(const std::string& message) override;
  void OnConnectionEstablished() override;
  void PrepareRequest(std::unique_ptr<network::ResourceRequest> request,
                      PrepareRequestCallback callback) override;
  bool ShouldRetryOnHttpFailure(int response_code) override;
  std::optional<StreamUploadBody> GetConnectionRequestBody() override;

 private:
  void OnTokenFetched(std::unique_ptr<network::ResourceRequest> request,
                      PrepareRequestCallback callback,
                      GoogleServiceAuthError error,
                      signin::AccessTokenInfo access_token_info);

  const std::unique_ptr<StreamConnectionDelegate> inner_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const signin::OAuthConsumerId oauth_consumer_id_;

  // Live while a token fetch is in flight; destroying it cancels the fetch.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  // The token attached to the current/last attempt, kept for cache
  // invalidation on 401.
  std::string last_access_token_;

  // 401 rejections since the last established connection; drives the
  // retry-once-per-fresh-token policy.
  int consecutive_auth_failures_ = 0;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_OAUTH_OAUTH_STREAM_CONNECTION_DELEGATE_H_
