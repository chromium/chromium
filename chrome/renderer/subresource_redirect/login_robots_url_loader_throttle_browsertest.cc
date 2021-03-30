// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/renderer/subresource_redirect/login_robots_decider_agent.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_url_loader_throttle.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_util.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_redirect/subresource_redirect_test_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace subresource_redirect {

// Possible deferral states for a subresource after sending the
// WillStartRequest.
enum WillStartRequestDeferralState {
  kRedirected,
  kNotRedirected,
  kDeferred,
};

// Holds the url loader throttle and the delegate together.
class LoginRobotsDeciderInfo : public blink::URLLoaderThrottle::Delegate {
 public:
  LoginRobotsDeciderInfo(
      const std::string url,
      std::unique_ptr<SubresourceRedirectURLLoaderThrottle> throttle)
      : url_(url), throttle_(std::move(throttle)) {
    throttle_->set_delegate(this);
  }

  // Sends WillStartRequest and verifies the deferral state.
  void SendStartRequestAndVerifyDeferral(
      WillStartRequestDeferralState expected_deferral) {
    network::ResourceRequest request;
    request.url = url_;
    request.destination = network::mojom::RequestDestination::kImage;
    request.previews_state = blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON;
    bool defer = true;

    throttle_->WillStartRequest(&request, &defer);
    switch (expected_deferral) {
      case WillStartRequestDeferralState::kDeferred:
        EXPECT_TRUE(defer);
        EXPECT_EQ(GetSubresourceURLForURL(url_), request.url);
        break;
      case WillStartRequestDeferralState::kRedirected:
        EXPECT_FALSE(defer);
        EXPECT_EQ(GetSubresourceURLForURL(url_), request.url);
        break;
      case WillStartRequestDeferralState::kNotRedirected:
        EXPECT_FALSE(defer);
        EXPECT_EQ(url_, request.url);
        break;
      default:
        NOTREACHED();
    }
  }

  // blink::URLLoaderThrottle::Delegate
  void Resume() override {
    EXPECT_FALSE(did_resume_);
    did_resume_ = true;
  }
  void RestartWithURLResetAndFlags(int load_flags) override {
    EXPECT_FALSE(did_restart_with_url_reset_and_flags_);
    did_restart_with_url_reset_and_flags_ = true;
  }
  void CancelWithError(int error_code,
                       base::StringPiece custom_reason = "") override {
    NOTIMPLEMENTED();
  }

  void VerifyWillProcessResponse() {
    network::mojom::URLResponseHeadPtr head =
        network::CreateURLResponseHead(net::HTTP_OK);
    head->headers->SetHeader("Content-Length", "1024");
    bool defer = false;
    throttle_->WillProcessResponse(GURL("https://foo.com/img.jpg"), head.get(),
                                   &defer);
    EXPECT_FALSE(defer);
  }

  bool did_resume() const { return did_resume_; }
  bool did_restart_with_url_reset_and_flags() const {
    return did_restart_with_url_reset_and_flags_;
  }

 private:
  GURL url_;
  std::unique_ptr<SubresourceRedirectURLLoaderThrottle> throttle_;

  // The state of delegate callbacks
  bool did_resume_ = false;
  bool did_restart_with_url_reset_and_flags_ = false;
};

class SubresourceRedirectLoginRobotsURLLoaderThrottleTest
    : public ChromeRenderViewTest {
 public:
  void DisableSubresourceRedirectFeature() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kSubresourceRedirect);
  }

  void SetUpRobotsRules(const std::string& origin,
                        const std::vector<RobotsRule>& patterns) {
    login_robots_decider_agent_->UpdateRobotsRulesForTesting(
        url::Origin::Create(GURL(origin)), GetRobotsRulesProtoString(patterns));
  }

  std::unique_ptr<SubresourceRedirectURLLoaderThrottle>
  CreateLoginRobotsDecider(
      const std::string& url,
      network::mojom::RequestDestination request_destination,
      int previews_state) {
    blink::WebURLRequest request;
    request.SetUrl(GURL(url));
    request.SetPreviewsState(previews_state);
    request.SetRequestDestination(request_destination);
    auto throttle = SubresourceRedirectURLLoaderThrottle::MaybeCreateThrottle(
        request, view_->GetMainRenderFrame()->GetRoutingID());
    EXPECT_TRUE(throttle.get());
    return throttle;
  }

  std::unique_ptr<LoginRobotsDeciderInfo> CreateURLLoaderThrottleInfo(
      const std::string& url) {
    return std::make_unique<LoginRobotsDeciderInfo>(
        url, CreateLoginRobotsDecider(
                 url, network::mojom::RequestDestination::kImage,
                 blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON));
  }

  void SetLoggedInState(bool is_logged_in) {
    login_robots_decider_agent_->SetLoggedInState(is_logged_in);
  }

 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kSubresourceRedirect,
          {{"enable_subresource_server_redirect", "true"},
           {"enable_login_robots_based_compression", "true"},
           {"enable_public_image_hints_based_compression", "false"}}}},
        {});
    login_robots_decider_agent_ = new LoginRobotsDeciderAgent(
        &associated_interfaces_, view_->GetMainRenderFrame());
  }

 protected:
  LoginRobotsDeciderAgent* login_robots_decider_agent_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

TEST_F(SubresourceRedirectLoginRobotsURLLoaderThrottleTest,
       TestMaybeCreateThrottle) {
  struct TestCase {
    bool data_saver_enabled;
    bool is_subresource_redirect_feature_enabled;
    network::mojom::RequestDestination destination;
    int previews_state;
    const std::string url;
    bool expected_is_throttle_created;
  };

  const TestCase kTestCases[]{
      {true, true, network::mojom::RequestDestination::kImage,
       blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
       "https://www.test.com/test.jpg", true},

      // Failure cases
      {false, true, network::mojom::RequestDestination::kImage,
       blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
       "https://www.test.com/test.jpg", false},
      {true, false, network::mojom::RequestDestination::kImage,
       blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
       "https://www.test.com/test.jpg", false},
      {true, true, network::mojom::RequestDestination::kScript,
       blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
       "https://www.test.com/test.jpg", false},
      {true, true, network::mojom::RequestDestination::kImage,
       blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
       "http://www.test.com/test.jpg", false},
  };

  for (const TestCase& test_case : kTestCases) {
    blink::WebNetworkStateNotifier::SetSaveDataEnabled(
        test_case.data_saver_enabled);
    if (!test_case.is_subresource_redirect_feature_enabled) {
      DisableSubresourceRedirectFeature();
    }

    blink::WebURLRequest request;
    request.SetPreviewsState(test_case.previews_state);
    request.SetUrl(GURL(test_case.url));
    request.SetRequestDestination(test_case.destination);
    EXPECT_EQ(
        test_case.expected_is_throttle_created,
        SubresourceRedirectURLLoaderThrottle::MaybeCreateThrottle(
            request, view_->GetMainRenderFrame()->GetRoutingID()) != nullptr);
  }
}

TEST_F(SubresourceRedirectLoginRobotsURLLoaderThrottleTest,
       TestGetSubresourceURL) {
  struct TestCase {
    int previews_state;
    bool is_logged_in;
    std::string original_url;
    GURL redirected_subresource_url;  // Empty URL means there will be no
                                      // redirect.
  };

  const TestCase kTestCases[]{
      {
          blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
          false,
          "https://www.test.com/public_img.jpg",
          GetSubresourceURLForURL(GURL("https://www.test.com/public_img.jpg")),
      },
      {
          blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
          false,
          "https://www.test.com/public_img.jpg#anchor",
          GetSubresourceURLForURL(
              GURL("https://www.test.com/public_img.jpg#anchor")),
      },
      {
          blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
          false,
          "https://www.test.com/public_img.jpg?public_arg1=bar&public_arg2",
          GetSubresourceURLForURL(
              GURL("https://www.test.com/"
                   "public_img.jpg?public_arg1=bar&public_arg2")),
      },
      // Private images will not be redirected.
      {
          blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
          false,
          "https://www.test.com/private_img.jpg",
          GURL(),
      },
      {
          blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
          false,
          "https://www.test.com/public_img.jpg&private_arg1=foo",
          GURL(),
      },
      // No redirection when logged-in
      {
          blink::PreviewsTypes::SUBRESOURCE_REDIRECT_ON,
          true,
          "https://www.test.com/public_img.jpg",
          GURL(),
      },
  };
  blink::WebNetworkStateNotifier::SetSaveDataEnabled(true);

  SetUpRobotsRules("https://www.test.com", {{kRuleTypeDisallow, "*private"},
                                            {kRuleTypeAllow, "/public"},
                                            {kRuleTypeDisallow, ""}});

  for (const TestCase& test_case : kTestCases) {
    SetLoggedInState(test_case.is_logged_in);
    auto throttle = CreateLoginRobotsDecider(
        test_case.original_url, network::mojom::RequestDestination::kImage,
        test_case.previews_state);
    network::ResourceRequest request;
    request.url = GURL(test_case.original_url);
    request.destination = network::mojom::RequestDestination::kImage;
    request.previews_state = test_case.previews_state;
    bool defer = true;
    throttle->WillStartRequest(&request, &defer);

    EXPECT_FALSE(defer);
    if (!test_case.redirected_subresource_url.is_empty()) {
      EXPECT_EQ(request.url, test_case.redirected_subresource_url);
    } else {
      EXPECT_EQ(request.url, test_case.original_url);
    }
  }
}

// Tests the cases when robots rules are already sent, before throttles are
// created.
TEST_F(SubresourceRedirectLoginRobotsURLLoaderThrottleTest,
       TestRobotsRulesSentBeforeThrottle) {
  blink::WebNetworkStateNotifier::SetSaveDataEnabled(true);
  SetLoggedInState(false);

  SetUpRobotsRules("https://www.test.com",
                   {{kRuleTypeAllow, "/public"}, {kRuleTypeDisallow, ""}});

  auto throttle_info1 =
      CreateURLLoaderThrottleInfo("https://www.test.com/public.jpg");
  auto throttle_info2 =
      CreateURLLoaderThrottleInfo("https://www.test.com/private.jpg");

  throttle_info1->SendStartRequestAndVerifyDeferral(
      WillStartRequestDeferralState::kRedirected);
  throttle_info2->SendStartRequestAndVerifyDeferral(
      WillStartRequestDeferralState::kNotRedirected);
  EXPECT_FALSE(throttle_info1->did_resume());
  EXPECT_FALSE(throttle_info2->did_resume());
  EXPECT_FALSE(throttle_info1->did_restart_with_url_reset_and_flags());
  EXPECT_FALSE(throttle_info2->did_restart_with_url_reset_and_flags());
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleRobotsDisallowed, 1);
}

// Tests the cases when robots rules are sent, after throttles are
// created.
TEST_F(SubresourceRedirectLoginRobotsURLLoaderThrottleTest,
       TestRobotsRulesSentAfterThrottle) {
  blink::WebNetworkStateNotifier::SetSaveDataEnabled(true);
  SetLoggedInState(false);

  auto throttle_info1 =
      CreateURLLoaderThrottleInfo("https://www.test.com/public.jpg");
  auto throttle_info2 =
      CreateURLLoaderThrottleInfo("https://www.test.com/private.jpg");

  // Both requests will be deferred until rules are received.
  throttle_info1->SendStartRequestAndVerifyDeferral(
      WillStartRequestDeferralState::kDeferred);
  throttle_info2->SendStartRequestAndVerifyDeferral(
      WillStartRequestDeferralState::kDeferred);
  EXPECT_FALSE(throttle_info1->did_resume());
  EXPECT_FALSE(throttle_info2->did_resume());
  EXPECT_FALSE(throttle_info1->did_restart_with_url_reset_and_flags());
  EXPECT_FALSE(throttle_info2->did_restart_with_url_reset_and_flags());

  SetUpRobotsRules("https://www.test.com",
                   {{kRuleTypeAllow, "/public"}, {kRuleTypeDisallow, ""}});
  // The public resource should resume loading, with the already modified URL.
  EXPECT_TRUE(throttle_info1->did_resume());
  EXPECT_FALSE(throttle_info1->did_restart_with_url_reset_and_flags());

  // The private resource should restart and resume loading with the original
  // URL.
  EXPECT_TRUE(throttle_info2->did_restart_with_url_reset_and_flags());
  EXPECT_TRUE(throttle_info2->did_resume());

  throttle_info1->VerifyWillProcessResponse();
  throttle_info2->VerifyWillProcessResponse();
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kRedirectable, 1);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleRobotsDisallowed, 1);
}

// Tests the cases when robots rules retrieval timesout.
TEST_F(SubresourceRedirectLoginRobotsURLLoaderThrottleTest,
       TestRobotsRulesTimeout) {
  blink::WebNetworkStateNotifier::SetSaveDataEnabled(true);
  SetLoggedInState(false);

  auto throttle_info1 =
      CreateURLLoaderThrottleInfo("https://www.test.com/public.jpg");
  auto throttle_info2 =
      CreateURLLoaderThrottleInfo("https://www.test.com/private.jpg");

  // Both requests will be deferred until rule retrieval times out.
  throttle_info1->SendStartRequestAndVerifyDeferral(
      WillStartRequestDeferralState::kDeferred);
  throttle_info2->SendStartRequestAndVerifyDeferral(
      WillStartRequestDeferralState::kDeferred);
  EXPECT_FALSE(throttle_info1->did_resume());
  EXPECT_FALSE(throttle_info2->did_resume());
  EXPECT_FALSE(throttle_info1->did_restart_with_url_reset_and_flags());
  EXPECT_FALSE(throttle_info2->did_restart_with_url_reset_and_flags());

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));

  // Both resources should restart and resume loading with the original URL.
  EXPECT_TRUE(throttle_info1->did_restart_with_url_reset_and_flags());
  EXPECT_TRUE(throttle_info1->did_resume());
  EXPECT_TRUE(throttle_info2->did_restart_with_url_reset_and_flags());
  EXPECT_TRUE(throttle_info2->did_resume());

  throttle_info1->VerifyWillProcessResponse();
  throttle_info2->VerifyWillProcessResponse();
  histogram_tester_.ExpectTotalCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult", 2);
  histogram_tester_.ExpectBucketCount(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      SubresourceRedirectResult::kIneligibleRobotsTimeout, 2);
}

}  // namespace subresource_redirect
