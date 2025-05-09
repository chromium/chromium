// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/intercept_navigation_throttle.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::test::RunOnceCallback;
using base::test::RunOnceCallbackRepeatedly;
using content::NavigationThrottle;
using testing::_;
using testing::AllOf;
using testing::Eq;
using testing::Ne;
using testing::Not;
using testing::Property;
using testing::Return;

namespace navigation_interception {

namespace {

const char kTestUrl[] = "http://www.test.com/";

// The MS C++ compiler complains about not being able to resolve which url()
// method (const or non-const) to use if we use the Property matcher to check
// the return value of the NavigationParams::url() method.
// It is possible to suppress the error by specifying the types directly but
// that results in very ugly syntax, which is why these custom matchers are
// used instead.
MATCHER(NavigationHandleUrlIsTest, "") {
  return arg->GetURL() == kTestUrl;
}

MATCHER(IsPost, "") {
  return arg->IsPost();
}

}  // namespace

// MockInterceptCallbackReceiver ----------------------------------------------

class MockInterceptCallbackReceiver {
 public:
  MOCK_METHOD3(
      ShouldIgnoreNavigation,
      void(content::NavigationHandle* handle,
           bool should_run_async,
           InterceptNavigationThrottle::ResultCallback result_callback));
};

// InterceptNavigationThrottleTest ------------------------------------

class InterceptNavigationThrottleTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  InterceptNavigationThrottleTest()
      : mock_callback_receiver_(new MockInterceptCallbackReceiver()) {
    if (GetParam()) {
      scoped_feature_.InitAndEnableFeature(kAsyncCheck);
    } else {
      scoped_feature_.InitAndDisableFeature(kAsyncCheck);
    }
  }

  void CreateAndAddThrottle(
      InterceptNavigationThrottle::CheckCallback callback,
      base::RepeatingClosure request_finish_closure,
      content::NavigationThrottleRegistry& registry) {
    std::unique_ptr<InterceptNavigationThrottle> throttle =
        std::make_unique<InterceptNavigationThrottle>(
            registry, callback,
            navigation_interception::SynchronyMode::kAsync,
            request_finish_closure);
    throttle_ = throttle.get()->GetWeakPtrForTesting();
    registry.AddThrottle(std::move(throttle));
  }

  std::unique_ptr<content::TestNavigationThrottleInserter>
  CreateThrottleInserter() {
    return std::make_unique<content::TestNavigationThrottleInserter>(
        web_contents(),
        base::BindRepeating(
            &InterceptNavigationThrottleTest::CreateAndAddThrottle,
            base::Unretained(this),
            base::BindRepeating(
                &MockInterceptCallbackReceiver::ShouldIgnoreNavigation,
                base::Unretained(mock_callback_receiver_.get())),
            request_finish_closure_.Get()));
  }

  NavigationThrottle::ThrottleCheckResult SimulateNavigation(
      const GURL& url,
      std::vector<GURL> redirect_chain,
      bool is_post) {
    auto throttle_inserter = CreateThrottleInserter();
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
    auto failed = [](content::NavigationSimulator* sim) {
      return sim->GetLastThrottleCheckResult().action() !=
             NavigationThrottle::PROCEED;
    };

    if (is_post) {
      simulator->SetMethod("POST");
    }

    simulator->Start();
    if (failed(simulator.get())) {
      return simulator->GetLastThrottleCheckResult();
    }
    for (const GURL& redirect_url : redirect_chain) {
      simulator->Redirect(redirect_url);
      if (failed(simulator.get())) {
        return simulator->GetLastThrottleCheckResult();
      }
    }
    simulator->Commit();
    return simulator->GetLastThrottleCheckResult();
  }

  void OnCheckComplete(bool should_ignore) {
    throttle_->OnCheckComplete(should_ignore);
  }

  base::test::ScopedFeatureList scoped_feature_;
  std::unique_ptr<MockInterceptCallbackReceiver> mock_callback_receiver_;
  base::MockRepeatingClosure request_finish_closure_;
  base::WeakPtr<InterceptNavigationThrottle> throttle_;
};

TEST_P(InterceptNavigationThrottleTest, AsyncRequestCompletesWhenRequested) {
  if (!GetParam()) {
    GTEST_SKIP();
  }
  ON_CALL(request_finish_closure_, Run()).WillByDefault([this]() {
    OnCheckComplete(false);
  });
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(NavigationHandleUrlIsTest(), _, _))
      .Times(2);
  EXPECT_CALL(request_finish_closure_, Run()).Times(2);
  NavigationThrottle::ThrottleCheckResult result =
      SimulateNavigation(GURL(kTestUrl), {GURL(kTestUrl)}, false);

  EXPECT_EQ(NavigationThrottle::PROCEED, result);
}

TEST_P(InterceptNavigationThrottleTest, AsyncRequestDefersWhenRequested) {
  if (!GetParam()) {
    GTEST_SKIP();
  }
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(NavigationHandleUrlIsTest(), Eq(true), _))
      .Times(2);
  EXPECT_CALL(
      *mock_callback_receiver_,
      ShouldIgnoreNavigation(NavigationHandleUrlIsTest(), Eq(false), _));
  EXPECT_CALL(request_finish_closure_, Run()).Times(2);
  ON_CALL(*mock_callback_receiver_, ShouldIgnoreNavigation(_, Eq(false), _))
      .WillByDefault(RunOnceCallback<2>(false));

  auto throttle_inserter = CreateThrottleInserter();
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(GURL(kTestUrl),
                                                            main_rfh());
  simulator->Start();
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult().action());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&, this] {
        EXPECT_TRUE(simulator->IsDeferred());
        OnCheckComplete(false);
      }));
  simulator->Redirect(GURL(kTestUrl));
  EXPECT_FALSE(simulator->IsDeferred());
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult().action());

  simulator->Redirect(GURL(kTestUrl));
  EXPECT_FALSE(simulator->IsDeferred());
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult().action());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&, this] {
        EXPECT_TRUE(simulator->IsDeferred());
        OnCheckComplete(false);
      }));
  simulator->Commit();

  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult().action());
}

TEST_P(InterceptNavigationThrottleTest, AsyncRequestDefersTwice) {
  if (!GetParam()) {
    GTEST_SKIP();
  }
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(NavigationHandleUrlIsTest(), Eq(true), _))
      .Times(1);
  EXPECT_CALL(
      *mock_callback_receiver_,
      ShouldIgnoreNavigation(NavigationHandleUrlIsTest(), Eq(false), _));
  EXPECT_CALL(request_finish_closure_, Run()).Times(1);

  auto throttle_inserter = CreateThrottleInserter();
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(GURL(kTestUrl),
                                                            main_rfh());
  simulator->Start();
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult().action());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&, this] {
        EXPECT_TRUE(simulator->IsDeferred());
        OnCheckComplete(false);
        EXPECT_TRUE(simulator->IsDeferred());
        OnCheckComplete(false);
      }));
  simulator->Redirect(GURL(kTestUrl));
  EXPECT_FALSE(simulator->IsDeferred());
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult().action());

  simulator->Commit();

  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult().action());
}

TEST_P(InterceptNavigationThrottleTest, RequestDefersWhenClientNotReady) {
  if (GetParam()) {
    GTEST_SKIP();
  }
  EXPECT_CALL(
      *mock_callback_receiver_,
      ShouldIgnoreNavigation(NavigationHandleUrlIsTest(), Eq(false), _));
  EXPECT_CALL(request_finish_closure_, Run()).Times(0);

  auto throttle_inserter = CreateThrottleInserter();
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(GURL(kTestUrl),
                                                            main_rfh());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&, this] {
        EXPECT_TRUE(simulator->IsDeferred());
        OnCheckComplete(false);
      }));
  simulator->Start();
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult().action());
  EXPECT_FALSE(simulator->IsDeferred());

  simulator->Commit();
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator->GetLastThrottleCheckResult().action());
}

TEST_P(InterceptNavigationThrottleTest,
       RequestCompletesIfNavigationNotIgnored) {
  ON_CALL(*mock_callback_receiver_, ShouldIgnoreNavigation(_, _, _))
      .WillByDefault(RunOnceCallback<2>(false));
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(NavigationHandleUrlIsTest(), _, _));
  EXPECT_CALL(request_finish_closure_, Run()).Times(0);
  NavigationThrottle::ThrottleCheckResult result =
      SimulateNavigation(GURL(kTestUrl), {}, false);

  EXPECT_EQ(NavigationThrottle::PROCEED, result);
}

TEST_P(InterceptNavigationThrottleTest, RequestCancelledIfNavigationIgnored) {
  ON_CALL(*mock_callback_receiver_, ShouldIgnoreNavigation(_, _, _))
      .WillByDefault(RunOnceCallback<2>(true));
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(NavigationHandleUrlIsTest(), _, _));
  EXPECT_CALL(request_finish_closure_, Run()).Times(0);
  NavigationThrottle::ThrottleCheckResult result =
      SimulateNavigation(GURL(kTestUrl), {}, false);

  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, result);
}

TEST_P(InterceptNavigationThrottleTest, CallbackIsPostFalseForGet) {
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(
                  AllOf(NavigationHandleUrlIsTest(), Not(IsPost())), _, _))
      .WillOnce(RunOnceCallback<2>(false));
  EXPECT_CALL(request_finish_closure_, Run()).Times(0);

  NavigationThrottle::ThrottleCheckResult result =
      SimulateNavigation(GURL(kTestUrl), {}, false);

  EXPECT_EQ(NavigationThrottle::PROCEED, result);
}

TEST_P(InterceptNavigationThrottleTest, CallbackIsPostTrueForPost) {
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(
                  AllOf(NavigationHandleUrlIsTest(), IsPost()), _, _))
      .WillOnce(RunOnceCallback<2>(false));
  EXPECT_CALL(request_finish_closure_, Run()).Times(0);

  NavigationThrottle::ThrottleCheckResult result =
      SimulateNavigation(GURL(kTestUrl), {}, true);

  EXPECT_EQ(NavigationThrottle::PROCEED, result);
}

TEST_P(InterceptNavigationThrottleTest,
       CallbackIsPostFalseForPostConvertedToGetBy302) {
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(
                  AllOf(NavigationHandleUrlIsTest(), IsPost()), _, _))
      .WillOnce(RunOnceCallback<2>(false));
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(
                  AllOf(NavigationHandleUrlIsTest(), Not(IsPost())), _, _))
      .WillOnce(RunOnceCallback<2>(false));
  EXPECT_CALL(request_finish_closure_, Run()).Times(0);

  NavigationThrottle::ThrottleCheckResult result =
      SimulateNavigation(GURL(kTestUrl), {GURL(kTestUrl)}, true);
  EXPECT_EQ(NavigationThrottle::PROCEED, result);
}

// Ensure POST navigations are cancelled before the start.
TEST_P(InterceptNavigationThrottleTest, PostNavigationCancelledAtStart) {
  ON_CALL(*mock_callback_receiver_, ShouldIgnoreNavigation(_, _, _))
      .WillByDefault(RunOnceCallback<2>(true));
  auto throttle_inserter = CreateThrottleInserter();
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(GURL(kTestUrl),
                                                            main_rfh());
  simulator->SetMethod("POST");
  simulator->Start();
  auto result = simulator->GetLastThrottleCheckResult();
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, result);
}

INSTANTIATE_TEST_SUITE_P(All,
                         InterceptNavigationThrottleTest,
                         testing::Values(true, false));

}  // namespace navigation_interception
