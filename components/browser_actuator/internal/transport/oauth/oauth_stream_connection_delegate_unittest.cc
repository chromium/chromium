// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/oauth/oauth_stream_connection_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"
#include "components/browser_actuator/internal/transport/test_support/mock_stream_connection_delegate.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

// Any registered consumer with static scopes works for tests; a dedicated
// browser actuation consumer id does not exist yet (see the TODO on
// OAuthStreamConnectionDelegate and crbug.com/535696266).
constexpr signin::OAuthConsumerId kTestConsumerId =
    signin::OAuthConsumerId::kFeedNetwork;

// Header the mock inner delegate adds in PrepareRequest, standing in for
// the resume-state decoration a production inner delegate performs.
constexpr char kInnerHeader[] = "X-Inner-Decorated";

class OAuthStreamConnectionDelegateTest
    : public testing::Test,
      public signin::IdentityManager::DiagnosticsObserver {
 protected:
  OAuthStreamConnectionDelegateTest() {
    identity_test_env_.identity_manager()->AddDiagnosticsObserver(this);
  }

  ~OAuthStreamConnectionDelegateTest() override {
    identity_test_env_.identity_manager()->RemoveDiagnosticsObserver(this);
  }

  // signin::IdentityManager::DiagnosticsObserver:
  // IdentityTestEnvironment's fake token service has no real token cache,
  // so invalidation is only observable through this notification.
  void OnAccessTokenRemovedFromCache(const CoreAccountId& account_id,
                                     const signin::ScopeSet& scopes) override {
    ++tokens_removed_from_cache_;
  }

  std::unique_ptr<OAuthStreamConnectionDelegate> MakeDelegate(
      std::unique_ptr<StreamConnectionDelegate> inner =
          std::make_unique<DefaultStreamConnectionDelegate>()) {
    return std::make_unique<OAuthStreamConnectionDelegate>(
        std::move(inner), identity_test_env_.identity_manager(),
        kTestConsumerId);
  }

  // PrepareRequest completes asynchronously with the prepared request, or
  // nullptr when the attempt aborts.
  using RequestFuture =
      base::test::TestFuture<std::unique_ptr<network::ResourceRequest>>;

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  int tokens_removed_from_cache_ = 0;
};

TEST_F(OAuthStreamConnectionDelegateTest, AttachesBearerTokenAndForwardsInner) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  auto inner =
      std::make_unique<testing::StrictMock<MockStreamConnectionDelegate>>();
  MockStreamConnectionDelegate& inner_ref = *inner;
  auto delegate = MakeDelegate(std::move(inner));

  // Dispatched messages reach the inner delegate through the decorator.
  EXPECT_CALL(inner_ref, OnMessageDispatched("message-payload"));
  delegate->OnMessageDispatched("message-payload");

  // The inner delegate receives the request after the bearer token is
  // attached and still gets to decorate it (e.g. with resume state).
  EXPECT_CALL(inner_ref, PrepareRequest)
      .WillOnce([](std::unique_ptr<network::ResourceRequest> request,
                   StreamConnectionDelegate::PrepareRequestCallback callback) {
        EXPECT_TRUE(request->headers.HasHeader(
            net::HttpRequestHeaders::kAuthorization));
        request->headers.SetHeader(kInnerHeader, "inner-was-here");
        std::move(callback).Run(std::move(request));
      });

  RequestFuture future;
  delegate->PrepareRequest(std::make_unique<network::ResourceRequest>(),
                           future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access-token", base::Time::Now() + base::Hours(1));

  std::unique_ptr<network::ResourceRequest> request = future.Take();
  ASSERT_TRUE(request);
  EXPECT_EQ(request->headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            "Bearer access-token");
  EXPECT_EQ(request->headers.GetHeader(kInnerHeader), "inner-was-here");
}

TEST_F(OAuthStreamConnectionDelegateTest, AbortsWithoutPrimaryAccount) {
  // The StrictMock inner pins that an aborted attempt never reaches the
  // inner delegate.
  auto delegate = MakeDelegate(
      std::make_unique<testing::StrictMock<MockStreamConnectionDelegate>>());

  // Mode::kImmediate: no signed-in account means the attempt aborts without
  // waiting for sign-in; the client's backoff schedule owns the retry
  // cadence.
  RequestFuture future;
  delegate->PrepareRequest(std::make_unique<network::ResourceRequest>(),
                           future.GetCallback());
  EXPECT_FALSE(future.Take());
}

TEST_F(OAuthStreamConnectionDelegateTest, AbortsOnTokenError) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  // The StrictMock inner pins that an aborted attempt never reaches the
  // inner delegate.
  auto delegate = MakeDelegate(
      std::make_unique<testing::StrictMock<MockStreamConnectionDelegate>>());

  RequestFuture future;
  delegate->PrepareRequest(std::make_unique<network::ResourceRequest>(),
                           future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromServiceError(""));
  EXPECT_FALSE(future.Take());
}

TEST_F(OAuthStreamConnectionDelegateTest, RetriesOnceOn401ThenFails) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  auto delegate = MakeDelegate();

  RequestFuture future;
  delegate->PrepareRequest(std::make_unique<network::ResourceRequest>(),
                           future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "stale-token", base::Time::Now() + base::Hours(1));
  ASSERT_TRUE(future.Take());

  // First 401: the stale token is invalidated, and a retry is requested.
  EXPECT_TRUE(delegate->ShouldRetryOnHttpFailure(net::HTTP_UNAUTHORIZED));
  // A second consecutive 401 means the identity is genuinely rejected.
  EXPECT_FALSE(delegate->ShouldRetryOnHttpFailure(net::HTTP_UNAUTHORIZED));
}

TEST_F(OAuthStreamConnectionDelegateTest, GiveUpDoesNotPoisonNextSession) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  auto delegate = MakeDelegate();

  // The first connection session exhausts its retry budget: one retry,
  // then permanent failure, which stops the client.
  EXPECT_TRUE(delegate->ShouldRetryOnHttpFailure(net::HTTP_UNAUTHORIZED));
  EXPECT_FALSE(delegate->ShouldRetryOnHttpFailure(net::HTTP_UNAUTHORIZED));

  // Connect() after a permanent failure starts over, and the delegate must
  // start over with it: the next session's first 401 gets the documented
  // retry, not an immediate second permanent failure.
  EXPECT_TRUE(delegate->ShouldRetryOnHttpFailure(net::HTTP_UNAUTHORIZED));
  EXPECT_FALSE(delegate->ShouldRetryOnHttpFailure(net::HTTP_UNAUTHORIZED));
}

TEST_F(OAuthStreamConnectionDelegateTest,
       EstablishedConnectionResetsAuthRetry) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  auto inner =
      std::make_unique<testing::StrictMock<MockStreamConnectionDelegate>>();
  MockStreamConnectionDelegate& inner_ref = *inner;
  auto delegate = MakeDelegate(std::move(inner));

  EXPECT_TRUE(delegate->ShouldRetryOnHttpFailure(net::HTTP_UNAUTHORIZED));
  // The notification resets the failure streak and reaches the inner
  // delegate.
  EXPECT_CALL(inner_ref, OnConnectionEstablished());
  delegate->OnConnectionEstablished();
  // The failure streak reset, so a later 401 gets a fresh retry again.
  EXPECT_TRUE(delegate->ShouldRetryOnHttpFailure(net::HTTP_UNAUTHORIZED));
}

TEST_F(OAuthStreamConnectionDelegateTest, ForwardsNon401FailuresToInner) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  auto inner =
      std::make_unique<testing::StrictMock<MockStreamConnectionDelegate>>();
  MockStreamConnectionDelegate& inner_ref = *inner;
  auto delegate = MakeDelegate(std::move(inner));

  // Non-401 failures are the inner delegate's decision: both the call and
  // its answer, in either direction, must pass through the decorator.
  EXPECT_CALL(inner_ref, ShouldRetryOnHttpFailure(net::HTTP_NOT_FOUND))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(delegate->ShouldRetryOnHttpFailure(net::HTTP_NOT_FOUND));
  EXPECT_CALL(inner_ref, ShouldRetryOnHttpFailure(net::HTTP_OK))
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(delegate->ShouldRetryOnHttpFailure(net::HTTP_OK));

  // A 401 is this decorator's own decision; the StrictMock inner fails the
  // test if it is consulted.
  EXPECT_TRUE(delegate->ShouldRetryOnHttpFailure(net::HTTP_UNAUTHORIZED));
}

TEST_F(OAuthStreamConnectionDelegateTest, MintsFreshTokenAfter401) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  auto delegate = MakeDelegate();

  RequestFuture first_future;
  delegate->PrepareRequest(std::make_unique<network::ResourceRequest>(),
                           first_future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "stale-token", base::Time::Now() + base::Hours(1));
  ASSERT_TRUE(first_future.Take());
  ASSERT_TRUE(delegate->ShouldRetryOnHttpFailure(net::HTTP_UNAUTHORIZED));
  // The rejected token was dropped from the cache, so the retry's fetch
  // cannot be served the same token again. (The fake token service never
  // caches, so the cache miss itself can't be observed here — the
  // diagnostics notification pins that the invalidation happened.)
  EXPECT_EQ(tokens_removed_from_cache_, 1);

  RequestFuture future;
  delegate->PrepareRequest(std::make_unique<network::ResourceRequest>(),
                           future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "fresh-token", base::Time::Now() + base::Hours(1));

  std::unique_ptr<network::ResourceRequest> request = future.Take();
  ASSERT_TRUE(request);
  EXPECT_EQ(request->headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            "Bearer fresh-token");

  // The budget is one retry per session even across the fresh mint: a 401
  // against the fresh token is the permanent failure — and that rejected
  // token is invalidated too, so it cannot leak into the next session.
  EXPECT_FALSE(delegate->ShouldRetryOnHttpFailure(net::HTTP_UNAUTHORIZED));
  EXPECT_EQ(tokens_removed_from_cache_, 2);
}

// A delegate that always offers an upload body, to check that the OAuth
// decorator forwards it to the inner delegate unchanged.
class BodyProvidingDelegate : public StreamConnectionDelegate {
 public:
  void PrepareRequest(std::unique_ptr<network::ResourceRequest> request,
                      PrepareRequestCallback callback) override {
    std::move(callback).Run(std::move(request));
  }
  std::optional<StreamUploadBody> GetConnectionRequestBody() override {
    return StreamUploadBody{"inner-watch-body", "application/x-protobuf"};
  }
};

TEST_F(OAuthStreamConnectionDelegateTest, ForwardsInnerConnectionRequestBody) {
  auto delegate = MakeDelegate(std::make_unique<BodyProvidingDelegate>());

  std::optional<StreamUploadBody> body = delegate->GetConnectionRequestBody();
  ASSERT_TRUE(body);
  EXPECT_EQ(body->content, "inner-watch-body");
  EXPECT_EQ(body->content_type, "application/x-protobuf");
}

}  // namespace
}  // namespace browser_actuator
