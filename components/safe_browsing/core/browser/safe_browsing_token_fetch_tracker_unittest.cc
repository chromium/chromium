// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/safe_browsing_token_fetch_tracker.h"
#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingTokenFetchTrackerTest : public ::testing::Test {
 public:
  SafeBrowsingTokenFetchTrackerTest() {}

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(SafeBrowsingTokenFetchTrackerTest, Success) {
  SafeBrowsingTokenFetchTracker fetcher;
  std::string access_token;
  int request_id = fetcher.StartTrackingTokenFetch(
      base::BindOnce([](std::string* target_token,
                        const std::string& token) { *target_token = token; },
                     &access_token),
      base::BindOnce([](int request_id) {}));

  fetcher.OnTokenFetchComplete(request_id, "token");
  EXPECT_EQ(access_token, "token");
}

TEST_F(SafeBrowsingTokenFetchTrackerTest, MultipleRequests) {
  SafeBrowsingTokenFetchTracker fetcher;
  std::string access_token1;
  std::string access_token2;
  int request_id1 = fetcher.StartTrackingTokenFetch(
      base::BindOnce([](std::string* target_token,
                        const std::string& token) { *target_token = token; },
                     &access_token1),
      base::BindOnce([](int request_id) {}));
  int request_id2 = fetcher.StartTrackingTokenFetch(
      base::BindOnce([](std::string* target_token,
                        const std::string& token) { *target_token = token; },
                     &access_token2),
      base::BindOnce([](int request_id) {}));

  fetcher.OnTokenFetchComplete(request_id2, "token2");
  EXPECT_EQ(access_token1, "");
  EXPECT_EQ(access_token2, "token2");

  fetcher.OnTokenFetchComplete(request_id1, "token1");
  EXPECT_EQ(access_token1, "token1");
  EXPECT_EQ(access_token2, "token2");
}

TEST_F(SafeBrowsingTokenFetchTrackerTest, FetcherDestruction) {
  auto fetcher = std::make_unique<SafeBrowsingTokenFetchTracker>();

  std::string access_token1;
  std::string access_token2 = "dummy_value";
  int request_id1 = fetcher->StartTrackingTokenFetch(
      base::BindOnce([](std::string* target_token,
                        const std::string& token) { *target_token = token; },
                     &access_token1),
      base::BindOnce([](int request_id) {}));
  fetcher->StartTrackingTokenFetch(
      base::BindOnce([](std::string* target_token,
                        const std::string& token) { *target_token = token; },
                     &access_token2),
      base::BindOnce([](int request_id) {}));

  fetcher->OnTokenFetchComplete(request_id1, "token1");
  EXPECT_EQ(access_token1, "token1");
  EXPECT_EQ(access_token2, "dummy_value");

  fetcher.reset();

  // The second request was outstanding when the fetcher is destroyed, so it
  // should have been invoked with the empty string.
  EXPECT_EQ(access_token1, "token1");
  EXPECT_EQ(access_token2, "");
}

// Verifies that destruction of a SafeBrowsingTokenFetchTracker instance from
// within the client callback that the token was fetched doesn't cause a crash.
TEST_F(SafeBrowsingTokenFetchTrackerTest,
       FetcherDestroyedFromWithinOnTokenFetchedCallback) {
  // Destroyed in the token fetch callback.
  auto* fetcher = new SafeBrowsingTokenFetchTracker();

  std::string access_token;
  int request_id = fetcher->StartTrackingTokenFetch(
      base::BindOnce(
          [](std::string* target_token, SafeBrowsingTokenFetchTracker* fetcher,
             const std::string& token) {
            *target_token = token;
            delete fetcher;
          },
          &access_token, fetcher),
      base::BindOnce([](int request_id) {}));

  fetcher->OnTokenFetchComplete(request_id, "token");
  EXPECT_EQ(access_token, "token");
}

TEST_F(SafeBrowsingTokenFetchTrackerTest, Timeout) {
  SafeBrowsingTokenFetchTracker fetcher;
  std::string access_token1 = "dummy_value1";
  std::string access_token2 = "dummy_value2";
  bool on_timeout1_invoked = false;
  bool on_timeout2_invoked = false;
  int delay_before_second_request_from_ms =
      kTokenFetchTimeoutDelayFromMilliseconds / 2;

  fetcher.StartTrackingTokenFetch(
      base::BindOnce([](std::string* target_token,
                        const std::string& token) { *target_token = token; },
                     &access_token1),
      base::BindOnce([](bool* target_on_timeout_invoked,
                        int request_id) { *target_on_timeout_invoked = true; },
                     &on_timeout1_invoked));

  task_environment_.FastForwardBy(
      base::Milliseconds(delay_before_second_request_from_ms));
  fetcher.StartTrackingTokenFetch(
      base::BindOnce([](std::string* target_token,
                        const std::string& token) { *target_token = token; },
                     &access_token2),
      base::BindOnce([](bool* target_on_timeout_invoked,
                        int request_id) { *target_on_timeout_invoked = true; },
                     &on_timeout2_invoked));

  // Fast-forward to trigger the first request's timeout threshold, but not the
  // second.
  int time_to_trigger_first_timeout_from_ms =
      kTokenFetchTimeoutDelayFromMilliseconds -
      delay_before_second_request_from_ms;
  task_environment_.FastForwardBy(
      base::Milliseconds(time_to_trigger_first_timeout_from_ms));
  EXPECT_EQ(access_token1, "");
  EXPECT_TRUE(on_timeout1_invoked);
  EXPECT_EQ(access_token2, "dummy_value2");
  EXPECT_FALSE(on_timeout2_invoked);

  // Fast-forward to trigger the second request's timeout threshold.
  task_environment_.FastForwardBy(
      base::Milliseconds(kTokenFetchTimeoutDelayFromMilliseconds -
                         time_to_trigger_first_timeout_from_ms));
  EXPECT_EQ(access_token2, "");
  EXPECT_TRUE(on_timeout2_invoked);
}

}  // namespace safe_browsing
