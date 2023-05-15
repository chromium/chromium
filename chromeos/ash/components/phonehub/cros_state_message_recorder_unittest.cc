// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/cros_state_message_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::phonehub {
namespace {
constexpr auto kLatencyDelta = base::Milliseconds(500u);
}  // namespace

class CrosStateMessageRecorderTest : public testing::Test {
 protected:
  CrosStateMessageRecorderTest() = default;
  CrosStateMessageRecorderTest(const CrosStateMessageRecorderTest&) = delete;
  CrosStateMessageRecorderTest& operator=(const CrosStateMessageRecorderTest&) =
      delete;
  ~CrosStateMessageRecorderTest() override = default;

  void SetUp() override {
    feature_status_provider_ = std::make_unique<FakeFeatureStatusProvider>();
    recorder_ = std::make_unique<CrosStateMessageRecorder>(
        feature_status_provider_.get());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<FakeFeatureStatusProvider> feature_status_provider_;
  std::unique_ptr<CrosStateMessageRecorder> recorder_;
  base::HistogramTester histogram_tester_;
};

TEST_F(CrosStateMessageRecorderTest, RecordLatency) {
  feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  recorder_->RecordCrosStateMessageSent();
  task_environment_.FastForwardBy(kLatencyDelta);
  recorder_->RecordPhoneStatusSnapShotReceived();
  histogram_tester_.ExpectTimeBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Latency", kLatencyDelta,
      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Received", true, 1);
  EXPECT_TRUE(recorder_->message_sent_timestamp_.is_null());

  feature_status_provider_->SetStatus(FeatureStatus::kEnabledButDisconnected);
  recorder_->RecordCrosStateMessageSent();
  EXPECT_FALSE(recorder_->is_cros_state_message_sent_);

  recorder_->RecordPhoneStatusSnapShotReceived();
  EXPECT_FALSE(recorder_->is_phone_status_snapshot_processed_);
}

TEST_F(CrosStateMessageRecorderTest, RecordSuccess) {
  feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  recorder_->RecordCrosStateMessageSent();

  feature_status_provider_->SetStatus(FeatureStatus::kUnavailableBluetoothOff);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Received", true, 0);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Received", false, 1);
  EXPECT_FALSE(recorder_->is_cros_state_message_sent_);
  EXPECT_FALSE(recorder_->is_phone_status_snapshot_processed_);

  feature_status_provider_->SetStatus(FeatureStatus::kEnabledButDisconnected);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Received", true, 0);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Received", false, 1);

  feature_status_provider_->SetStatus(FeatureStatus::kEnabledAndConnected);
  recorder_->RecordCrosStateMessageSent();
  recorder_->RecordPhoneStatusSnapShotReceived();
  feature_status_provider_->SetStatus(FeatureStatus::kEnabledButDisconnected);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Received", true, 1);
  histogram_tester_.ExpectBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Received", false, 1);
}

}  // namespace ash::phonehub
