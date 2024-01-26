// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/consent_throttle.h"
#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unified_consent {
namespace {

class TestUrlKeyedDataCollectionConsentHelper
    : public UrlKeyedDataCollectionConsentHelper {
 public:
  void SetConsentStateAndFireNotification(State state) {
    consent_state_ = state;
    FireOnStateChanged();
  }

  // UrlKeyedCollectionConsentHelper:
  State GetConsentState() override { return consent_state_; }

 private:
  State consent_state_ = State::kInitializing;
};

class ConsentThrottleTest : public testing::Test {
 protected:
  bool GetResultSynchronously(ConsentThrottle* throttle) {
    std::optional<bool> out_result;
    throttle->EnqueueRequest(
        base::BindLambdaForTesting([&](bool result) { out_result = result; }));
    EXPECT_TRUE(out_result.has_value())
        << "The throttle must have run the callback.";
    return *out_result;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ConsentThrottleTest, EnabledAndDisabledRunSynchronously) {
  auto helper = std::make_unique<TestUrlKeyedDataCollectionConsentHelper>();
  helper->SetConsentStateAndFireNotification(
      UrlKeyedDataCollectionConsentHelper::State::kDisabled);

  auto* helper_ptr = helper.get();

  auto consent_throttle = ConsentThrottle(std::move(helper));
  EXPECT_FALSE(GetResultSynchronously(&consent_throttle));

  helper_ptr->SetConsentStateAndFireNotification(
      UrlKeyedDataCollectionConsentHelper::State::kEnabled);
  EXPECT_TRUE(GetResultSynchronously(&consent_throttle));
}

TEST_F(ConsentThrottleTest, ExpireOldRequests) {
  auto helper = std::make_unique<TestUrlKeyedDataCollectionConsentHelper>();
  ASSERT_EQ(helper->GetConsentState(),
            UrlKeyedDataCollectionConsentHelper::State::kInitializing);

  auto consent_throttle = ConsentThrottle(std::move(helper));
  std::vector<bool> results;
  consent_throttle.EnqueueRequest(base::BindLambdaForTesting(
      [&](bool result) { results.push_back(result); }));

  EXPECT_TRUE(results.empty()) << "Callback should not be run immediately.";
  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_TRUE(results.empty()) << "Callback should not be run after 3 seconds.";

  // Add another request while the first request is still pending.
  consent_throttle.EnqueueRequest(base::BindLambdaForTesting(
      [&](bool result) { results.push_back(result); }));

  task_environment_.FastForwardBy(base::Seconds(5));
  ASSERT_EQ(results.size(), 2U) << "Both callbacks should expire as false.";
  EXPECT_FALSE(results[0]);
  EXPECT_FALSE(results[1]);

  // Enqueuing another one should restart the timer, which should expire after
  // a second delay of 5 seconds.
  consent_throttle.EnqueueRequest(base::BindLambdaForTesting(
      [&](bool result) { results.push_back(result); }));
  EXPECT_EQ(results.size(), 2U) << "Callback should not be run immediately.";
  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_EQ(results.size(), 2U);
  task_environment_.FastForwardBy(base::Seconds(3));
  ASSERT_EQ(results.size(), 3U);
  EXPECT_FALSE(results[2]);
}

TEST_F(ConsentThrottleTest, InitializationFulfillsAllQueuedRequests) {
  auto helper = std::make_unique<TestUrlKeyedDataCollectionConsentHelper>();
  ASSERT_EQ(helper->GetConsentState(),
            UrlKeyedDataCollectionConsentHelper::State::kInitializing);

  auto* helper_ptr = helper.get();
  auto consent_throttle = ConsentThrottle(std::move(helper));

  // Enqueue two requests, 2 seconds apart.
  std::vector<bool> results;
  consent_throttle.EnqueueRequest(base::BindLambdaForTesting(
      [&](bool result) { results.push_back(result); }));
  ASSERT_TRUE(results.empty());
  task_environment_.FastForwardBy(base::Seconds(2));
  consent_throttle.EnqueueRequest(base::BindLambdaForTesting(
      [&](bool result) { results.push_back(result); }));
  ASSERT_TRUE(results.empty()) << "Still nothing should be run yet.";

  helper_ptr->SetConsentStateAndFireNotification(
      UrlKeyedDataCollectionConsentHelper::State::kEnabled);
  ASSERT_EQ(results.size(), 2U)
      << "Requests should have been immediately fulfilled as true.";
  EXPECT_TRUE(results[0]);
  EXPECT_TRUE(results[1]);
}

TEST_F(ConsentThrottleTest, InitializationDisabledCase) {
  auto helper = std::make_unique<TestUrlKeyedDataCollectionConsentHelper>();
  ASSERT_EQ(helper->GetConsentState(),
            UrlKeyedDataCollectionConsentHelper::State::kInitializing);

  auto* helper_ptr = helper.get();
  auto consent_throttle = ConsentThrottle(std::move(helper));

  std::vector<bool> results;
  consent_throttle.EnqueueRequest(base::BindLambdaForTesting(
      [&](bool result) { results.push_back(result); }));
  ASSERT_TRUE(results.empty());

  helper_ptr->SetConsentStateAndFireNotification(
      UrlKeyedDataCollectionConsentHelper::State::kDisabled);
  ASSERT_EQ(results.size(), 1U);
  EXPECT_FALSE(results[0]);
}

// In production, sometimes the callback to a request enqueues a new request.
// This tests this case and fixes the crash in https://crbug.com/1483454.
TEST_F(ConsentThrottleTest, CallbacksMakingNewRequests) {
  auto helper = std::make_unique<TestUrlKeyedDataCollectionConsentHelper>();
  ASSERT_EQ(helper->GetConsentState(),
            UrlKeyedDataCollectionConsentHelper::State::kInitializing);

  auto consent_throttle = ConsentThrottle(std::move(helper));
  std::vector<bool> results;

  // These two blocks are identical. The crash is reliably triggered when
  // adding two of these. Probably having two pushes the vector to reallocate
  // while iterating.
  consent_throttle.EnqueueRequest(base::BindLambdaForTesting([&](bool result) {
    results.push_back(result);
    consent_throttle.EnqueueRequest(base::BindLambdaForTesting(
        [&](bool result2) { results.push_back(result2); }));
  }));
  consent_throttle.EnqueueRequest(base::BindLambdaForTesting([&](bool result) {
    results.push_back(result);
    consent_throttle.EnqueueRequest(base::BindLambdaForTesting(
        [&](bool result2) { results.push_back(result2); }));
  }));

  // New requests added during iteration live as long as the NEXT timeout.
  task_environment_.FastForwardBy(base::Seconds(6));
  EXPECT_EQ(results.size(), 2U);
  task_environment_.FastForwardBy(base::Seconds(6));
  EXPECT_EQ(results.size(), 4U);
}

}  // namespace
}  // namespace unified_consent
