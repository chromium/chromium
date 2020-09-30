// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/password_protection/password_protection_navigation_throttle.h"

#include <memory>

#include "base/test/bind_test_util.h"
#include "components/safe_browsing/content/password_protection/mock_password_protection_service.h"
#include "components/safe_browsing/content/password_protection/password_protection_request.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

class PasswordProtectionNavigationThrottleTest
    : public content::RenderViewHostTestHarness,
      public content::WebContentsObserver {
 public:
  PasswordProtectionNavigationThrottleTest() = default;
  ~PasswordProtectionNavigationThrottleTest() override = default;

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    // Observe the WebContents to add the throttle.
    Observe(RenderViewHostTestHarness::web_contents());
  }

  void TearDown() override {
    content::RenderViewHostTestHarness::TearDown();
    throttle_.release();
  }

  void SetIsWarningShown(bool is_warning_shown) {
    is_warning_shown_ = is_warning_shown;
  }

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    std::vector<password_manager::MatchingReusedCredential> credentials = {
        {"http://example.test"}, {"http://2.example.com"}};
    std::unique_ptr<safe_browsing::MockPasswordProtectionService>
        password_protection_service =
            std::make_unique<safe_browsing::MockPasswordProtectionService>();

    scoped_refptr<PasswordProtectionRequest> request =
        new PasswordProtectionRequest(
            RenderViewHostTestHarness::web_contents(), GURL(), GURL(), GURL(),
            "username", PasswordType::PASSWORD_TYPE_UNKNOWN, credentials,
            LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
            /* password_field_exists*/ true, password_protection_service.get(),
            /*request_timeout_in_ms=*/0);
    std::unique_ptr<PasswordProtectionNavigationThrottle> throttle =
        std::make_unique<PasswordProtectionNavigationThrottle>(
            navigation_handle, request, is_warning_shown_);
    throttle_ = std::move(throttle);
  }

  std::unique_ptr<PasswordProtectionNavigationThrottle> throttle() {
    return std::move(throttle_);
  }

 protected:
  std::unique_ptr<content::NavigationSimulator> StartNavigation(
      const GURL& first_url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(first_url,
                                                              main_rfh());
    navigation_simulator->SetAutoAdvance(false);
    navigation_simulator->Start();
    return navigation_simulator;
  }

  bool is_warning_shown_ = false;
  std::unique_ptr<PasswordProtectionNavigationThrottle> throttle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordProtectionNavigationThrottleTest);
};

TEST_F(PasswordProtectionNavigationThrottleTest, DeferOnNavigation) {
  auto navigation_simulator = StartNavigation(GURL("http://www.example.com/"));
  EXPECT_EQ(content::NavigationThrottle::DEFER, throttle()->WillStartRequest());
}

TEST_F(PasswordProtectionNavigationThrottleTest,
       CancelNavigationOnWarningShown) {
  SetIsWarningShown(true);

  auto navigation_simulator = StartNavigation(GURL("http://www.example.com/"));
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            throttle()->WillStartRequest());
}

TEST_F(PasswordProtectionNavigationThrottleTest, WillDeferRedirectRequest) {
  auto navigation_simulator = StartNavigation(GURL("http://www.example.com/"));

  std::unique_ptr<PasswordProtectionNavigationThrottle> throttle_copy =
      std::move(throttle_);
  throttle_copy->set_resume_callback_for_testing(
      base::BindLambdaForTesting([&]() {}));
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle_copy->WillRedirectRequest());

  // Release request.
  throttle_copy->ResumeNavigation();
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle_copy->WillRedirectRequest());
}

TEST_F(PasswordProtectionNavigationThrottleTest, WillCancelRedirectRequest) {
  auto navigation_simulator = StartNavigation(GURL("http://www.example.com/"));

  // Release request.
  std::unique_ptr<PasswordProtectionNavigationThrottle> throttle_copy =
      std::move(throttle_);
  throttle_copy->set_cancel_deferred_navigation_callback_for_testing(
      base::BindRepeating(
          [](content::NavigationThrottle::ThrottleCheckResult result) {}));
  throttle_copy->CancelNavigation(content::NavigationThrottle::DEFER);

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle_copy->WillRedirectRequest());
}

TEST_F(PasswordProtectionNavigationThrottleTest,
       WillCancelRedirectRequestOnWarningShown) {
  SetIsWarningShown(true);

  auto navigation_simulator = StartNavigation(GURL("http://www.example.com/"));
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            throttle()->WillRedirectRequest());
}

}  // namespace safe_browsing