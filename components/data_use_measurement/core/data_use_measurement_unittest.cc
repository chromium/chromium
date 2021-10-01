// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_measurement.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/data_use_measurement/core/data_use_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/network_change_notifier.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace data_use_measurement {

class DataUseMeasurementTest {
 public:
  explicit DataUseMeasurementTest(TestingPrefServiceSimple* test_prefs_)
      : data_use_measurement_(
            test_prefs_,
            network::TestNetworkConnectionTracker::GetInstance()) {
    // During the test it is expected to not have cellular connection.
    DCHECK(!net::NetworkChangeNotifier::IsConnectionCellular(
        net::NetworkChangeNotifier::GetConnectionType()));
  }

  DataUseMeasurementTest(const DataUseMeasurementTest&) = delete;
  DataUseMeasurementTest& operator=(const DataUseMeasurementTest&) = delete;

  // This function makes a user request and confirms that its effect is
  // reflected in proper histograms.
  void TestForAUserRequest(const std::string& target_dimension) {
    base::HistogramTester histogram_tester;
    data_use_measurement_.RecordDownstreamUserTrafficSizeMetric(
        true /* is_tab_visible */, 5 /* bytest */);
    data_use_measurement_.RecordDownstreamUserTrafficSizeMetric(
        true /* is_tab_visible */, 5 /* bytest */);
    histogram_tester.ExpectTotalCount("DataUse.TrafficSize.User.Downstream." +
                                          target_dimension + kConnectionType,
                                      2);
  }


  DataUseMeasurement* data_use_measurement() { return &data_use_measurement_; }

 protected:
  // Required to register a NetworkConnectionObserver from the constructor of
  // DataUseMeasurement.
  base::test::TaskEnvironment task_environment_;

  DataUseMeasurement data_use_measurement_;
  const std::string kConnectionType = "NotCellular";
};

// This test function tests recording of data use information in UMA histogram
// when packet is originated from user or services when the app is in the
// foreground or the OS is not Android.
// TODO(amohammadkhan): Add tests for Cellular/non-cellular connection types
// when support for testing is provided in its class.
TEST(DataUseMeasurementTest, UserNotUserTest) {
  TestingPrefServiceSimple test_prefs;

  test_prefs.registry()->RegisterDictionaryPref(prefs::kDataUsedUserForeground);
  test_prefs.registry()->RegisterDictionaryPref(prefs::kDataUsedUserBackground);
  test_prefs.registry()->RegisterDictionaryPref(
      prefs::kDataUsedServicesForeground);
  test_prefs.registry()->RegisterDictionaryPref(
      prefs::kDataUsedServicesBackground);

  DataUseMeasurementTest data_use_measurement_test(&test_prefs);
#if defined(OS_ANDROID)
  data_use_measurement_test.data_use_measurement()
      ->OnApplicationStateChangeForTesting(
          base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
#endif
  data_use_measurement_test.TestForAUserRequest("Foreground.");
}

#if defined(OS_ANDROID)
// This test function tests recording of data use information in UMA histogram
// when packet is originated from user or services when the app is in the
// background and OS is Android.
TEST(DataUseMeasurementTest, ApplicationStateTest) {
  TestingPrefServiceSimple test_prefs;

  test_prefs.registry()->RegisterDictionaryPref(prefs::kDataUsedUserForeground);
  test_prefs.registry()->RegisterDictionaryPref(prefs::kDataUsedUserBackground);
  test_prefs.registry()->RegisterDictionaryPref(
      prefs::kDataUsedServicesForeground);
  test_prefs.registry()->RegisterDictionaryPref(
      prefs::kDataUsedServicesBackground);

  DataUseMeasurementTest data_use_measurement_test(&test_prefs);

  data_use_measurement_test.data_use_measurement()
      ->OnApplicationStateChangeForTesting(
          base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  data_use_measurement_test.TestForAUserRequest("Background.");
}
#endif

}  // namespace data_use_measurement
