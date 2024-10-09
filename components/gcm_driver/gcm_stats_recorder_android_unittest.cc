// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_stats_recorder_android.h"

#include <stddef.h>

#include "components/gcm_driver/crypto/gcm_decryption_result.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const char kTestAppId[] = "test_app_id";
const char kTestSenderId[] = "test_sender_id";

class GCMStatsRecorderAndroidTest : public ::testing::Test,
                                    public GCMStatsRecorderAndroid::Delegate {
 public:
  GCMStatsRecorderAndroidTest()
      : activity_recorded_calls_(0) {}
  ~GCMStatsRecorderAndroidTest() override = default;

  // GCMStatsRecorderAndroid::Delegate implementation.
  void OnActivityRecorded() override {
    ++activity_recorded_calls_;
  }

  size_t activity_recorded_calls() const { return activity_recorded_calls_; }

 private:
  size_t activity_recorded_calls_;
};

TEST_F(GCMStatsRecorderAndroidTest, RecordsAndCallsDelegate) {
  GCMStatsRecorderAndroid recorder(this /* delegate */);
  recorder.set_is_recording(true);

  ASSERT_TRUE(recorder.is_recording());

  EXPECT_EQ(0u, activity_recorded_calls());

  recorder.RecordRegistrationSent(kTestAppId);
  EXPECT_EQ(1u, activity_recorded_calls());

  recorder.RecordRegistrationResponse(kTestAppId, true /* success */);
  EXPECT_EQ(2u, activity_recorded_calls());

  recorder.RecordUnregistrationSent(kTestAppId);
  EXPECT_EQ(3u, activity_recorded_calls());

  recorder.RecordUnregistrationResponse(kTestAppId, true /* success */);
  EXPECT_EQ(4u, activity_recorded_calls());

  recorder.RecordDataMessageReceived(kTestAppId, kTestSenderId,
                                     42 /* message_byte_size */);
  EXPECT_EQ(5u, activity_recorded_calls());

  recorder.RecordDecryptionFailure(kTestAppId,
                                   GCMDecryptionResult::INVALID_PAYLOAD);
  EXPECT_EQ(6u, activity_recorded_calls());

  RecordedActivities activities;
  recorder.CollectActivities(&activities);

  EXPECT_EQ(4u, activities.registration_activities.size());
  EXPECT_EQ(1u, activities.receiving_activities.size());
  EXPECT_EQ(1u, activities.decryption_failure_activities.size());

  recorder.CollectActivities(&activities);

  EXPECT_EQ(8u, activities.registration_activities.size());
  EXPECT_EQ(2u, activities.receiving_activities.size());
  EXPECT_EQ(2u, activities.decryption_failure_activities.size());

  recorder.Clear();

  RecordedActivities empty_activities;
  recorder.CollectActivities(&empty_activities);

  EXPECT_EQ(0u, empty_activities.registration_activities.size());
  EXPECT_EQ(0u, empty_activities.receiving_activities.size());
  EXPECT_EQ(0u, empty_activities.decryption_failure_activities.size());
}

TEST_F(GCMStatsRecorderAndroidTest, NullDelegate) {
  GCMStatsRecorderAndroid recorder(nullptr /* delegate */);
  recorder.set_is_recording(true);

  ASSERT_TRUE(recorder.is_recording());

  recorder.RecordRegistrationSent(kTestAppId);

  RecordedActivities activities;
  recorder.CollectActivities(&activities);

  EXPECT_EQ(1u, activities.registration_activities.size());
}

TEST_F(GCMStatsRecorderAndroidTest, NotRecording) {
  GCMStatsRecorderAndroid recorder(this /* delegate */);
  ASSERT_FALSE(recorder.is_recording());

  recorder.RecordRegistrationSent(kTestAppId);

  RecordedActivities activities;
  recorder.CollectActivities(&activities);

  EXPECT_EQ(0u, activities.registration_activities.size());
}

}  // namespace

}  // namespace gcm
