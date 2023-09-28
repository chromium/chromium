// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/google_url_loader_throttle.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/signin/public/base/signin_switches.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

// This file only contains tests relevant to the bound session credentials
// feature.
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/common/bound_session_request_throttled_listener.h"

namespace {

enum class RequestAction { kWillStartRequest, kWillRedirectRequest };

class FakeBoundSessionRequestThrottledListener
    : public BoundSessionRequestThrottledListener {
 public:
  void OnRequestBlockedOnCookie(
      ResumeOrCancelThrottledRequestCallback callback) override {
    EXPECT_FALSE(callback_);
    callback_ = std::move(callback);
  }

  void SimulateOnRequestBlockedOnCookieCompleted(UnblockAction unblock_action) {
    std::move(callback_).Run(unblock_action);
  }

  bool IsRequestBlocked() { return !callback_.is_null(); }

 private:
  ResumeOrCancelThrottledRequestCallback callback_;
};

class MockThrottleDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  MockThrottleDelegate() = default;
  MOCK_METHOD(void, CancelWithError, (int, base::StringPiece), (override));
  MOCK_METHOD(void, Resume, (), (override));
};

class GoogleURLLoaderThrottleTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<RequestAction> {
 public:
  const GURL kTestGoogleURL = GURL("https://google.com");
  const GURL kGoogleSubdomainURL = GURL("https://accounts.google.com");

  GoogleURLLoaderThrottleTest() = default;
  ~GoogleURLLoaderThrottleTest() override = default;

  void ConfigureBoundSessionThrottlerParams(const std::string& domain,
                                            const std::string& path,
                                            base::Time expiration_date) {
    bound_session_throttler_params_ =
        chrome::mojom::BoundSessionThrottlerParams::New(domain, path,
                                                        expiration_date);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  FakeBoundSessionRequestThrottledListener* bound_session_listener() {
    return bound_session_listener_.get();
  }

  GoogleURLLoaderThrottle* throttle() {
    if (!throttle_) {
      CreateThrottle();
    }
    return throttle_.get();
  }

  MockThrottleDelegate* delegate() { return delegate_.get(); }

  void CallThrottleAndVerifyDeferExpectation(bool expect_defer,
                                             const GURL& url) {
    bool defer = false;
    switch (GetParam()) {
      case RequestAction::kWillStartRequest: {
        network::ResourceRequest request;
        request.url = url;
        throttle()->WillStartRequest(&request, &defer);
        break;
      }
      case RequestAction::kWillRedirectRequest: {
        net::RedirectInfo redirect_info;
        redirect_info.new_url = url;
        network::mojom::URLResponseHead response_head;
        std::vector<std::string> to_be_removed_headers;
        net::HttpRequestHeaders modified_headers;
        net::HttpRequestHeaders modified_cors_exempt_headers;
        throttle()->WillRedirectRequest(
            &redirect_info, response_head, &defer, &to_be_removed_headers,
            &modified_headers, &modified_cors_exempt_headers);
        break;
      }
    }
    EXPECT_EQ(expect_defer, defer);
    EXPECT_EQ(expect_defer, bound_session_listener()->IsRequestBlocked());
  }

  void UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledListener::UnblockAction unblock_action) {
    switch (unblock_action) {
      case BoundSessionRequestThrottledListener::UnblockAction::kResume:
        EXPECT_CALL(*delegate(), Resume());
        break;
      case BoundSessionRequestThrottledListener::UnblockAction::kCancel:
        EXPECT_CALL(*delegate(), CancelWithError(net::ERR_ABORTED, testing::_));
        break;
    }

    bound_session_listener_->SimulateOnRequestBlockedOnCookieCompleted(
        unblock_action);

    RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(delegate());
    histogram_tester_->ExpectTotalCount(
        "Signin.BoundSessionCredentials.DeferredRequestDelay",
        /*expected_count=*/1);
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

 private:
  void CreateThrottle() {
    chrome::mojom::DynamicParamsPtr dynamic_params(
        chrome::mojom::DynamicParams::New());
    dynamic_params->bound_session_throttler_params =
        bound_session_throttler_params_.Clone();

    std::unique_ptr<FakeBoundSessionRequestThrottledListener>
        bound_session_listener =
            std::make_unique<FakeBoundSessionRequestThrottledListener>();
    bound_session_listener_ = bound_session_listener.get();
    delegate_ = std::make_unique<MockThrottleDelegate>();

    throttle_ = std::make_unique<GoogleURLLoaderThrottle>(
#if BUILDFLAG(IS_ANDROID)
        "",
#endif
        std::move(bound_session_listener), std::move(dynamic_params));
    throttle_->set_delegate(delegate_.get());
  }

  base::test::ScopedFeatureList feature_list_{
      switches::kEnableBoundSessionCredentials};
  base::test::TaskEnvironment task_environment_;
  raw_ptr<FakeBoundSessionRequestThrottledListener, DanglingUntriaged>
      bound_session_listener_ = nullptr;
  std::unique_ptr<GoogleURLLoaderThrottle> throttle_;
  std::unique_ptr<MockThrottleDelegate> delegate_;
  chrome::mojom::BoundSessionThrottlerParamsPtr bound_session_throttler_params_;
  std::unique_ptr<base::HistogramTester> histogram_tester_ =
      std::make_unique<base::HistogramTester>();
};

TEST_P(GoogleURLLoaderThrottleTest, NullBoundSessionThrottlerParams) {
  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/false, kTestGoogleURL);
}

TEST_P(GoogleURLLoaderThrottleTest, EmptyBoundSessionThrottlerParams) {
  ConfigureBoundSessionThrottlerParams("", "", base::Time::Now());
  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/false, kGoogleSubdomainURL);
}

TEST_P(GoogleURLLoaderThrottleTest, NoInterceptBoundSessionCookieFresh) {
  ConfigureBoundSessionThrottlerParams("google.com", "/",
                                       base::Time::Now() + base::Minutes(10));
  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/false, kGoogleSubdomainURL);
}

TEST_P(GoogleURLLoaderThrottleTest, NoInterceptDomainNotInBoundSession) {
  ConfigureBoundSessionThrottlerParams("google.com", "/", base::Time::Min());
  CallThrottleAndVerifyDeferExpectation(/*expect_defer=*/false,
                                        GURL("https://youtube.com"));
}

TEST_P(GoogleURLLoaderThrottleTest, NoInterceptPathNotInBoundSession) {
  ConfigureBoundSessionThrottlerParams("google.com", "/test",
                                       base::Time::Min());
  CallThrottleAndVerifyDeferExpectation(/*expect_defer=*/false, kTestGoogleURL);
}

TEST_F(GoogleURLLoaderThrottleTest, NoInterceptRequestWithSendCookiesFalse) {
  ConfigureBoundSessionThrottlerParams("google.com", "/", base::Time::Min());
  bool defer = false;
  network::ResourceRequest request;
  request.url = kTestGoogleURL;
  request.credentials_mode = network::mojom::CredentialsMode::kOmit;
  throttle()->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(bound_session_listener()->IsRequestBlocked());
}

TEST_P(GoogleURLLoaderThrottleTest, InterceptBoundSessionCookieExpired) {
  ConfigureBoundSessionThrottlerParams("google.com", "/",
                                       base::Time::Now() - base::Minutes(10));
  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/true, GURL("https://accounts.google.com/test/bar.html"));
  UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledListener::UnblockAction::kResume);
}

TEST_P(GoogleURLLoaderThrottleTest,
       InterceptBoundSessionCookieExpiredCookieDomainWithLeadingDot) {
  ConfigureBoundSessionThrottlerParams(".google.com", "/",
                                       base::Time::Now() - base::Minutes(10));
  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/true, GURL("https://google.com/"));
  UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledListener::UnblockAction::kResume);
}

TEST_P(GoogleURLLoaderThrottleTest,
       InterceptBoundSessionCookieExpiredCookieDomainWithoutLeadingDot) {
  ConfigureBoundSessionThrottlerParams("google.com", "/",
                                       base::Time::Now() - base::Minutes(10));
  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/true, GURL("https://google.com/"));
  UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledListener::UnblockAction::kResume);
}

TEST_P(GoogleURLLoaderThrottleTest, InterceptBoundSessionCookieExpiresNow) {
  ConfigureBoundSessionThrottlerParams("google.com", "/", base::Time::Now());

  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/true, kGoogleSubdomainURL);
  UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledListener::UnblockAction::kResume);
}

TEST_P(GoogleURLLoaderThrottleTest, InterceptBoundSessionPathEmpty) {
  ConfigureBoundSessionThrottlerParams("google.com", "", base::Time::Now());

  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/true, kGoogleSubdomainURL);
  UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledListener::UnblockAction::kResume);
}

TEST_P(GoogleURLLoaderThrottleTest,
       NoInterceptRequestURLNotOnBoundSessionPath) {
  ConfigureBoundSessionThrottlerParams("google.com", "/test",
                                       base::Time::Now());
  CallThrottleAndVerifyDeferExpectation(/*expect_defer=*/false, kTestGoogleURL);
}

TEST_P(GoogleURLLoaderThrottleTest,
       InterceptRequestURLWithPrefixBoundSessionPath) {
  ConfigureBoundSessionThrottlerParams("google.com", "/test",
                                       base::Time::Now());
  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/true,
      GURL("https://accounts.google.com/test/foo/bar.html"));
  UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledListener::UnblockAction::kResume);
}

TEST_P(GoogleURLLoaderThrottleTest, InterceptAndCancelRequest) {
  ConfigureBoundSessionThrottlerParams("google.com", "/", base::Time::Now());

  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/true, kGoogleSubdomainURL);
  UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledListener::UnblockAction::kCancel);
}

INSTANTIATE_TEST_SUITE_P(WillStartRequest,
                         GoogleURLLoaderThrottleTest,
                         ::testing::Values(RequestAction::kWillStartRequest));

INSTANTIATE_TEST_SUITE_P(
    WillRedirectRequest,
    GoogleURLLoaderThrottleTest,
    ::testing::Values(RequestAction::kWillRedirectRequest));

}  // namespace

#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
