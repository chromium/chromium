// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_measurement.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "net/base/network_change_notifier.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace data_use_measurement {

class DataUseMeasurementTest : public testing::Test {
 public:
  DataUseMeasurementTest()
      : data_use_measurement_(
            network::TestNetworkConnectionTracker::GetInstance()) {
    // During the test it is expected to not have cellular connection.
    DCHECK(!net::NetworkChangeNotifier::IsConnectionCellular(
        net::NetworkChangeNotifier::GetConnectionType()));
  }

  // This function makes a user request and confirms that its effect is
  // reflected in proper histograms.
  void TestForAUserRequest(const std::string& target_dimension) {
    base::HistogramTester histogram_tester;
    data_use_measurement_.RecordTrafficSizeMetric(
        true /* is_user_traffic */, true /* is_downstream */,
        true /* is_tab_visible */, 5 /* bytest */);
    data_use_measurement_.RecordTrafficSizeMetric(
        true /* is_user_traffic */, false /* is_downstream */,
        true /* is_tab_visible */, 5 /* bytest */);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.User.Downstream." +
                                          target_dimension + kConnectionType,
                                      1);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.User.Upstream." +
                                          target_dimension + kConnectionType,
                                      1);
  }

  // This function makes a service request and confirms that its effect is
  // reflected in proper histograms.
  void TestForAServiceRequest(const std::string& target_dimension) {
    base::HistogramTester histogram_tester;
    data_use_measurement_.RecordTrafficSizeMetric(
        false /* is_user_traffic */, true /* is_downstream */,
        true /* is_tab_visible */, 5 /* bytest */);
    data_use_measurement_.RecordTrafficSizeMetric(
        false /* is_user_traffic */, false /* is_downstream */,
        true /* is_tab_visible */, 5 /* bytest */);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.System.Downstream." +
                                          target_dimension + kConnectionType,
                                      1);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.System.Upstream." +
                                          target_dimension + kConnectionType,
                                      1);
  }

  DataUseMeasurement* data_use_measurement() { return &data_use_measurement_; }

 protected:
  DataUseMeasurement data_use_measurement_;
  const std::string kConnectionType = "NotCellular";

  DISALLOW_COPY_AND_ASSIGN(DataUseMeasurementTest);
};

// This test function tests recording of data use information in UMA histogram
// when packet is originated from user or services when the app is in the
// foreground or the OS is not Android.
// TODO(amohammadkhan): Add tests for Cellular/non-cellular connection types
// when support for testing is provided in its class.
TEST_F(DataUseMeasurementTest, UserNotUserTest) {
#if defined(OS_ANDROID)
  data_use_measurement()->OnApplicationStateChangeForTesting(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
#endif
  TestForAServiceRequest("Foreground.");
  TestForAUserRequest("Foreground.");
}

#if defined(OS_ANDROID)
// This test function tests recording of data use information in UMA histogram
// when packet is originated from user or services when the app is in the
// background and OS is Android.
TEST_F(DataUseMeasurementTest, ApplicationStateTest) {
  data_use_measurement()->OnApplicationStateChangeForTesting(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  TestForAServiceRequest("Background.");
  TestForAUserRequest("Background.");
}
#endif

}  // namespace data_use_measurement
