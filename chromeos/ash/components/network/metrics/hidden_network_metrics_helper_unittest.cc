// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/metrics/hidden_network_metrics_helper.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

const char kHistogramLoggedIn[] = "Network.Ash.WiFi.Hidden.LoggedIn";
const char kHistogramNotLoggedIn[] = "Network.Ash.WiFi.Hidden.NotLoggedIn";

void ErrorCallback(const std::string& error_name) {
  ADD_FAILURE() << "Unexpected error: " << error_name;
}

// Helper function to create a WiFi network using NetworkConfigurationHandler.
void CreateTestShillConfiguration(bool is_hidden) {
  static int guid_index = 0;

  base::Value properties(base::Value::Type::DICTIONARY);

  properties.SetKey(shill::kGuidProperty,
                    base::Value(base::StringPrintf("guid-%i", guid_index++)));
  properties.SetKey(shill::kTypeProperty, base::Value(shill::kTypeWifi));
  properties.SetKey(shill::kStateProperty, base::Value(shill::kStateIdle));
  properties.SetKey(shill::kWifiHiddenSsid, base::Value(is_hidden));
  properties.SetKey(shill::kProfileProperty,
                    base::Value(NetworkProfileHandler::GetSharedProfilePath()));

  NetworkHandler::Get()
      ->network_configuration_handler()
      ->CreateShillConfiguration(properties, base::DoNothing(),
                                 base::BindOnce(&ErrorCallback));
  base::RunLoop().RunUntilIdle();
}

}  // namespace

class HiddenNetworkMetricsHelperTest : public testing::Test {
 public:
  HiddenNetworkMetricsHelperTest() = default;
  HiddenNetworkMetricsHelperTest(const HiddenNetworkMetricsHelperTest&) =
      delete;
  HiddenNetworkMetricsHelperTest& operator=(
      const HiddenNetworkMetricsHelperTest&) = delete;
  ~HiddenNetworkMetricsHelperTest() override = default;

  void SetUp() override {
    LoginState::Initialize();
    LoginState::Get()->set_always_logged_in(false);

    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    network_handler_test_helper_->ClearServices();
  }

  void TearDown() override {
    network_handler_test_helper_->ClearServices();
    network_handler_test_helper_.reset();

    LoginState::Shutdown();
  }

  void SetLoggedIn(bool logged_in) const {
    LoginState::Get()->SetLoggedInState(
        logged_in ? LoginState::LoggedInState::LOGGED_IN_ACTIVE
                  : LoginState::LoggedInState::LOGGED_IN_NONE,
        logged_in ? LoginState::LoggedInUserType::LOGGED_IN_USER_OWNER
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
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
};

TEST_F(HiddenNetworkMetricsHelperTest, LogHiddenNetworkCreation) {
  ExpectHiddenCounts(/*histogram=*/kHistogramLoggedIn,
                     /*hidden_count=*/0,
                     /*not_hidden_count=*/0);
  ExpectHiddenCounts(/*histogram=*/kHistogramNotLoggedIn,
                     /*hidden_count=*/0,
                     /*not_hidden_count=*/0);

  SetLoggedIn(true);

  CreateTestShillConfiguration(/*is_hidden=*/true);
  ExpectHiddenCounts(/*histogram=*/kHistogramLoggedIn,
                     /*hidden_count=*/1,
                     /*not_hidden_count=*/0);

  CreateTestShillConfiguration(/*is_hidden=*/false);
  ExpectHiddenCounts(/*histogram=*/kHistogramLoggedIn,
                     /*hidden_count=*/1,
                     /*not_hidden_count=*/1);
  ExpectHiddenCounts(/*histogram=*/kHistogramNotLoggedIn,
                     /*hidden_count=*/0,
                     /*not_hidden_count=*/0);

  SetLoggedIn(false);

  CreateTestShillConfiguration(/*is_hidden=*/true);
  ExpectHiddenCounts(/*histogram=*/kHistogramNotLoggedIn,
                     /*hidden_count=*/1,
                     /*not_hidden_count=*/0);

  CreateTestShillConfiguration(/*is_hidden=*/false);
  ExpectHiddenCounts(/*histogram=*/kHistogramLoggedIn,
                     /*hidden_count=*/1,
                     /*not_hidden_count=*/1);
  ExpectHiddenCounts(/*histogram=*/kHistogramNotLoggedIn,
                     /*hidden_count=*/1,
                     /*not_hidden_count=*/1);
}

}  // namespace ash
