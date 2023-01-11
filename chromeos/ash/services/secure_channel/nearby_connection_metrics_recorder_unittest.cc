// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/nearby_connection_metrics_recorder.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

const char kTestRemoteDeviceId[] = "testRemoteDeviceId";
const char kTestLocalDeviceId[] = "testLocalDeviceId";

class SecureChannelNearbyConnectionMetricsRecorderTest : public testing::Test {
 protected:
  SecureChannelNearbyConnectionMetricsRecorderTest()
      : device_id_pair_(kTestRemoteDeviceId, kTestLocalDeviceId) {}
  SecureChannelNearbyConnectionMetricsRecorderTest(
      const SecureChannelNearbyConnectionMetricsRecorderTest&) = delete;
  SecureChannelNearbyConnectionMetricsRecorderTest& operator=(
      const SecureChannelNearbyConnectionMetricsRecorderTest&) = delete;
  ~SecureChannelNearbyConnectionMetricsRecorderTest() override = default;

  const DeviceIdPair device_id_pair_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;

  NearbyConnectionMetricsRecorder recorder_;
};

TEST_F(SecureChannelNearbyConnectionMetricsRecorderTest, Test) {
  // Succeed; metric should be logged.
  recorder_.HandleConnectionSuccess(device_id_pair_);
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.EffectiveConnectionResult",
      /*sample=*/true,
      /*count=*/1);

  task_environment_.FastForwardBy(base::Seconds(10));

  // Succeed again; should be logged.
  recorder_.HandleConnectionSuccess(device_id_pair_);
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.EffectiveConnectionResult",
      /*sample=*/true,
      /*count=*/2);

  task_environment_.FastForwardBy(base::Seconds(10));

  // Fail; nothing should be logged since the failure just occurred.
  recorder_.HandleConnectionFailure(device_id_pair_);
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.EffectiveConnectionResult",
      /*sample=*/false,
      /*count=*/0);

  // Fast forward 59 seconds (under 1min).
  task_environment_.FastForwardBy(base::Seconds(59));

  // Fail; still nothing should have been logged.
  recorder_.HandleConnectionFailure(device_id_pair_);
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.EffectiveConnectionResult",
      /*sample=*/false,
      /*count=*/0);

  // Fast forward 1 more second; a minute has passed, so a failure should have
  // been logged.
  task_environment_.FastForwardBy(base::Seconds(1));
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.EffectiveConnectionResult",
      /*sample=*/false,
      /*count=*/1);

  task_environment_.FastForwardBy(base::Seconds(10));

  // Succeed; this should reset any ongoing timer.
  recorder_.HandleConnectionSuccess(device_id_pair_);
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.EffectiveConnectionResult",
      /*sample=*/true,
      /*count=*/3);

  task_environment_.FastForwardBy(base::Seconds(10));

  // Fail; nothing should be logged.
  recorder_.HandleConnectionFailure(device_id_pair_);
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.EffectiveConnectionResult",
      /*sample=*/false,
      /*count=*/1);

  // Move forward another minute and verify that another failure was logged.
  task_environment_.FastForwardBy(base::Minutes(1));
  histogram_tester_.ExpectBucketCount(
      "MultiDevice.SecureChannel.Nearby.EffectiveConnectionResult",
      /*sample=*/false,
      /*count=*/2);
}

}  // namespace ash::secure_channel
