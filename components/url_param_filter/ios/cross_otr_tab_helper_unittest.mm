// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_param_filter/ios/cross_otr_tab_helper.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ios/web/public/test/fakes/fake_browser_state.h"
#include "ios/web/public/test/fakes/fake_navigation_context.h"
#include "ios/web/public/test/fakes/fake_web_state.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {
class FakeNavigationContext;
class FakeBrowserState;
}  // namespace web

namespace url_param_filter {

namespace {
const char StandardResponseCodeMetric[] =
    "Navigation.CrossOtr.ContextMenu.ResponseCode";
const char ExperimentalResponseCodeMetric[] =
    "Navigation.CrossOtr.ContextMenu.ResponseCodeExperimental";
const char StandardRefreshCountMetric[] =
    "Navigation.CrossOtr.ContextMenu.RefreshCount";
const char ExperimentalRefreshCountMetric[] =
    "Navigation.CrossOtr.ContextMenu.RefreshCountExperimental";
}  // namespace

class CrossOtrTabHelperTest
    : public PlatformTest,
      public ::testing::WithParamInterface<ClassificationExperimentStatus> {
 public:
  CrossOtrTabHelperTest() : PlatformTest() {
    // Create new OTR web_state to navigate to.
    std::unique_ptr<web::FakeWebState> unique_web_state =
        std::make_unique<web::FakeWebState>();
    web::FakeBrowserState browser_state;
    browser_state.SetOffTheRecord(true);
    unique_web_state->SetBrowserState(&browser_state);
    web_state_ = unique_web_state.get();

    // Initialize observer with this web_state.
    CrossOtrTabHelper::CreateForWebState(web_state_);
    observer_ = CrossOtrTabHelper::FromWebState(web_state_);

    observer_->SetExperimentalStatus(GetParam());
    // Create Cross OTR navigation which...
    // (1) navigates into OTR
    context_.SetWebState(std::move(unique_web_state));
    // (2) transitions into OTR from "Open In Incognito"
    context_.SetPageTransition(ui::PAGE_TRANSITION_TYPED);
    // (3) is a user-initiated navigation
    context_.SetHasUserGesture(true);
  }

 protected:
  web::FakeWebState* web_state() { return web_state_; };
  CrossOtrTabHelper* observer() { return observer_; };
  web::FakeNavigationContext* context() { return &context_; };
  bool IsExperimental() {
    return GetParam() == ClassificationExperimentStatus::EXPERIMENTAL;
  }

 private:
  base::test::ScopedFeatureList features_;
  web::FakeWebState* web_state_;
  CrossOtrTabHelper* observer_;
  web::FakeNavigationContext context_;
};

TEST_P(CrossOtrTabHelperTest, ObserverAttached) {
  auto web_state = std::make_unique<web::FakeWebState>();
  CrossOtrTabHelper::CreateForWebState(web_state.get());
  // The observer should have been attached.
  ASSERT_NE(CrossOtrTabHelper::FromWebState(web_state.get()), nullptr);
}

TEST_P(CrossOtrTabHelperTest, CreateKey) {
  auto web_state = std::make_unique<web::FakeWebState>();
  CrossOtrTabHelper::CreateForWebState(web_state.get());
  // The observer should have been attached.
  ASSERT_NE(CrossOtrTabHelper::FromWebState(web_state.get()), nullptr);
}
TEST_P(CrossOtrTabHelperTest, DuplicateCreateKey) {
  auto web_state = std::make_unique<web::FakeWebState>();
  CrossOtrTabHelper::CreateForWebState(web_state.get());
  CrossOtrTabHelper::CreateForWebState(web_state.get());
  // The observer should have been attached.
  ASSERT_NE(CrossOtrTabHelper::FromWebState(web_state.get()), nullptr);
}

TEST_P(CrossOtrTabHelperTest, TransitionsWithNonTypedLink_NotCrossOtr) {
  // Create new non-OTR web_state to navigate to.
  std::unique_ptr<web::FakeWebState> unique_web_state =
      std::make_unique<web::FakeWebState>();
  web::FakeBrowserState browser_state;
  browser_state.SetOffTheRecord(true);
  unique_web_state->SetBrowserState(&browser_state);
  web::FakeWebState* web_state = unique_web_state.get();

  // Initialize observer with this web_state.
  CrossOtrTabHelper::CreateForWebState(web_state);
  CrossOtrTabHelper* observer = CrossOtrTabHelper::FromWebState(web_state);
  ASSERT_NE(observer, nullptr);

  // Create navigation context.
  web::FakeNavigationContext context;
  context.SetWebState(std::move(unique_web_state));
  context.SetPageTransition(ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  context.SetHasUserGesture(true);

  // We don't enter cross-OTR state since we didn't navigate to OTR via "Open In
  // Incognito".
  observer->DidStartNavigation(web_state, &context);
  EXPECT_FALSE(observer->GetCrossOtrStateForTesting());
}

TEST_P(CrossOtrTabHelperTest, NonUserInitiatedNavigation_NotCrossOtr) {
  // Create new non-OTR web_state to navigate to.
  std::unique_ptr<web::FakeWebState> unique_web_state =
      std::make_unique<web::FakeWebState>();
  web::FakeBrowserState browser_state;
  browser_state.SetOffTheRecord(true);
  unique_web_state->SetBrowserState(&browser_state);
  web::FakeWebState* web_state = unique_web_state.get();

  // Initialize observer with this web_state.
  CrossOtrTabHelper::CreateForWebState(web_state);
  CrossOtrTabHelper* observer = CrossOtrTabHelper::FromWebState(web_state);
  ASSERT_NE(observer, nullptr);

  // Create navigation context.
  web::FakeNavigationContext context;
  context.SetWebState(std::move(unique_web_state));
  context.SetPageTransition(ui::PAGE_TRANSITION_TYPED);
  context.SetHasUserGesture(false);

  // We don't enter cross-OTR state since we didn't navigate to OTR via
  // user-initiated press.
  observer->DidStartNavigation(web_state, &context);
  EXPECT_FALSE(observer->GetCrossOtrStateForTesting());
}

TEST_P(CrossOtrTabHelperTest, FinishedNavigation) {
  base::HistogramTester histogram_tester;
  ASSERT_NE(observer(), nullptr);

  scoped_refptr<net::HttpResponseHeaders> response =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  context()->SetResponseHeaders(response);

  observer()->DidStartNavigation(web_state(), context());
  observer()->DidFinishNavigation(web_state(), context());

  ASSERT_TRUE(observer()->GetCrossOtrStateForTesting());
  histogram_tester.ExpectTotalCount(StandardResponseCodeMetric, 1);
  histogram_tester.ExpectUniqueSample(
      StandardResponseCodeMetric, net::HttpUtil::MapStatusCodeForHistogram(200),
      1);
  if (IsExperimental()) {
    histogram_tester.ExpectTotalCount(ExperimentalResponseCodeMetric, 1);
    histogram_tester.ExpectUniqueSample(
        ExperimentalResponseCodeMetric,
        net::HttpUtil::MapStatusCodeForHistogram(200), 1);
  }
}

TEST_P(CrossOtrTabHelperTest, BadNavigationResponse) {
  base::HistogramTester histogram_tester;
  ASSERT_NE(observer(), nullptr);
  context()->SetResponseHeaders(nullptr);
  observer()->DidStartNavigation(web_state(), context());
  observer()->DidFinishNavigation(web_state(), context());

  histogram_tester.ExpectTotalCount(StandardResponseCodeMetric, 0);
  if (IsExperimental()) {
    histogram_tester.ExpectTotalCount(ExperimentalResponseCodeMetric, 0);
  }
  // The observer should not cease observation after first load, regardless of
  // whether the headers include a response code. We still want to see
  // the refresh count.
  ASSERT_NE(CrossOtrTabHelper::FromWebState(web_state()), nullptr);
}
TEST_P(CrossOtrTabHelperTest, RefreshedAfterNavigation) {
  base::HistogramTester histogram_tester;
  ASSERT_NE(observer(), nullptr);

  scoped_refptr<net::HttpResponseHeaders> response =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  context()->SetResponseHeaders(response);

  // Perform the initial navigation.
  observer()->DidStartNavigation(web_state(), context());
  observer()->DidFinishNavigation(web_state(), context());

  // Simulate a reload.
  context()->SetPageTransition(ui::PAGE_TRANSITION_RELOAD);
  observer()->DidStartNavigation(web_state(), context());
  observer()->DidFinishNavigation(web_state(), context());
  observer()->WebStateDestroyed(web_state());

  histogram_tester.ExpectTotalCount(StandardResponseCodeMetric, 1);
  histogram_tester.ExpectUniqueSample(
      StandardResponseCodeMetric, net::HttpUtil::MapStatusCodeForHistogram(200),
      1);
  histogram_tester.ExpectTotalCount(StandardRefreshCountMetric, 1);
  ASSERT_EQ(histogram_tester.GetTotalSum(StandardRefreshCountMetric), 1);
  if (IsExperimental()) {
    histogram_tester.ExpectTotalCount(ExperimentalResponseCodeMetric, 1);
    histogram_tester.ExpectUniqueSample(
        ExperimentalResponseCodeMetric,
        net::HttpUtil::MapStatusCodeForHistogram(200), 1);
    histogram_tester.ExpectTotalCount(ExperimentalRefreshCountMetric, 1);
    ASSERT_EQ(histogram_tester.GetTotalSum(ExperimentalRefreshCountMetric), 1);
  }
}
TEST_P(CrossOtrTabHelperTest, UncommittedNavigationWithRefresh) {
  base::HistogramTester histogram_tester;
  ASSERT_NE(observer(), nullptr);

  scoped_refptr<net::HttpResponseHeaders> response =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  context()->SetResponseHeaders(response);

  observer()->DidStartNavigation(web_state(), context());
  observer()->DidFinishNavigation(web_state(), context());
  ASSERT_TRUE(observer()->GetCrossOtrStateForTesting());

  // Finish a non-reload navigation, but one that isn't committed (so no
  // actual navigation away from the monitored page)
  context()->SetPageTransition(ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  context()->SetIsSameDocument(false);
  context()->SetHasCommitted(false);
  observer()->DidStartNavigation(web_state(), context());
  observer()->DidFinishNavigation(web_state(), context());

  observer()->DidStartNavigation(web_state(), context());
  observer()->DidFinishNavigation(web_state(), context());
  // We just observed another navigation not due to a client redirect, so
  // should no longer be in the cross-OTR state.
  ASSERT_FALSE(observer()->GetCrossOtrStateForTesting());

  // After that uncommitted navigation, trigger a refresh, then destroy.
  context()->SetPageTransition(ui::PAGE_TRANSITION_RELOAD);
  observer()->DidStartNavigation(web_state(), context());
  observer()->DidFinishNavigation(web_state(), context());
  observer()->WebStateDestroyed(web_state());

  // We had 1 relevant refresh.
  histogram_tester.ExpectTotalCount(StandardResponseCodeMetric, 1);
  histogram_tester.ExpectUniqueSample(
      StandardResponseCodeMetric, net::HttpUtil::MapStatusCodeForHistogram(200),
      1);
  histogram_tester.ExpectTotalCount(StandardRefreshCountMetric, 1);
  ASSERT_EQ(histogram_tester.GetTotalSum(StandardRefreshCountMetric), 1);
  if (IsExperimental()) {
    histogram_tester.ExpectTotalCount(ExperimentalResponseCodeMetric, 1);
    histogram_tester.ExpectUniqueSample(
        ExperimentalResponseCodeMetric,
        net::HttpUtil::MapStatusCodeForHistogram(200), 1);
    histogram_tester.ExpectTotalCount(ExperimentalRefreshCountMetric, 1);
    ASSERT_EQ(histogram_tester.GetTotalSum(ExperimentalRefreshCountMetric), 1);
  }
}
TEST_P(CrossOtrTabHelperTest, MultipleRefreshesAfterNavigation) {
  base::HistogramTester histogram_tester;
  ASSERT_NE(observer(), nullptr);

  scoped_refptr<net::HttpResponseHeaders> response =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  context()->SetResponseHeaders(response);
  observer()->DidStartNavigation(web_state(), context());
  observer()->DidFinishNavigation(web_state(), context());

  // Reload twice and ensure the count is persisted.
  context()->SetPageTransition(ui::PAGE_TRANSITION_RELOAD);
  observer()->DidStartNavigation(web_state(), context());
  // With the refresh navigation started, we are no longer in cross-OTR mode.
  ASSERT_FALSE(observer()->GetCrossOtrStateForTesting());
  observer()->DidFinishNavigation(web_state(), context());
  observer()->DidStartNavigation(web_state(), context());
  observer()->DidFinishNavigation(web_state(), context());

  // Navigating away means no more observer.
  context()->SetPageTransition(ui::PAGE_TRANSITION_LINK);
  context()->SetIsSameDocument(false);
  context()->SetHasCommitted(true);
  observer()->DidStartNavigation(web_state(), context());
  observer()->DidFinishNavigation(web_state(), context());

  ASSERT_EQ(CrossOtrTabHelper::FromWebState(web_state()), nullptr);

  histogram_tester.ExpectTotalCount(StandardResponseCodeMetric, 1);
  histogram_tester.ExpectUniqueSample(
      StandardResponseCodeMetric, net::HttpUtil::MapStatusCodeForHistogram(200),
      1);
  histogram_tester.ExpectTotalCount(StandardRefreshCountMetric, 1);
  ASSERT_EQ(histogram_tester.GetTotalSum(StandardRefreshCountMetric), 2);
  if (IsExperimental()) {
    histogram_tester.ExpectTotalCount(ExperimentalResponseCodeMetric, 1);
    histogram_tester.ExpectUniqueSample(
        ExperimentalResponseCodeMetric,
        net::HttpUtil::MapStatusCodeForHistogram(200), 1);
    histogram_tester.ExpectTotalCount(ExperimentalRefreshCountMetric, 1);
    ASSERT_EQ(histogram_tester.GetTotalSum(ExperimentalRefreshCountMetric), 2);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    CrossOtrTabHelperTest,
    ::testing::Values(ClassificationExperimentStatus::EXPERIMENTAL,
                      ClassificationExperimentStatus::NON_EXPERIMENTAL));

}  // namespace url_param_filter
