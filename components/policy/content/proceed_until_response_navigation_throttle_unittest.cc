// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/proceed_until_response_navigation_throttle.h"

#include "base/memory/raw_ptr.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class EventExpectation final {
 public:
  EventExpectation() = default;
  EventExpectation(const EventExpectation&) = delete;
  EventExpectation& operator=(const EventExpectation&) = delete;
  ~EventExpectation() = default;

  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() {
    EXPECT_TRUE(expect_will_start_request_);
    visit_will_start_request_ = true;
    return action_on_will_start_request_;
  }
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest() {
    EXPECT_TRUE(expect_will_redirect_request_);
    visit_will_redirect_request_ = true;
    return action_on_will_redirect_request_;
  }
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse() {
    EXPECT_TRUE(expect_will_process_response_);
    visit_will_process_response_ = true;
    return action_on_will_process_response_;
  }

  void SetDeferredResultCallback(
      const ProceedUntilResponseNavigationThrottle::DeferredResultCallback&
          deferred_result_callback) {
    deferred_result_callback_ = deferred_result_callback;
  }

  void ExpectWillStartRequest(
      content::NavigationThrottle::ThrottleAction action =
          content::NavigationThrottle::ThrottleAction::PROCEED) {
    action_on_will_start_request_ = action;
    expect_will_start_request_ = true;
  }
  void ExpectWillRedirectRequest(
      content::NavigationThrottle::ThrottleAction action =
          content::NavigationThrottle::ThrottleAction::PROCEED) {
    action_on_will_redirect_request_ = action;
    expect_will_redirect_request_ = true;
  }
  void ExpectWillProcessResponse(
      content::NavigationThrottle::ThrottleAction action =
          content::NavigationThrottle::ThrottleAction::PROCEED) {
    action_on_will_process_response_ = action;
    expect_will_process_response_ = true;
  }

  void CheckExpectations() {
    EXPECT_EQ(expect_will_start_request_, visit_will_start_request_);
    EXPECT_EQ(expect_will_redirect_request_, visit_will_redirect_request_);
    EXPECT_EQ(expect_will_process_response_, visit_will_process_response_);
  }

  void Resolve(bool proceed,
               std::optional<content::NavigationThrottle::ThrottleCheckResult>
                   result = std::nullopt) {
    deferred_result_callback_.Run(proceed, result);
  }

 private:
  bool expect_will_start_request_ = false;
  bool expect_will_redirect_request_ = false;
  bool expect_will_process_response_ = false;
  bool visit_will_start_request_ = false;
  bool visit_will_redirect_request_ = false;
  bool visit_will_process_response_ = false;
  content::NavigationThrottle::ThrottleAction action_on_will_start_request_ =
      content::NavigationThrottle::ThrottleAction::PROCEED;
  content::NavigationThrottle::ThrottleAction action_on_will_redirect_request_ =
      content::NavigationThrottle::ThrottleAction::PROCEED;
  content::NavigationThrottle::ThrottleAction action_on_will_process_response_ =
      content::NavigationThrottle::ThrottleAction::PROCEED;

  ProceedUntilResponseNavigationThrottle::DeferredResultCallback
      deferred_result_callback_;
};

class EventMonitorNavigationThrottleClient final
    : public ProceedUntilResponseNavigationThrottle::Client {
 public:
  EventMonitorNavigationThrottleClient(
      content::NavigationHandle* navigation_handle,
      raw_ptr<EventExpectation> event_expectation)
      : ProceedUntilResponseNavigationThrottle::Client(navigation_handle),
        event_expectation_(event_expectation) {}

 private:
  // NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override {
    return event_expectation_->WillStartRequest();
  }
  ThrottleCheckResult WillRedirectRequest() override {
    return event_expectation_->WillRedirectRequest();
  }
  ThrottleCheckResult WillProcessResponse() override {
    return event_expectation_->WillProcessResponse();
  }
  const char* GetNameForLogging() override {
    return "EventMonitorNavigationThrottle";
  }

  // ProceedUntilResponseNavigationThrottle::Client implementation:
  void SetDeferredResultCallback(
      const ProceedUntilResponseNavigationThrottle::DeferredResultCallback&
          deferred_result_callback) override {
    event_expectation_->SetDeferredResultCallback(deferred_result_callback);
  }

  raw_ptr<EventExpectation> event_expectation_;
};

}  // namespace

class ProceedUntilResponseNavigationThrottleTest
    : public content::RenderViewHostTestHarness,
      public content::WebContentsObserver {
 public:
  ProceedUntilResponseNavigationThrottleTest() = default;
  ProceedUntilResponseNavigationThrottleTest(
      const ProceedUntilResponseNavigationThrottleTest&) = delete;
  ProceedUntilResponseNavigationThrottleTest& operator=(
      const ProceedUntilResponseNavigationThrottleTest&) = delete;
  ~ProceedUntilResponseNavigationThrottleTest() override = default;

 protected:
  std::unique_ptr<content::NavigationSimulator> StartNavigation(
      const GURL& url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
    navigation_simulator->SetAutoAdvance(false);
    navigation_simulator->Start();
    return navigation_simulator;
  }

  EventExpectation& event_expectation() { return event_expectation_; }

 private:
  // content::RenderViewHostTestHarness implementation:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    Observe(content::RenderViewHostTestHarness::web_contents());
  }

  // content::WebContentsObserver implementation:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    std::unique_ptr<ProceedUntilResponseNavigationThrottle::Client>
        monitor_throttle =
            std::make_unique<EventMonitorNavigationThrottleClient>(
                navigation_handle, &event_expectation_);
    auto throttle = std::make_unique<ProceedUntilResponseNavigationThrottle>(
        navigation_handle, std::move(monitor_throttle), std::nullopt);

    navigation_handle->RegisterThrottleForTesting(std::move(throttle));
  }

  EventExpectation event_expectation_;
};

// Test a simple case, the internal throttle doesn't ask DEFER.
TEST_F(ProceedUntilResponseNavigationThrottleTest, Proceed) {
  // Proceed on WillStartRequest.
  event_expectation().ExpectWillStartRequest();
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Proceed on WillRedirectRequest.
  event_expectation().ExpectWillRedirectRequest();
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Proceed on WillProcessResponse.
  event_expectation().ExpectWillProcessResponse();
  navigation_simulator->ReadyToCommit();
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  EXPECT_FALSE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a simplest case, the internal throttle doesn't ask DEFER, but CANCEL
// on WillStartRequest.
TEST_F(ProceedUntilResponseNavigationThrottleTest, CancelOnStartRequest) {
  // Cancel on WillStartRequest.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::CANCEL);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            navigation_simulator->GetLastThrottleCheckResult());
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a simplest case, the internal throttle doesn't ask DEFER, but CANCEL
// on WillRedirectRequest.
TEST_F(ProceedUntilResponseNavigationThrottleTest, CancelOnRedirectRequest) {
  // Proceed on WillStartRequest.
  event_expectation().ExpectWillStartRequest();
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Cancel on WillRedirectRequest.
  event_expectation().ExpectWillRedirectRequest(
      content::NavigationThrottle::CANCEL);
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            navigation_simulator->GetLastThrottleCheckResult());
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a simplest case, the internal throttle doesn't ask DEFER, but CANCEL
// on WillProcessResponse.
TEST_F(ProceedUntilResponseNavigationThrottleTest, CancelOnProcessResponse) {
  // Proceed on WillStartRequest.
  event_expectation().ExpectWillStartRequest();
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Proceed on WillRedirectRequest.
  event_expectation().ExpectWillRedirectRequest();
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Proceed on WillProcessResponse.
  event_expectation().ExpectWillProcessResponse(
      content::NavigationThrottle::CANCEL);
  navigation_simulator->ReadyToCommit();
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            navigation_simulator->GetLastThrottleCheckResult());
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and resumes
// the request before WillRedirectRequest. It asks DEFER on WillRedirectRequest
// again and resumes the request before WillProcessResponse.
TEST_F(ProceedUntilResponseNavigationThrottleTest, DeferOnStartAndRedirect) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to proceed.
  event_expectation().Resolve(/*proceed=*/true);

  // Defer on WillRedirectRequest.
  // This will be observed as PROCEED again in the throttle runner.
  event_expectation().ExpectWillRedirectRequest(
      content::NavigationThrottle::DEFER);
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Resolve the deferred redirect request to proceed.
  event_expectation().Resolve(/*proceed=*/true);

  // Proceed on WillProcessResponse.
  event_expectation().ExpectWillProcessResponse();
  navigation_simulator->ReadyToCommit();
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  EXPECT_FALSE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and resumes
// the request before WillRedirectRequest with a junk action, but intended to
// proceed.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferOnStartAndResolveToProceedWithJunkAction) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to proceed, but with a junk action, `CANCEL`.
  // If the `proceed` is true, the given action should be ignored.
  event_expectation().Resolve(/*proceed=*/true,
                              content::NavigationThrottle::CANCEL);

  // Proceed on WillProcessResponse.
  event_expectation().ExpectWillProcessResponse();
  navigation_simulator->ReadyToCommit();
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  EXPECT_FALSE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and resumes
// the request to cancel before WillRedirectRequest.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferOnStartAndResolveToCancelOnRedirect) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to cancel.
  event_expectation().Resolve(/*proceed=*/false,
                              content::NavigationThrottle::CANCEL);

  // Cancel on WillRedirectRequest without calling the internal throttle.
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            navigation_simulator->GetLastThrottleCheckResult());
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and resumes
// the request to cancel before WillProcessResponse.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferOnStartAndResolveToCancelOnResponse) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to cancel.
  event_expectation().Resolve(/*proceed=*/false,
                              content::NavigationThrottle::CANCEL);

  // Cancel on WillProcessResponse without calling the internal throttle.
  navigation_simulator->ReadyToCommit();
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            navigation_simulator->GetLastThrottleCheckResult());
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and resumes
// the request before WillRedirectRequest. It asks DEFER on WillRedirectRequest
// again and resumes the request to cancel before WillProcessResponse.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferOnStartAndRedirectAndResolveRedirectToCancelBeforeResponse) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to proceed.
  event_expectation().Resolve(/*proceed=*/true);

  // Defer on WillRedirectRequest.
  // This will be observed as PROCEED again in the throttle runner.
  event_expectation().ExpectWillRedirectRequest(
      content::NavigationThrottle::DEFER);
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Resolve the deferred redirect request to cancel.
  event_expectation().Resolve(/*proceed=*/false,
                              content::NavigationThrottle::CANCEL);

  // Proceed on WillProcessResponse.
  navigation_simulator->ReadyToCommit();
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            navigation_simulator->GetLastThrottleCheckResult());
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and resumes
// the request before WillRedirectRequest. It asks DEFER on WillRedirectRequest
// again and resumes the request after WillProcessResponse.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferOnStartAndRedirectAndResolveRedirectAfterResponse) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to proceed.
  event_expectation().Resolve(/*proceed=*/true);

  // Defer on WillRedirectRequest.
  // This will be observed as PROCEED again in the throttle runner.
  event_expectation().ExpectWillRedirectRequest(
      content::NavigationThrottle::DEFER);
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Proceed later on WillProcessResponse, observed as DEFER without calling the
  // internal throttle.
  navigation_simulator->ReadyToCommit();
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred redirect request.
  event_expectation().ExpectWillProcessResponse(
      content::NavigationThrottle::PROCEED);
  event_expectation().Resolve(/*proceed=*/true);
  EXPECT_FALSE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and resumes
// the request before WillRedirectRequest. It asks DEFER on WillRedirectRequest
// again and resumes the request to cancel after WillProcessResponse.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferOnStartAndRedirectAndResolveRedirectToCancelAfterResponse) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to proceed.
  event_expectation().Resolve(/*proceed=*/true);

  // Defer on WillRedirectRequest.
  // This will be observed as PROCEED again in the throttle runner.
  event_expectation().ExpectWillRedirectRequest(
      content::NavigationThrottle::DEFER);
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Proceed later on WillProcessResponse, observed as DEFER without calling the
  // internal throttle.
  navigation_simulator->ReadyToCommit();
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred redirect request to cancel.
  event_expectation().Resolve(/*proceed=*/false,
                              content::NavigationThrottle::CANCEL);
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and resumes
// the request before WillRedirectRequest. It asks DEFER on WillRedirectRequest
// again and resumes the request after WillProcessResponse that will cancel.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferOnStartAndRedirectAndResolveRedirectAfterResponseToBeCanceled) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to proceed.
  event_expectation().Resolve(/*proceed=*/true);

  // Defer on WillRedirectRequest.
  // This will be observed as PROCEED again in the throttle runner.
  event_expectation().ExpectWillRedirectRequest(
      content::NavigationThrottle::DEFER);
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Proceed later on WillProcessResponse, observed as DEFER without calling the
  // internal throttle.
  navigation_simulator->ReadyToCommit();
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred redirect request, and the throttle cancel on
  // WillProcessResponse.
  event_expectation().ExpectWillProcessResponse(
      content::NavigationThrottle::CANCEL);
  event_expectation().Resolve(/*proceed=*/true);
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and resumes
// the request to cancel after WillRedirectRequest.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferOnStartAndResolveToCancelAfterRedirect) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Defer on WillRedirectRequest without calling the internal throttle.
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to cancel.
  event_expectation().Resolve(/*proceed=*/false,
                              content::NavigationThrottle::CANCEL);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and resumes
// the request after WillRedirectRequest that will cancel the request.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferOnStartAndResolveAfterRedirectToBeCanceled) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Defer on WillRedirectRequest without calling the internal throttle.
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to proceed.
  event_expectation().ExpectWillRedirectRequest(
      content::NavigationThrottle::CANCEL);
  event_expectation().Resolve(/*proceed=*/true);

  // Deferred cancel on the redirect will be resolved.
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and resumes
// the request to cancel after WillProcessResponse.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferOnStartAndResolveToCancelAfterResponse) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Defer on WillProcessResponse without calling the internal throttle.
  navigation_simulator->ReadyToCommit();
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to cancel.
  event_expectation().Resolve(/*proceed=*/false,
                              content::NavigationThrottle::CANCEL);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and resumes
// the request after WillProcessResponse that will cancel the request.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferOnStartAndResolveAfterResponseToBeCanceled) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Defer on WillProcessResponse without calling the internal throttle.
  navigation_simulator->ReadyToCommit();
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to proceed.
  event_expectation().ExpectWillProcessResponse(
      content::NavigationThrottle::CANCEL);
  event_expectation().Resolve(/*proceed=*/true);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and
// WillRedirectRequest again before it resumes the request. It resumes the
// request again before WillProcessResponse to proceed.
TEST_F(ProceedUntilResponseNavigationThrottleTest, DeferWithPending) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Defer on WillRedirectRequest again.
  // This will be observed also as DEFER in the throttle runner.
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to proceed.
  event_expectation().ExpectWillRedirectRequest(
      content::NavigationThrottle::DEFER);
  event_expectation().Resolve(/*proceed=*/true);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred redirect request to proceed.
  event_expectation().Resolve(/*proceed=*/true);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Proceed on WillProcessResponse.
  event_expectation().ExpectWillProcessResponse();
  navigation_simulator->ReadyToCommit();
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  EXPECT_FALSE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and
// WillRedirectRequest again before it resumes the request. It resumes the
// request again before WillProcessResponse to cancel.
TEST_F(ProceedUntilResponseNavigationThrottleTest, DeferWithPendingAndCancel) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Defer on WillRedirectRequest again.
  // This will be observed also as DEFER in the throttle runner.
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to proceed.
  event_expectation().ExpectWillRedirectRequest(
      content::NavigationThrottle::DEFER);
  event_expectation().Resolve(/*proceed=*/true);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred redirect request to proceed.
  event_expectation().Resolve(/*proceed=*/true);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Proceed on WillProcessResponse.
  event_expectation().ExpectWillProcessResponse(
      content::NavigationThrottle::CANCEL);
  navigation_simulator->ReadyToCommit();
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            navigation_simulator->GetLastThrottleCheckResult());
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and
// WillRedirectRequest again before it resumes the request. It resumes the
// request to cancel again before WillProcessResponse.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferWithPendingAndResolveToCancel) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Defer on WillRedirectRequest again.
  // This will be observed also as DEFER in the throttle runner.
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to proceed.
  event_expectation().ExpectWillRedirectRequest(
      content::NavigationThrottle::DEFER);
  event_expectation().Resolve(/*proceed=*/true);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred redirect request to proceed.
  event_expectation().Resolve(/*proceed=*/false,
                              content::NavigationThrottle::CANCEL);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Proceed on WillProcessResponse.
  navigation_simulator->ReadyToCommit();
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and
// WillRedirectRequest again before it resumes the request. It resumes the
// request again after WillProcessResponse to proceed.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferWithPendingAndDeferResponse) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Defer on WillRedirectRequest again.
  // This will be observed also as DEFER in the throttle runner.
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to proceed.
  event_expectation().ExpectWillRedirectRequest(
      content::NavigationThrottle::DEFER);
  event_expectation().Resolve(/*proceed=*/true);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Defer on WillProcessResponse.
  navigation_simulator->ReadyToCommit();
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred redirect request to proceed.
  event_expectation().ExpectWillProcessResponse();
  event_expectation().Resolve(/*proceed=*/true);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_FALSE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and
// WillRedirectRequest again before it resumes the request. It resumes the
// request again after WillProcessResponse to cancel.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferWithPendingAndResolveResponseToCancel) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Defer on WillRedirectRequest again.
  // This will be observed also as DEFER in the throttle runner.
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to proceed.
  event_expectation().ExpectWillRedirectRequest(
      content::NavigationThrottle::DEFER);
  event_expectation().Resolve(/*proceed=*/true);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Defer on WillProcessResponse.
  navigation_simulator->ReadyToCommit();
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred redirect request to proceed.
  event_expectation().ExpectWillProcessResponse(
      content::NavigationThrottle::CANCEL);
  event_expectation().Resolve(/*proceed=*/true);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}

// Test a case, the internal throttle asks DEFER on WillStartRequest and
// WillRedirectRequest again before it resumes the request. It resumes the
// request to cancel again after WillProcessResponse.
TEST_F(ProceedUntilResponseNavigationThrottleTest,
       DeferWithPendingAndResolveResponseAndCancel) {
  // Defer on WillStartRequest.
  // This will be observed as PROCEED in the throttle runner.
  event_expectation().ExpectWillStartRequest(
      content::NavigationThrottle::DEFER);
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  event_expectation().CheckExpectations();

  // Defer on WillRedirectRequest again.
  // This will be observed also as DEFER in the throttle runner.
  navigation_simulator->Redirect(GURL("http://alt.example.com/"));
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred request to proceed.
  event_expectation().ExpectWillRedirectRequest(
      content::NavigationThrottle::DEFER);
  event_expectation().Resolve(/*proceed=*/true);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Defer on WillProcessResponse.
  navigation_simulator->ReadyToCommit();
  ASSERT_TRUE(navigation_simulator->IsDeferred());
  event_expectation().CheckExpectations();

  // Resolve the deferred redirect request to cancel.
  event_expectation().Resolve(/*proceed=*/false,
                              content::NavigationThrottle::CANCEL);
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_TRUE(navigation_simulator->HasFailed());
  event_expectation().CheckExpectations();
}
