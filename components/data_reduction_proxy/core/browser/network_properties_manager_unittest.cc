// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"

#include <map>
#include "base/run_loop.h"

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

class TestNetworkPropertiesManager : public NetworkPropertiesManager {
 public:
  TestNetworkPropertiesManager(PrefService* pref_service)
      : TestNetworkPropertiesManager(base::DefaultClock::GetInstance(),
                                     pref_service) {}
  TestNetworkPropertiesManager(base::Clock* clock, PrefService* pref_service)
      : NetworkPropertiesManager(clock, pref_service) {}
  ~TestNetworkPropertiesManager() override {}
};

TEST(NetworkPropertyTest, TestSetterGetterCaptivePortal) {
  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  TestNetworkPropertiesManager network_properties_manager(&test_prefs);

  std::string network_id("test");
  network_properties_manager.OnChangeInNetworkID(network_id);

  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));

  network_properties_manager.SetIsCaptivePortal(true);
  EXPECT_FALSE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  base::RunLoop().RunUntilIdle();

  // Verify the prefs.
  EXPECT_EQ(1u, test_prefs.GetDictionary(prefs::kNetworkProperties)->size());
  EXPECT_FALSE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                   ->HasKey("mismatched_network_id"));
  EXPECT_TRUE(
      test_prefs.GetDictionary(prefs::kNetworkProperties)->HasKey(network_id));
  {
    TestNetworkPropertiesManager network_properties_manager_2(&test_prefs);
    network_properties_manager_2.OnChangeInNetworkID(network_id);
    EXPECT_TRUE(network_properties_manager_2.IsCaptivePortal());
  }

  network_properties_manager.SetIsCaptivePortal(false);
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  base::RunLoop().RunUntilIdle();

  {
    TestNetworkPropertiesManager network_properties_manager_2(&test_prefs);
    network_properties_manager_2.OnChangeInNetworkID(network_id);
    EXPECT_FALSE(network_properties_manager_2.IsCaptivePortal());
  }

  // Verify the prefs.
  EXPECT_EQ(1u, test_prefs.GetDictionary(prefs::kNetworkProperties)->size());
  EXPECT_FALSE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                   ->HasKey("mismatched_network_id"));
  EXPECT_TRUE(
      test_prefs.GetDictionary(prefs::kNetworkProperties)->HasKey(network_id));
}

TEST(NetworkPropertyTest, TestSetterGetterDisallowedByCarrier) {
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  TestNetworkPropertiesManager network_properties_manager(&test_prefs);

  network_properties_manager.SetIsSecureProxyDisallowedByCarrier(true);
  EXPECT_FALSE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());

  network_properties_manager.SetIsSecureProxyDisallowedByCarrier(false);
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
}

TEST(NetworkPropertyTest, TestWarmupURLFailedOnSecureCoreProxy) {
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  TestNetworkPropertiesManager network_properties_manager(&test_prefs);

  network_properties_manager.SetHasWarmupURLProbeFailed(
      true /* secure_proxy */, true /* is_core_proxy */,
      true /* warmup_url_probe_failed */);
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(false, true));
  EXPECT_TRUE(network_properties_manager.HasWarmupURLProbeFailed(true, true));

  network_properties_manager.SetHasWarmupURLProbeFailed(true, true, false);
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(false, true));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, true));
}

TEST(NetworkPropertyTest, TestWarmupURLFailedOnInSecureCoreProxy) {
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  TestNetworkPropertiesManager network_properties_manager(&test_prefs);

  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));

  network_properties_manager.SetHasWarmupURLProbeFailed(
      false /* secure_proxy */, true /* is_core_proxy */,
      true /* warmup_url_probe_failed */);
  EXPECT_FALSE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.HasWarmupURLProbeFailed(false, true));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, true));

  network_properties_manager.SetHasWarmupURLProbeFailed(false, true, false);
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(false, true));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, true));
}

TEST(NetworkPropertyTest, TestLimitPrefSize) {
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  TestNetworkPropertiesManager network_properties_manager(&test_prefs);

  size_t num_network_ids = 100;

  for (size_t i = 0; i < num_network_ids; ++i) {
    std::string network_id("test" + base::NumberToString(i));
    network_properties_manager.OnChangeInNetworkID(network_id);

    // State should be reset when there is a change in the network ID.
    EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
    EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));

    network_properties_manager.SetIsCaptivePortal(true);
    EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
    EXPECT_FALSE(network_properties_manager.IsSecureProxyAllowed(true));
    EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
    EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
    EXPECT_FALSE(
        network_properties_manager.HasWarmupURLProbeFailed(false, true));
    EXPECT_FALSE(
        network_properties_manager.HasWarmupURLProbeFailed(true, true));

    base::RunLoop().RunUntilIdle();
  }

  // Pref size should be bounded to 10.
  EXPECT_EQ(10u, test_prefs.GetDictionary(prefs::kNetworkProperties)->size());
  EXPECT_FALSE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                   ->HasKey("mismatched_network_id"));

  // The last 10 network IDs are guaranteed to be present in the prefs.
  for (size_t i = num_network_ids - 10; i < num_network_ids; ++i) {
    EXPECT_TRUE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                    ->HasKey("test" + base::NumberToString(i)));
  }

  {
    TestNetworkPropertiesManager network_properties_manager_2(&test_prefs);
    for (size_t i = 0; i < num_network_ids; ++i) {
      std::string network_id("test" + base::NumberToString(i));
      network_properties_manager_2.OnChangeInNetworkID(network_id);

      EXPECT_EQ(test_prefs.GetDictionary(prefs::kNetworkProperties)
                    ->HasKey(network_id),
                network_properties_manager_2.IsCaptivePortal());
      EXPECT_FALSE(
          network_properties_manager_2.IsSecureProxyDisallowedByCarrier());
      EXPECT_FALSE(
          network_properties_manager_2.HasWarmupURLProbeFailed(false, true));
      EXPECT_FALSE(
          network_properties_manager_2.HasWarmupURLProbeFailed(true, true));
    }
  }
}

TEST(NetworkPropertyTest, TestChangeNetworkIDBackAndForth) {
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  TestNetworkPropertiesManager network_properties_manager(&test_prefs);

  // First network ID has a captive portal.
  std::string first_network_id("test1");
  network_properties_manager.OnChangeInNetworkID(first_network_id);
  // State should be reset when there is a change in the network ID.
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  network_properties_manager.SetIsCaptivePortal(true);
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  base::RunLoop().RunUntilIdle();

  // Warmup probe failed on the insecure proxy for the second network ID.
  std::string second_network_id("test2");
  network_properties_manager.OnChangeInNetworkID(second_network_id);
  // State should be reset when there is a change in the network ID.
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  network_properties_manager.SetHasWarmupURLProbeFailed(false, true, true);
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.IsInsecureProxyAllowed(true));
  base::RunLoop().RunUntilIdle();

  // Change back to |first_network_id|.
  network_properties_manager.OnChangeInNetworkID(first_network_id);
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));

  // Change back to |second_network_id|.
  network_properties_manager.OnChangeInNetworkID(second_network_id);
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.IsInsecureProxyAllowed(true));

  // Verify the prefs.
  EXPECT_EQ(2u, test_prefs.GetDictionary(prefs::kNetworkProperties)->size());
  EXPECT_FALSE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                   ->HasKey("mismatched_network_id"));
  EXPECT_TRUE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                  ->HasKey(first_network_id));
  EXPECT_TRUE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                  ->HasKey(second_network_id));
}

TEST(NetworkPropertyTest, TestNetworkQualitiesOverwrite) {
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  TestNetworkPropertiesManager network_properties_manager(&test_prefs);

  // First network ID has a captive portal.
  std::string first_network_id("test1");
  network_properties_manager.OnChangeInNetworkID(first_network_id);
  // State should be reset when there is a change in the network ID.
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  network_properties_manager.SetIsCaptivePortal(true);
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  base::RunLoop().RunUntilIdle();

  // Warmup probe failed on the insecure proxy for the second network ID.
  std::string second_network_id("test2");
  network_properties_manager.OnChangeInNetworkID(second_network_id);
  // State should be reset when there is a change in the network ID.
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  network_properties_manager.SetHasWarmupURLProbeFailed(false, true, true);
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.IsInsecureProxyAllowed(true));
  base::RunLoop().RunUntilIdle();

  // Change back to |first_network_id|.
  network_properties_manager.OnChangeInNetworkID(first_network_id);
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  network_properties_manager.SetHasWarmupURLProbeFailed(false, true, true);

  // Change to |first_network_id|.
  network_properties_manager.OnChangeInNetworkID(first_network_id);
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.IsInsecureProxyAllowed(true));

  // Verify the prefs.
  EXPECT_EQ(2u, test_prefs.GetDictionary(prefs::kNetworkProperties)->size());
  EXPECT_FALSE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                   ->HasKey("mismatched_network_id"));
  EXPECT_TRUE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                  ->HasKey(first_network_id));
  EXPECT_TRUE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                  ->HasKey(second_network_id));
}

TEST(NetworkPropertyTest, TestDeleteHistory) {
  base::HistogramTester histogram_tester;
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  TestNetworkPropertiesManager network_properties_manager(&test_prefs);

  std::string network_id("test");
  network_properties_manager.OnChangeInNetworkID(network_id);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.NetworkProperties.CacheHit", false, 1);

  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_TRUE(network_properties_manager.IsSecureProxyAllowed(true));

  network_properties_manager.SetIsCaptivePortal(true);
  EXPECT_TRUE(network_properties_manager.IsInsecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyAllowed(true));
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(false, true));
  EXPECT_FALSE(network_properties_manager.HasWarmupURLProbeFailed(true, true));
  base::RunLoop().RunUntilIdle();

  // Verify the prefs.
  EXPECT_EQ(1u, test_prefs.GetDictionary(prefs::kNetworkProperties)->size());
  EXPECT_FALSE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                   ->HasKey("mismatched_network_id"));
  EXPECT_TRUE(
      test_prefs.GetDictionary(prefs::kNetworkProperties)->HasKey(network_id));
  {
    TestNetworkPropertiesManager network_properties_manager_2(&test_prefs);
    network_properties_manager_2.OnChangeInNetworkID(network_id);
    EXPECT_TRUE(network_properties_manager_2.IsCaptivePortal());
    histogram_tester.ExpectBucketCount(
        "DataReductionProxy.NetworkProperties.CacheHit", true, 1);
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.NetworkProperties.CacheHit", 2);
  }

  // Pref should be cleared.
  network_properties_manager.DeleteHistory();
  base::RunLoop().RunUntilIdle();
  {
    TestNetworkPropertiesManager network_properties_manager_2(&test_prefs);
    network_properties_manager_2.OnChangeInNetworkID(network_id);
    EXPECT_FALSE(network_properties_manager_2.IsCaptivePortal());
    histogram_tester.ExpectBucketCount(
        "DataReductionProxy.NetworkProperties.CacheHit", false, 2);
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.NetworkProperties.CacheHit", 3);
  }
}

TEST(NetworkPropertyTest, TestDeleteOldValues) {
  base::HistogramTester histogram_tester;
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::DefaultClock::GetInstance()->Now());

  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  TestNetworkPropertiesManager network_properties_manager(&test_clock,
                                                          &test_prefs);

  for (size_t i = 0; i < 5; ++i) {
    std::string network_id("test" + base::NumberToString(i));
    network_properties_manager.OnChangeInNetworkID(network_id);
    network_properties_manager.SetIsCaptivePortal(true);
  }

  test_clock.Advance(base::TimeDelta::FromDays(20));

  for (size_t i = 5; i < 10; ++i) {
    std::string network_id("test" + base::NumberToString(i));
    network_properties_manager.OnChangeInNetworkID(network_id);
    network_properties_manager.SetIsCaptivePortal(true);
  }

  base::RunLoop().RunUntilIdle();

  // Verify the prefs.
  EXPECT_EQ(10u, test_prefs.GetDictionary(prefs::kNetworkProperties)->size());
  for (size_t i = 0; i < 10; ++i) {
    std::string network_id("test" + base::NumberToString(i));
    EXPECT_TRUE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                    ->HasKey(network_id));
  }

  // Entries should not be cleared since all values are less than 30 days old.
  {
    TestNetworkPropertiesManager network_properties_manager_2(&test_clock,
                                                              &test_prefs);
    for (size_t i = 0; i < 10; ++i) {
      std::string network_id("test" + base::NumberToString(i));

      EXPECT_TRUE(test_prefs.GetDictionary(prefs::kNetworkProperties)
                      ->HasKey(network_id));
    }
  }

  // Only the entries from 5 to 9 should be cleared since they are 40 days old.
  test_clock.Advance(base::TimeDelta::FromDays(20));
  {
    TestNetworkPropertiesManager network_properties_manager_3(&test_clock,
                                                              &test_prefs);
    for (size_t i = 0; i < 10; ++i) {
      std::string network_id("test" + base::NumberToString(i));
      EXPECT_EQ(i >= 5, test_prefs.GetDictionary(prefs::kNetworkProperties)
                            ->HasKey(network_id));
    }
  }
}

TEST(NetworkPropertyTest,
     TestSetterGetterDisallowedByCarrierDiscardingEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;

  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  TestNetworkPropertiesManager network_properties_manager(&test_prefs);

  // First network ID has a captive portal and the canary check failed.
  std::string first_network_id("test1");
  network_properties_manager.OnChangeInNetworkID(first_network_id);
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  network_properties_manager.SetIsSecureProxyDisallowedByCarrier(true);
  network_properties_manager.SetIsCaptivePortal(true);
  EXPECT_TRUE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
  base::RunLoop().RunUntilIdle();

  // Change to a different network. State should be reset when there is a
  // change in the network ID.
  std::string second_network_id("test2");
  network_properties_manager.OnChangeInNetworkID(second_network_id);
  EXPECT_FALSE(network_properties_manager.IsCaptivePortal());
  EXPECT_FALSE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  base::RunLoop().RunUntilIdle();

  // Change back to |first_network_id|. Captive portal state should be
  // persisted but the canary check state should not be.
  network_properties_manager.OnChangeInNetworkID(first_network_id);
  EXPECT_TRUE(network_properties_manager.IsSecureProxyDisallowedByCarrier());
  EXPECT_TRUE(network_properties_manager.IsCaptivePortal());
}

}  // namespace

}  // namespace data_reduction_proxy
