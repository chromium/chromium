// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/drive_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

class DriveMetricsProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    DriveMetricsProvider::RegisterPrefs(local_state_.registry());
  }

  TestingPrefServiceSimple local_state_;
};

TEST_F(DriveMetricsProviderTest, HasSeekPenalty_FallbackToLocalState) {
  // Set the pref indicating the app drive has a seek penalty.
  local_state_.SetBoolean(prefs::kMetricsAppDriveHasSeekPenalty, true);

  DriveMetricsProvider provider(0, &local_state_);
  SystemProfileProto::Hardware::Drive drive_proto;

  // Create an empty response, simulating that the async task hasn't finished.
  DriveMetricsProvider::SeekPenaltyResponse response;

  provider.FillDriveMetrics(response, &drive_proto,
                            prefs::kMetricsAppDriveHasSeekPenalty);

  EXPECT_TRUE(drive_proto.has_has_seek_penalty());
  EXPECT_TRUE(drive_proto.has_seek_penalty());
}

TEST_F(DriveMetricsProviderTest, HasSeekPenalty_WritesToLocalState) {
  DriveMetricsProvider provider(0, &local_state_);
  SystemProfileProto::Hardware::Drive drive_proto;

  DriveMetricsProvider::SeekPenaltyResponse response;
  response.has_seek_penalty = false;

  provider.FillDriveMetrics(response, &drive_proto,
                            prefs::kMetricsUserDataDriveHasSeekPenalty);

  EXPECT_TRUE(drive_proto.has_has_seek_penalty());
  EXPECT_FALSE(drive_proto.has_seek_penalty());

  // Verify the pref is updated.
  EXPECT_TRUE(
      local_state_.HasPrefPath(prefs::kMetricsUserDataDriveHasSeekPenalty));
  EXPECT_FALSE(
      local_state_.GetBoolean(prefs::kMetricsUserDataDriveHasSeekPenalty));
}

}  // namespace metrics
