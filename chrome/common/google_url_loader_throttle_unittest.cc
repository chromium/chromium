// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/google_url_loader_throttle.h"

#include <memory>
#include <string_view>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/safe_search_api/safe_search_util.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/common/bound_session_request_throttled_handler.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

namespace {

class MockThrottleDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  MockThrottleDelegate() = default;
  MOCK_METHOD(void, CancelWithError, (int, std::string_view), (override));
  MOCK_METHOD(void, Resume, (), (override));
};

class GoogleURLLoaderThrottleTest : public ::testing::Test {
 public:
  const GURL kTestGoogleURL = GURL("https://google.com");
  const GURL kGoogleSubdomainURL = GURL("https://accounts.google.com");

  GoogleURLLoaderThrottleTest() = default;
  ~GoogleURLLoaderThrottleTest() override = default;

  void SetAllowedDomainsForApps(const std::string& allowed_domains) {
    allowed_domains_for_apps_ = allowed_domains;
  }

  GoogleURLLoaderThrottle* throttle() {
    if (!throttle_) {
      CreateThrottle();
    }
    return throttle_.get();
  }

  MockThrottleDelegate* delegate() { return delegate_.get(); }

 protected:
  virtual void CreateThrottle() {
    chrome::mojom::DynamicParamsPtr dynamic_params(
        chrome::mojom::DynamicParams::New());
    dynamic_params->allowed_domains_for_apps = allowed_domains_for_apps_;

    delegate_ = std::make_unique<MockThrottleDelegate>();

    throttle_ = std::make_unique<GoogleURLLoaderThrottle>(
#if BUILDFLAG(IS_ANDROID)
        "",
#endif
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
        /*bound_session_request_throttled_handler=*/nullptr,
#endif
        std::move(dynamic_params));
    throttle_->set_delegate(delegate_.get());
  }

  // Must outlive `throttle_`.
  std::unique_ptr<MockThrottleDelegate> delegate_;
  std::unique_ptr<GoogleURLLoaderThrottle> throttle_;
  std::string allowed_domains_for_apps_;
};

TEST_F(GoogleURLLoaderThrottleTest,
       AllowedDomainsForAppsWillStartRequestGoogleDomain) {
  SetAllowedDomainsForApps("example.com");
  network::ResourceRequest request;
  request.url = kTestGoogleURL;
  bool defer = false;
  throttle()->WillStartRequest(&request, &defer);

  EXPECT_EQ(request.cors_exempt_headers.GetHeader(
                safe_search_api::kGoogleAppsAllowedDomains),
            "example.com");
}

TEST_F(GoogleURLLoaderThrottleTest,
       AllowedDomainsForAppsWillStartRequestGoogleSubdomain) {
  SetAllowedDomainsForApps("example.com");
  network::ResourceRequest request;
  request.url = kGoogleSubdomainURL;
  bool defer = false;
  throttle()->WillStartRequest(&request, &defer);

  EXPECT_EQ(request.cors_exempt_headers.GetHeader(
                safe_search_api::kGoogleAppsAllowedDomains),
            "example.com");
}

TEST_F(GoogleURLLoaderThrottleTest,
       AllowedDomainsForAppsWillStartRequestNonGoogleDomain) {
  SetAllowedDomainsForApps("example.com");
  network::ResourceRequest request;
  request.url = GURL("https://example.com");
  bool defer = false;
  throttle()->WillStartRequest(&request, &defer);

  EXPECT_FALSE(request.cors_exempt_headers.HasHeader(
      safe_search_api::kGoogleAppsAllowedDomains));
}

TEST_F(GoogleURLLoaderThrottleTest,
       AllowedDomainsForAppsWillRedirectRequestToGoogleDomain) {
  SetAllowedDomainsForApps("example.com");
  net::RedirectInfo redirect_info;
  redirect_info.new_url = kTestGoogleURL;
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;

  throttle()->WillRedirectRequest(&redirect_info, response_head, &defer,
                                  &headers_update_params);

  EXPECT_EQ(headers_update_params.modified_cors_exempt_headers.GetHeader(
                safe_search_api::kGoogleAppsAllowedDomains),
            "example.com");
  EXPECT_THAT(headers_update_params.removed_headers,
              testing::Not(testing::Contains(
                  safe_search_api::kGoogleAppsAllowedDomains)));
}

TEST_F(GoogleURLLoaderThrottleTest,
       AllowedDomainsForAppsWillRedirectRequestToGoogleSubdomain) {
  SetAllowedDomainsForApps("example.com");
  net::RedirectInfo redirect_info;
  redirect_info.new_url = kGoogleSubdomainURL;
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;

  throttle()->WillRedirectRequest(&redirect_info, response_head, &defer,
                                  &headers_update_params);

  EXPECT_EQ(headers_update_params.modified_cors_exempt_headers.GetHeader(
                safe_search_api::kGoogleAppsAllowedDomains),
            "example.com");
  EXPECT_THAT(headers_update_params.removed_headers,
              testing::Not(testing::Contains(
                  safe_search_api::kGoogleAppsAllowedDomains)));
}

TEST_F(GoogleURLLoaderThrottleTest,
       AllowedDomainsForAppsWillRedirectRequestToNonGoogleDomain) {
  SetAllowedDomainsForApps("example.com");
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://example.com");
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;

  throttle()->WillRedirectRequest(&redirect_info, response_head, &defer,
                                  &headers_update_params);

  EXPECT_FALSE(headers_update_params.modified_cors_exempt_headers.HasHeader(
      safe_search_api::kGoogleAppsAllowedDomains));
  EXPECT_THAT(headers_update_params.removed_headers,
              testing::ElementsAre(safe_search_api::kGoogleAppsAllowedDomains));
}

TEST_F(GoogleURLLoaderThrottleTest,
       AllowedDomainsForAppsEmptyWillRedirectRequestToNonGoogleDomain) {
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://example.com");
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;

  throttle()->WillRedirectRequest(&redirect_info, response_head, &defer,
                                  &headers_update_params);

  EXPECT_FALSE(headers_update_params.modified_cors_exempt_headers.HasHeader(
      safe_search_api::kGoogleAppsAllowedDomains));
  EXPECT_THAT(headers_update_params.removed_headers,
              testing::Not(testing::Contains(
                  safe_search_api::kGoogleAppsAllowedDomains)));
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
using chrome::mojom::BoundSessionThrottlerParams;
using chrome::mojom::BoundSessionThrottlerParamsPtr;
using RequestBoundSessionStatus =
    GoogleURLLoaderThrottle::RequestBoundSessionStatus;

enum class RequestAction { kWillStartRequest, kWillRedirectRequest };

class FakeBoundSessionRequestThrottledHandler
    : public BoundSessionRequestThrottledHandler {
 public:
  void HandleRequestBlockedOnCookie(
      const GURL& request_url,
      ResumeOrCancelThrottledRequestCallback callback) override {
    EXPECT_FALSE(callback_);
    request_url_ = request_url;
    callback_ = std::move(callback);
  }

  void SimulateHandleRequestBlockedOnCookieCompleted(
      UnblockAction unblock_action,
      chrome::mojom::ResumeBlockedRequestsTrigger resume_trigger) {
    // Reset `request_url_`.
    request_url_ = GURL();
    std::move(callback_).Run(unblock_action, resume_trigger);
  }

  // Only relevant if a request is blocked.
  const GURL& request_url() { return request_url_; }

  bool IsRequestBlocked() { return !callback_.is_null(); }

 private:
  GURL request_url_;
  ResumeOrCancelThrottledRequestCallback callback_;
};

// std::vector<BoundSessionThrottlerParamsPtr> initializer list constructor
// doesn't work for some reason, so tests use these helpers to reduce
// boilerplate.
std::vector<BoundSessionThrottlerParamsPtr> ToVector(
    BoundSessionThrottlerParamsPtr params) {
  std::vector<BoundSessionThrottlerParamsPtr> result;
  result.push_back(std::move(params));
  return result;
}
std::vector<BoundSessionThrottlerParamsPtr> ToVector(
    BoundSessionThrottlerParamsPtr params1,
    BoundSessionThrottlerParamsPtr params2) {
  std::vector<BoundSessionThrottlerParamsPtr> result;
  result.push_back(std::move(params1));
  result.push_back(std::move(params2));
  return result;
}

class GoogleURLLoaderThrottleBoundSessionCredentialsTest
    : public GoogleURLLoaderThrottleTest,
      public ::testing::WithParamInterface<RequestAction> {
 public:
  GoogleURLLoaderThrottleBoundSessionCredentialsTest() = default;
  ~GoogleURLLoaderThrottleBoundSessionCredentialsTest() override = default;

  void ConfigureBoundSessionThrottlerParams(const std::string& domain,
                                            const std::string& path,
                                            base::Time expiration_date) {
    bound_session_throttler_params_ = ToVector(
        BoundSessionThrottlerParams::New(domain, path, expiration_date));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  FakeBoundSessionRequestThrottledHandler* bound_session_handler() {
    return bound_session_handler_.get();
  }

  void CallThrottleAndVerifyDeferExpectation(bool expect_defer,
                                             const GURL& url) {
    // `WillStartRequest()` has to be occur before `WillRedirectRequest()`, so
    // call it unconditionally.
    bool defer = false;
    network::ResourceRequest request;
    request.url = url;
    throttle()->WillStartRequest(&request, &defer);
    if (GetParam() == RequestAction::kWillRedirectRequest) {
      // Undo effects of the initial request, if needed.
      if (defer == true) {
        UnblockRequestAndVerifyCallbackAction(
            BoundSessionRequestThrottledHandler::UnblockAction::kResume);
      }

      net::RedirectInfo redirect_info;
      redirect_info.new_url = url;
      network::mojom::URLResponseHead response_head;
      network::HttpRequestHeadersUpdateParams headers_update_params;
      throttle()->WillRedirectRequest(&redirect_info, response_head, &defer,
                                      &headers_update_params);
    }
    EXPECT_EQ(expect_defer, defer);
    EXPECT_EQ(expect_defer, bound_session_handler()->IsRequestBlocked());
    if (expect_defer) {
      EXPECT_EQ(bound_session_handler()->request_url(), url);
    }
  }

  void UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledHandler::UnblockAction unblock_action,
      bool is_expected_navigation = false,
      chrome::mojom::ResumeBlockedRequestsTrigger resume_trigger =
          chrome::mojom::ResumeBlockedRequestsTrigger::kObservedFreshCookies) {
    switch (unblock_action) {
      case BoundSessionRequestThrottledHandler::UnblockAction::kResume:
        EXPECT_CALL(*delegate(), Resume());
        break;
      case BoundSessionRequestThrottledHandler::UnblockAction::kCancel:
        EXPECT_CALL(*delegate(), CancelWithError(net::ERR_ABORTED, testing::_));
        break;
    }

    bound_session_handler_->SimulateHandleRequestBlockedOnCookieCompleted(
        unblock_action, resume_trigger);

    RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(delegate());
    histogram_tester_->ExpectTotalCount(
        "Signin.BoundSessionCredentials.DeferredRequestDelay",
        /*expected_count=*/1);
    histogram_tester_->ExpectTotalCount(
        "Signin.BoundSessionCredentials.DeferredNavigationRequestDelay",
        /*expected_count=*/is_expected_navigation ? 1 : 0);
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

 protected:
  void CreateThrottle() override {
    chrome::mojom::DynamicParamsPtr dynamic_params(
        chrome::mojom::DynamicParams::New());
    dynamic_params->allowed_domains_for_apps = allowed_domains_for_apps_;
    for (const auto& params : bound_session_throttler_params_) {
      dynamic_params->bound_session_throttler_params.push_back(params->Clone());
    }

    std::unique_ptr<FakeBoundSessionRequestThrottledHandler>
        bound_session_handler =
            std::make_unique<FakeBoundSessionRequestThrottledHandler>();
    bound_session_handler_ = bound_session_handler.get();
    delegate_ = std::make_unique<MockThrottleDelegate>();

    throttle_ = std::make_unique<GoogleURLLoaderThrottle>(
#if BUILDFLAG(IS_ANDROID)
        "",
#endif
        std::move(bound_session_handler), std::move(dynamic_params));
    throttle_->set_delegate(delegate_.get());
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kEnableBoundSessionCredentials};
  base::test::TaskEnvironment task_environment_;
  raw_ptr<FakeBoundSessionRequestThrottledHandler, DanglingUntriaged>
      bound_session_handler_ = nullptr;
  std::vector<BoundSessionThrottlerParamsPtr> bound_session_throttler_params_;
  std::unique_ptr<base::HistogramTester> histogram_tester_ =
      std::make_unique<base::HistogramTester>();
};

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusNullParams) {
  EXPECT_EQ(
      GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(kTestGoogleURL, {}),
      RequestBoundSessionStatus::kNotCovered);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusEmptyParams) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                kTestGoogleURL, ToVector(BoundSessionThrottlerParams::New(
                                    "", "", base::Time::Now()))),
            RequestBoundSessionStatus::kNotCovered);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusCookieFresh) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                kTestGoogleURL,
                ToVector(BoundSessionThrottlerParams::New(
                    "google.com", "/", base::Time::Now() + base::Minutes(10)))),
            RequestBoundSessionStatus::kCoveredWithFreshCookie);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusNotInBoundSession) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                GURL("https://youtube.com"),
                ToVector(BoundSessionThrottlerParams::New("google.com", "/",
                                                          base::Time::Min()))),
            RequestBoundSessionStatus::kNotCovered);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusCookieExpired) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                kTestGoogleURL, ToVector(BoundSessionThrottlerParams::New(
                                    "google.com", "/", base::Time::Min()))),
            RequestBoundSessionStatus::kCoveredWithMissingCookie);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusCookieExpiresNow) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                kTestGoogleURL, ToVector(BoundSessionThrottlerParams::New(
                                    "google.com", "/", base::Time::Now()))),
            RequestBoundSessionStatus::kCoveredWithMissingCookie);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusCookieExpiredDomainWithLeadingDot) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                kTestGoogleURL, ToVector(BoundSessionThrottlerParams::New(
                                    ".google.com", "/", base::Time::Min()))),
            RequestBoundSessionStatus::kCoveredWithMissingCookie);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusSubdomainUrl) {
  EXPECT_EQ(
      GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
          kGoogleSubdomainURL, ToVector(BoundSessionThrottlerParams::New(
                                   "google.com", "/", base::Time::Min()))),
      RequestBoundSessionStatus::kCoveredWithMissingCookie);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusParentDomainUrl) {
  EXPECT_EQ(
      GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
          kTestGoogleURL, ToVector(BoundSessionThrottlerParams::New(
                              "accounts.google.com", "/", base::Time::Min()))),
      RequestBoundSessionStatus::kNotCovered);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusUrlWithPath) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                GURL("https://google.com/foo/bar.html"),
                ToVector(BoundSessionThrottlerParams::New("google.com", "/",
                                                          base::Time::Now()))),
            RequestBoundSessionStatus::kCoveredWithMissingCookie);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusPathEmpty) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                kTestGoogleURL, ToVector(BoundSessionThrottlerParams::New(
                                    "google.com", "", base::Time::Now()))),
            RequestBoundSessionStatus::kCoveredWithMissingCookie);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusUrlNotOnBoundSessionPath) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                kTestGoogleURL, ToVector(BoundSessionThrottlerParams::New(
                                    "google.com", "/test", base::Time::Now()))),
            RequestBoundSessionStatus::kNotCovered);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusUrlWithPathOnBoundSessionPath) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                GURL("https://google.com/test/foo/bar.html"),
                ToVector(BoundSessionThrottlerParams::New("google.com", "/test",
                                                          base::Time::Now()))),
            RequestBoundSessionStatus::kCoveredWithMissingCookie);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusSubdomainUrlWithPathOnBoundSessionPath) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                GURL("https://accounts.google.com/test/foo/bar.html"),
                ToVector(BoundSessionThrottlerParams::New("google.com", "/test",
                                                          base::Time::Now()))),
            RequestBoundSessionStatus::kCoveredWithMissingCookie);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusNonOverlappingParams) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                GURL("https://accounts.youtube.com/index.html"),
                ToVector(BoundSessionThrottlerParams::New("google.com", "/",
                                                          base::Time::Now()),
                         BoundSessionThrottlerParams::New("youtube.com", "/",
                                                          base::Time::Now()))),
            RequestBoundSessionStatus::kCoveredWithMissingCookie);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusNonOverlappingParamsHit) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                GURL("https://youtube.com/index.html"),
                ToVector(BoundSessionThrottlerParams::New("google.com", "/",
                                                          base::Time::Now()),
                         BoundSessionThrottlerParams::New("youtube.com", "/",
                                                          base::Time::Now()))),
            RequestBoundSessionStatus::kCoveredWithMissingCookie);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusNonOverlappingParamsHitSwapped) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                GURL("https://youtube.com/index.html"),
                ToVector(BoundSessionThrottlerParams::New("youtube.com", "/",
                                                          base::Time::Now()),
                         BoundSessionThrottlerParams::New("google.com", "/",
                                                          base::Time::Now()))),
            RequestBoundSessionStatus::kCoveredWithMissingCookie);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusNonOverlappingParamsMiss) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                GURL("https://example.org/index.html"),
                ToVector(BoundSessionThrottlerParams::New("google.com", "/",
                                                          base::Time::Now()),
                         BoundSessionThrottlerParams::New("youtube.com", "/",
                                                          base::Time::Now()))),
            RequestBoundSessionStatus::kNotCovered);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusOverlappingParamsBothExpired) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                GURL("https://accounts.google.com/index.html"),
                ToVector(BoundSessionThrottlerParams::New(
                             "accounts.google.com", "/", base::Time::Now()),
                         BoundSessionThrottlerParams::New("google.com", "/",
                                                          base::Time::Now()))),
            RequestBoundSessionStatus::kCoveredWithMissingCookie);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusOverlappingParamsOneExpired) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                GURL("https://accounts.google.com/index.html"),
                ToVector(BoundSessionThrottlerParams::New(
                             "accounts.google.com", "/", base::Time::Now()),
                         BoundSessionThrottlerParams::New("google.com", "/",
                                                          base::Time::Min()))),
            RequestBoundSessionStatus::kCoveredWithMissingCookie);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       GetRequestBoundSessionStatusOverlappingParamsOneExpiredSwapped) {
  EXPECT_EQ(GoogleURLLoaderThrottle::GetRequestBoundSessionStatus(
                GURL("https://accounts.google.com/index.html"),
                ToVector(BoundSessionThrottlerParams::New(
                             "accounts.google.com", "/", base::Time::Min()),
                         BoundSessionThrottlerParams::New("google.com", "/",
                                                          base::Time::Now()))),
            RequestBoundSessionStatus::kCoveredWithMissingCookie);
}

TEST_P(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       NullBoundSessionThrottlerParams) {
  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/false, kTestGoogleURL);
}

TEST_P(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       EmptyBoundSessionThrottlerParams) {
  ConfigureBoundSessionThrottlerParams("", "", base::Time::Now());
  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/false, kGoogleSubdomainURL);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       InterceptRequestWithSameOriginCredsMode) {
  ConfigureBoundSessionThrottlerParams("google.com", "/", base::Time::Min());
  bool defer = false;
  network::ResourceRequest request;
  request.url = kTestGoogleURL;
  request.credentials_mode = network::mojom::CredentialsMode::kSameOrigin;
  throttle()->WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(bound_session_handler()->IsRequestBlocked());
  UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledHandler::UnblockAction::kResume);

  // Subsequent redirects should be intercepted as well.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = kTestGoogleURL;
  network::mojom::URLResponseHead response_head;
  network::HttpRequestHeadersUpdateParams headers_update_params;
  throttle()->WillRedirectRequest(&redirect_info, response_head, &defer,
                                  &headers_update_params);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(bound_session_handler()->IsRequestBlocked());
  UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledHandler::UnblockAction::kResume);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       NoInterceptRequestWithSendCookiesFalse) {
  ConfigureBoundSessionThrottlerParams("google.com", "/", base::Time::Min());
  bool defer = false;
  network::ResourceRequest request;
  request.url = kTestGoogleURL;
  request.credentials_mode = network::mojom::CredentialsMode::kOmit;
  throttle()->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(bound_session_handler()->IsRequestBlocked());

  // Subsequent redirects shouldn't be intercepted as well.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = kTestGoogleURL;
  network::mojom::URLResponseHead response_head;
  network::HttpRequestHeadersUpdateParams headers_update_params;
  throttle()->WillRedirectRequest(&redirect_info, response_head, &defer,
                                  &headers_update_params);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(bound_session_handler()->IsRequestBlocked());
}

TEST_P(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       InterceptBoundSessionCookieExpired) {
  ConfigureBoundSessionThrottlerParams("google.com", "/",
                                       base::Time::Now() - base::Minutes(10));
  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/true, GURL("https://accounts.google.com/test/bar.html"));
  UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledHandler::UnblockAction::kResume);
}

TEST_F(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       InterceptNavigationBoundSessionCookieExpired) {
  ConfigureBoundSessionThrottlerParams("google.com", "/",
                                       base::Time::Now() - base::Minutes(10));
  bool defer = false;
  network::ResourceRequest request;
  request.url = GURL("https://accounts.google.com/test/bar.html");
  // Make `request` look like a main frame navigation request.
  request.is_outermost_main_frame = true;
  request.destination = network::mojom::RequestDestination::kDocument;
  throttle()->WillStartRequest(&request, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(bound_session_handler()->IsRequestBlocked());
  UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledHandler::UnblockAction::kResume,
      /*is_expected_navigation=*/true);
}

TEST_P(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       InterceptAndCancelRequest) {
  ConfigureBoundSessionThrottlerParams("google.com", "/", base::Time::Now());

  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/true, kGoogleSubdomainURL);
  UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledHandler::UnblockAction::kCancel);
}

TEST_P(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       RecordDeferredRequestUnblockTriggerSuccess) {
  ConfigureBoundSessionThrottlerParams("google.com", "/", base::Time::Now());

  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/true, kGoogleSubdomainURL);
  const auto kResumeTrigger =
      chrome::mojom::ResumeBlockedRequestsTrigger::kCookieRefreshFetchFailure;
  UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledHandler::UnblockAction::kResume,
      /*is_expected_navigation=*/false, kResumeTrigger);
  base::HistogramTester histogram_tester;
  throttle()->WillProcessResponse(kGoogleSubdomainURL, nullptr, nullptr);
  histogram_tester.ExpectBucketCount(
      "Signin.BoundSessionCredentials.DeferredRequestUnblockTrigger.Success",
      static_cast<int>(kResumeTrigger),
      /*expected_count=*/1);
}

TEST_P(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       RecordDeferredRequestUnblockTriggerFailure) {
  ConfigureBoundSessionThrottlerParams("google.com", "/", base::Time::Now());

  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/true, kGoogleSubdomainURL);
  const auto kResumeTrigger =
      chrome::mojom::ResumeBlockedRequestsTrigger::kCookieRefreshFetchFailure;
  UnblockRequestAndVerifyCallbackAction(
      BoundSessionRequestThrottledHandler::UnblockAction::kResume,
      /*is_expected_navigation=*/false, kResumeTrigger);
  base::HistogramTester histogram_tester;
  throttle()->WillOnCompleteWithError(network::URLLoaderCompletionStatus());
  histogram_tester.ExpectBucketCount(
      "Signin.BoundSessionCredentials.DeferredRequestUnblockTrigger.Failure",
      static_cast<int>(kResumeTrigger),
      /*expected_count=*/1);
}

TEST_P(GoogleURLLoaderThrottleBoundSessionCredentialsTest,
       NoRecordDeferredRequestUnblockTriggerNotDeferred) {
  ConfigureBoundSessionThrottlerParams("google.com", "/",
                                       base::Time::Now() + base::Minutes(10));

  CallThrottleAndVerifyDeferExpectation(
      /*expect_defer=*/false, kGoogleSubdomainURL);
  base::HistogramTester histogram_tester;
  throttle()->WillProcessResponse(kGoogleSubdomainURL, nullptr, nullptr);
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix(
          "Signin.BoundSessionCredentials.DeferredRequestUnblockTrigger."),
      testing::IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(WillStartRequest,
                         GoogleURLLoaderThrottleBoundSessionCredentialsTest,
                         ::testing::Values(RequestAction::kWillStartRequest));

INSTANTIATE_TEST_SUITE_P(
    WillRedirectRequest,
    GoogleURLLoaderThrottleBoundSessionCredentialsTest,
    ::testing::Values(RequestAction::kWillRedirectRequest));

#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

}  // namespace
