// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_service.h"

#include "base/time/time_override.h"
#include "components/metrics/dwa/dwa_pref_names.h"
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

TEST_F(DwaServiceTest, ClientIdIsGenerated) {
  TestingPrefServiceSimple pref_service;
  DwaService::RegisterPrefs(pref_service.registry());

  base::Time expected_time_at_daily_resolution;
  EXPECT_TRUE(base::Time::FromString("1 May 2024 00:00:00 GMT",
                                     &expected_time_at_daily_resolution));

  uint64_t client_id;

  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time now;
          EXPECT_TRUE(base::Time::FromString("1 May 2024 12:15 GMT", &now));
          return now;
        },
        nullptr, nullptr);
    client_id = DwaService::GetEphemeralClientId(pref_service);
  }

  EXPECT_GT(pref_service.GetUint64(metrics::dwa::prefs::kDwaClientId), 0u);
  EXPECT_EQ(client_id,
            pref_service.GetUint64(metrics::dwa::prefs::kDwaClientId));
  EXPECT_EQ(expected_time_at_daily_resolution,
            pref_service.GetTime(metrics::dwa::prefs::kDwaClientIdLastUpdated));
}

TEST_F(DwaServiceTest, ClientIdOnlyChangesBetweenDays) {
  TestingPrefServiceSimple pref_service;
  DwaService::RegisterPrefs(pref_service.registry());

  uint64_t client_id_day1;
  uint64_t client_id_day2;
  uint64_t client_id_day2_later;

  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time now;
          EXPECT_TRUE(base::Time::FromString("1 May 2024 12:15 GMT", &now));
          return now;
        },
        nullptr, nullptr);
    client_id_day1 = DwaService::GetEphemeralClientId(pref_service);
  }

  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time now;
          EXPECT_TRUE(base::Time::FromString("2 May 2024 12:15 GMT", &now));
          return now;
        },
        nullptr, nullptr);
    client_id_day2 = DwaService::GetEphemeralClientId(pref_service);
  }

  {
    base::subtle::ScopedTimeClockOverrides time_override(
        []() {
          base::Time now;
          EXPECT_TRUE(base::Time::FromString("2 May 2024 16:15 GMT", &now));
          return now;
        },
        nullptr, nullptr);
    client_id_day2_later = DwaService::GetEphemeralClientId(pref_service);
  }

  EXPECT_NE(client_id_day1, client_id_day2);
  EXPECT_EQ(client_id_day2, client_id_day2_later);
}

}  // namespace metrics::dwa
