// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/recording_data_manager_impl.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace record_replay {

namespace {

Recording CreateLoginRecording() {
  Recording r;
  r.set_url("https://foo.com");
  r.set_name("Login Recording");
  r.set_screenshot("fake_screenshot_data_for_login");
  r.set_start_time(1337);
  Recording::Action* action = r.add_actions();
  action->set_delta(8);
  action->set_element_selector("#username");
  action->mutable_click_specifics();
  action = r.add_actions();
  action->set_delta(14);
  action->set_element_selector("#password");
  action->mutable_click_specifics();
  return r;
}

Recording CreateAppointmentRecording() {
  Recording r;
  r.set_url("https://bar.com");
  r.set_name("Appointment Recording");
  r.set_screenshot("fake_screenshot_data_for_appointment");
  r.set_start_time(8008);
  Recording::Action* action = r.add_actions();
  action->set_delta(11);
  action->set_element_selector("#book");
  action->mutable_click_specifics();
  return r;
}

using ::base::test::EqualsProto;

class RecordingDataManagerImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    data_manager_ =
        std::make_unique<RecordingDataManagerImpl>(temp_dir_.GetPath());
    WaitForDatabaseOperations();
  }

  void TearDown() override {
    data_manager_.reset();
    WaitForDatabaseOperations();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  void WaitForDatabaseOperations() { task_environment_.RunUntilIdle(); }

  RecordingDataManagerImpl& data_manager() { return *data_manager_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<RecordingDataManagerImpl> data_manager_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(RecordingDataManagerImplTest, AddAndRetrieveRecording) {
  const Recording recording = CreateLoginRecording();

  data_manager().AddRecording(recording, base::BindOnce([](int64_t id) {}));
  WaitForDatabaseOperations();

  base::test::TestFuture<std::vector<Recording>> future;
  data_manager().GetRecordingsByUrl(recording.url(), future.GetCallback());
  std::vector<Recording> recordings = future.Get();
  ASSERT_EQ(recordings.size(), 1u);
  recordings[0].clear_id();
  EXPECT_THAT(recordings[0], EqualsProto(recording));
}

// Tests that recordings can be added and retrieved from the database.
TEST_F(RecordingDataManagerImplTest, AddMultipleRecordings) {
  const Recording login_recording = CreateLoginRecording();
  const Recording appointment_recording = CreateAppointmentRecording();

  {
    base::test::TestFuture<std::vector<Recording>> login_future;
    base::test::TestFuture<std::vector<Recording>> appointment_future;
    data_manager().GetRecordingsByUrl(login_recording.url(),
                                      login_future.GetCallback());
    data_manager().GetRecordingsByUrl(appointment_recording.url(),
                                      appointment_future.GetCallback());
    EXPECT_TRUE(login_future.Get().empty());
    EXPECT_TRUE(appointment_future.Get().empty());
  }

  data_manager().AddRecording(appointment_recording,
                              base::BindOnce([](int64_t id) {}));
  data_manager().AddRecording(login_recording,
                              base::BindOnce([](int64_t id) {}));
  WaitForDatabaseOperations();

  {
    base::test::TestFuture<std::vector<Recording>> login_future;
    base::test::TestFuture<std::vector<Recording>> appointment_future;
    data_manager().GetRecordingsByUrl(login_recording.url(),
                                      login_future.GetCallback());
    data_manager().GetRecordingsByUrl(appointment_recording.url(),
                                      appointment_future.GetCallback());
    std::vector<Recording> login_recordings = login_future.Get();
    std::vector<Recording> appointment_recordings = appointment_future.Get();
    ASSERT_EQ(login_recordings.size(), 1u);
    ASSERT_EQ(appointment_recordings.size(), 1u);
    login_recordings[0].clear_id();
    appointment_recordings[0].clear_id();
    EXPECT_THAT(login_recordings[0], EqualsProto(login_recording));
    EXPECT_THAT(appointment_recordings[0], EqualsProto(appointment_recording));
  }
}

TEST_F(RecordingDataManagerImplTest, AddMultipleIdenticalRecordingsForSameUrl) {
  const Recording first_recording = CreateLoginRecording();
  const Recording second_recording = CreateLoginRecording();

  data_manager().AddRecording(first_recording,
                              base::BindOnce([](int64_t id) {}));
  data_manager().AddRecording(first_recording,
                              base::BindOnce([](int64_t id) {}));
  WaitForDatabaseOperations();

  base::test::TestFuture<std::vector<Recording>> future;
  data_manager().GetRecordingsByUrl(first_recording.url(),
                                    future.GetCallback());
  std::vector<Recording> recordings = future.Get();
  ASSERT_EQ(recordings.size(), 2u);
  // Ensure the newest recording is first since the UI currently uses that.
  recordings[0].clear_id();
  recordings[1].clear_id();
  EXPECT_THAT(recordings[0], EqualsProto(second_recording));
  EXPECT_THAT(recordings[1], EqualsProto(first_recording));
}

TEST_F(RecordingDataManagerImplTest, SaveAndRetrieveActivityAnnotation) {
  const Recording recording = CreateLoginRecording();

  base::test::TestFuture<int64_t> id_future;
  data_manager().AddRecording(recording, id_future.GetCallback());
  int64_t recording_id = id_future.Get();
  ASSERT_GT(recording_id, 0);

  ActivityAnnotation annotation;
  annotation.set_title("Test Title");
  annotation.set_description("Test Description");
  StepAnnotation step;
  step.set_description("Step 1");
  (*annotation.mutable_steps())[1] = step;

  base::test::TestFuture<void> add_future;
  data_manager().SaveActivityAnnotation(std::nullopt, annotation,
                                        recording.url(), recording_id,
                                        add_future.GetCallback());
  add_future.Get();

  base::test::TestFuture<std::vector<std::pair<int64_t, ActivityAnnotation>>>
      get_future;
  data_manager().GetActivityAnnotationsByUrl(recording.url(),
                                             get_future.GetCallback());
  auto retrieved = get_future.Get();

  ASSERT_EQ(retrieved.size(), 1u);
  EXPECT_EQ(retrieved[0].second.title(), "Test Title");
  EXPECT_EQ(retrieved[0].second.description(), "Test Description");
  EXPECT_EQ(retrieved[0].second.steps().at(1).description(), "Step 1");
}

}  // namespace

}  // namespace record_replay
