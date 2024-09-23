// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/wifi_network_metrics_helper.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kHistogramLoggedIn[] = "Network.Ash.WiFi.Hidden.LoggedIn";
const char kHistogramNotLoggedIn[] = "Network.Ash.WiFi.Hidden.NotLoggedIn";

}  // namespace

class WifiNetworkMetricsHelperTest : public testing::Test {
 public:
  WifiNetworkMetricsHelperTest() = default;
  WifiNetworkMetricsHelperTest(const WifiNetworkMetricsHelperTest&) = delete;
  WifiNetworkMetricsHelperTest& operator=(const WifiNetworkMetricsHelperTest&) =
      delete;
  ~WifiNetworkMetricsHelperTest() override = default;

  void SetUp() override {
    LoginState::Initialize();
    LoginState::Get()->set_always_logged_in(false);

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override { LoginState::Shutdown(); }

  void SetLoggedIn(bool logged_in) const {
    LoginState::Get()->SetLoggedInState(
        logged_in ? LoginState::LoggedInState::LOGGED_IN_ACTIVE
                  : LoginState::LoggedInState::LOGGED_IN_NONE,
        logged_in ? LoginState::LoggedInUserType::LOGGED_IN_USER_REGULAR
                  : LoginState::LoggedInUserType::LOGGED_IN_USER_NONE);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void ExpectHiddenCounts(const char* histogram,
                          size_t hidden_count,
                          size_t not_hidden_count) {
    histogram_tester_->ExpectBucketCount(histogram, true, hidden_count);
    histogram_tester_->ExpectBucketCount<>(histogram, false, not_hidden_count);
    histogram_tester_->ExpectTotalCount(histogram,
                                        hidden_count + not_hidden_count);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(WifiNetworkMetricsHelperTest, LogWifiNetworkCreation) {
  ExpectHiddenCounts(/*histogram=*/kHistogramLoggedIn,
                     /*hidden_count=*/0,
                     /*not_hidden_count=*/0);
  ExpectHiddenCounts(/*histogram=*/kHistogramNotLoggedIn,
                     /*hidden_count=*/0,
                     /*not_hidden_count=*/0);

  SetLoggedIn(true);

  WifiNetworkMetricsHelper::LogInitiallyConfiguredAsHidden(/*is_hidden=*/true);
  ExpectHiddenCounts(/*histogram=*/kHistogramLoggedIn,
                     /*hidden_count=*/1,
                     /*not_hidden_count=*/0);

  WifiNetworkMetricsHelper::LogInitiallyConfiguredAsHidden(/*is_hidden=*/false);
  ExpectHiddenCounts(/*histogram=*/kHistogramLoggedIn,
                     /*hidden_count=*/1,
                     /*not_hidden_count=*/1);
  ExpectHiddenCounts(/*histogram=*/kHistogramNotLoggedIn,
                     /*hidden_count=*/0,
                     /*not_hidden_count=*/0);

  SetLoggedIn(false);

  WifiNetworkMetricsHelper::LogInitiallyConfiguredAsHidden(/*is_hidden=*/true);
  ExpectHiddenCounts(/*histogram=*/kHistogramNotLoggedIn,
                     /*hidden_count=*/1,
                     /*not_hidden_count=*/0);

  WifiNetworkMetricsHelper::LogInitiallyConfiguredAsHidden(/*is_hidden=*/false);
  ExpectHiddenCounts(/*histogram=*/kHistogramLoggedIn,
                     /*hidden_count=*/1,
                     /*not_hidden_count=*/1);
  ExpectHiddenCounts(/*histogram=*/kHistogramNotLoggedIn,
                     /*hidden_count=*/1,
                     /*not_hidden_count=*/1);
}

}  // namespace ash
