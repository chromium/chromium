// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_service.h"

#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::dwa {
class DwaServiceTest : public testing::Test {
 public:
  DwaServiceTest() = default;

  DwaServiceTest(const DwaServiceTest&) = delete;
  DwaServiceTest& operator=(const DwaServiceTest&) = delete;

  ~DwaServiceTest() override = default;

 protected:
  // Check that the values in |coarse_system_info| are filled in and expected
  // ones correspond to the test data.
  void CheckCoarseSystemInformation(
      const ::dwa::CoarseSystemInfo& coarse_system_info) {
    EXPECT_TRUE(coarse_system_info.has_channel());
    // TestMetricsServiceClient::GetChannel() returns CHANNEL_BETA, so we should
    // expect |coarse_system_info| channel to be "NOT_STABLE".
    EXPECT_EQ(coarse_system_info.channel(),
              ::dwa::CoarseSystemInfo::CHANNEL_NOT_STABLE);
    EXPECT_TRUE(coarse_system_info.has_platform());
    EXPECT_TRUE(coarse_system_info.has_geo_designation());
    EXPECT_TRUE(coarse_system_info.has_client_age());
    EXPECT_TRUE(coarse_system_info.has_milestone_prefix_trimmed());
    EXPECT_TRUE(coarse_system_info.has_is_ukm_enabled());
  }

  TestMetricsServiceClient client_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(DwaServiceTest, RecordCoarseSystemInformation) {
  TestingPrefServiceSimple pref_service;
  MetricsStateManager::RegisterPrefs(pref_service.registry());
  ::dwa::CoarseSystemInfo coarse_system_info;
  DwaService::RecordCoarseSystemInformation(client_, pref_service,
                                            &coarse_system_info);
  CheckCoarseSystemInformation(coarse_system_info);
}

}  // namespace metrics::dwa
