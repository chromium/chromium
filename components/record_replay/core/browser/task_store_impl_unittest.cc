// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_store_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/record_replay/core/common/record_replay_switches.h"
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

class TaskStoreImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    task_store_ = std::make_unique<TaskStoreImpl>(temp_dir_.GetPath());
    WaitForDatabaseOperations();
  }

  void TearDown() override {
    task_store_.reset();
    WaitForDatabaseOperations();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  void WaitForDatabaseOperations() {
    task_environment_.RunUntilIdle();
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

  TaskStoreImpl& task_store() { return *task_store_; }

  void ResetTaskStore() {
    task_store_.reset();
    WaitForDatabaseOperations();
  }

  void RecreateTaskStore() {
    task_store_ = std::make_unique<TaskStoreImpl>(temp_dir_.GetPath());
    WaitForDatabaseOperations();
  }

  base::FilePath profile_path() { return temp_dir_.GetPath(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TaskStoreImpl> task_store_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(TaskStoreImplTest, AddAndRetrieveRecording) {
  const Recording recording = CreateLoginRecording();

  task_store().AddRecording(recording, base::BindOnce([](int64_t id) {}));
  WaitForDatabaseOperations();

  base::test::TestFuture<std::vector<Recording>> future;
  task_store().GetRecordingsByUrl(recording.url(), future.GetCallback());
  std::vector<Recording> recordings = future.Get();
  ASSERT_EQ(recordings.size(), 1u);
  recordings[0].clear_id();
  EXPECT_THAT(recordings[0], EqualsProto(recording));
}

// Tests that recordings can be added and retrieved from the database.
TEST_F(TaskStoreImplTest, AddMultipleRecordings) {
  const Recording login_recording = CreateLoginRecording();
  const Recording appointment_recording = CreateAppointmentRecording();

  {
    base::test::TestFuture<std::vector<Recording>> login_future;
    base::test::TestFuture<std::vector<Recording>> appointment_future;
    task_store().GetRecordingsByUrl(login_recording.url(),
                                    login_future.GetCallback());
    task_store().GetRecordingsByUrl(appointment_recording.url(),
                                    appointment_future.GetCallback());
    EXPECT_TRUE(login_future.Get().empty());
    EXPECT_TRUE(appointment_future.Get().empty());
  }

  task_store().AddRecording(appointment_recording,
                            base::BindOnce([](int64_t id) {}));
  task_store().AddRecording(login_recording, base::BindOnce([](int64_t id) {}));
  WaitForDatabaseOperations();

  {
    base::test::TestFuture<std::vector<Recording>> login_future;
    base::test::TestFuture<std::vector<Recording>> appointment_future;
    task_store().GetRecordingsByUrl(login_recording.url(),
                                    login_future.GetCallback());
    task_store().GetRecordingsByUrl(appointment_recording.url(),
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

TEST_F(TaskStoreImplTest, AddMultipleIdenticalRecordingsForSameUrl) {
  const Recording first_recording = CreateLoginRecording();
  const Recording second_recording = CreateLoginRecording();

  task_store().AddRecording(first_recording, base::BindOnce([](int64_t id) {}));
  task_store().AddRecording(first_recording, base::BindOnce([](int64_t id) {}));
  WaitForDatabaseOperations();

  base::test::TestFuture<std::vector<Recording>> future;
  task_store().GetRecordingsByUrl(first_recording.url(), future.GetCallback());
  std::vector<Recording> recordings = future.Get();
  ASSERT_EQ(recordings.size(), 2u);
  // Ensure the newest recording is first since the UI currently uses that.
  recordings[0].clear_id();
  recordings[1].clear_id();
  EXPECT_THAT(recordings[0], EqualsProto(second_recording));
  EXPECT_THAT(recordings[1], EqualsProto(first_recording));
}

TEST_F(TaskStoreImplTest, SaveAndRetrieveTaskDefinition) {
  const Recording recording = CreateLoginRecording();

  base::test::TestFuture<int64_t> id_future;
  task_store().AddRecording(recording, id_future.GetCallback());
  int64_t recording_id = id_future.Get();
  ASSERT_GT(recording_id, 0);

  TaskDefinition task_definition;
  task_definition.set_title("Test Title");
  task_definition.set_description("Test Description");
  task_definition.set_url(recording.url());
  task_definition.set_recording_id(recording_id);
  TaskStep* step = task_definition.add_task_steps();
  step->set_step_index(0);
  step->set_description("Step 1");
  step->set_url(recording.url());

  base::test::TestFuture<int64_t> add_future;
  task_store().SaveTaskDefinition(std::nullopt, std::move(task_definition),
                                  add_future.GetCallback());
  int64_t def_id = add_future.Get();
  EXPECT_GT(def_id, 0);

  base::test::TestFuture<std::vector<TaskDefinition>> get_future;
  task_store().GetTaskDefinitionsByUrl(recording.url(),
                                       get_future.GetCallback());
  auto retrieved = get_future.Get();

  ASSERT_EQ(retrieved.size(), 1u);
  EXPECT_EQ(retrieved[0].id(), def_id);
  EXPECT_EQ(retrieved[0].title(), "Test Title");
  EXPECT_EQ(retrieved[0].description(), "Test Description");
  ASSERT_EQ(retrieved[0].task_steps_size(), 1);
  EXPECT_EQ(retrieved[0].task_steps(0).description(), "Step 1");
}

TEST_F(TaskStoreImplTest, DeleteTaskDefinition) {
  TaskDefinition definition;
  definition.set_title("Simple Booking");
  definition.set_url("https://example.com");

  base::test::TestFuture<int64_t> save_future;
  task_store().SaveTaskDefinition(std::nullopt, std::move(definition),
                                  save_future.GetCallback());
  int64_t def_id = save_future.Get();
  ASSERT_GT(def_id, 0);

  base::test::TestFuture<bool> delete_future;
  task_store().DeleteTaskDefinition(def_id, delete_future.GetCallback());
  EXPECT_TRUE(delete_future.Get());

  base::test::TestFuture<std::optional<TaskDefinition>> get_future;
  task_store().GetTaskDefinition(def_id, get_future.GetCallback());
  EXPECT_FALSE(get_future.Get().has_value());
}

TEST_F(TaskStoreImplTest, SaveAndRetrieveObservations) {
  TaskDefinition definition;
  definition.set_title("Simple Booking");
  definition.set_url("https://example.com");

  TaskStep* step = definition.add_task_steps();
  step->set_step_index(0);
  step->set_description("Step 1");
  step->set_url("https://example.com");

  TaskParameter* param = step->add_parameters();
  param->set_key("k1");
  param->set_name("param1");
  param->set_type("string");

  base::test::TestFuture<int64_t> save_def_future;
  task_store().SaveTaskDefinition(std::nullopt, std::move(definition),
                                  save_def_future.GetCallback());
  int64_t def_id = save_def_future.Get();
  ASSERT_GT(def_id, 0);

  base::test::TestFuture<std::optional<TaskDefinition>> get_def_future;
  task_store().GetTaskDefinition(def_id, get_def_future.GetCallback());
  std::optional<TaskDefinition> retrieved_def = get_def_future.Take();
  ASSERT_TRUE(retrieved_def.has_value());

  TaskObservation obs;
  *obs.mutable_definition() = std::move(*retrieved_def);
  obs.set_start_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  obs.set_end_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  obs.set_execution_source(ExecutionSource::AUTOMATIC);
  obs.mutable_definition()
      ->mutable_task_steps(0)
      ->mutable_parameters(0)
      ->set_value("Window");

  const int64_t expected_start_time = obs.start_time();

  base::test::TestFuture<int64_t> save_obs_future;
  task_store().SaveObservation(std::move(obs), save_obs_future.GetCallback());
  int64_t obs_id = save_obs_future.Get();
  EXPECT_GT(obs_id, 0);

  base::test::TestFuture<std::vector<TaskObservation>> get_obs_future;
  task_store().GetObservationsForDefinition(def_id,
                                            get_obs_future.GetCallback());
  std::vector<TaskObservation> observations = get_obs_future.Take();

  ASSERT_EQ(observations.size(), 1u);
  EXPECT_EQ(observations[0].id(), obs_id);
  EXPECT_EQ(observations[0].start_time(), expected_start_time);
  EXPECT_EQ(observations[0].execution_source(), ExecutionSource::AUTOMATIC);
  EXPECT_EQ(observations[0].definition().task_steps(0).parameters(0).value(),
            "Window");
}

TEST_F(TaskStoreImplTest, SeedFromFileQuickSyntax) {
  ResetTaskStore();

  base::test::ScopedCommandLine scoped_command_line;
  base::FilePath seed_file = profile_path().AppendASCII("seed.json");
  std::string json = R"([
    {
      "url": "https://example.com",
      "title": "Quick Title",
      "instructions": "Quick Instructions",
      "anchored_message": "Quick Message"
    }
  ])";
  ASSERT_TRUE(base::WriteFile(seed_file, json));

  scoped_command_line.GetProcessCommandLine()->AppendSwitchPath(
      switches::kTaskDefinitionFile, seed_file);

  RecreateTaskStore();

  base::test::TestFuture<std::vector<TaskDefinition>> future;
  task_store().GetTaskDefinitionsByUrl("https://example.com",
                                       future.GetCallback());
  auto task_definitions = future.Get();

  ASSERT_EQ(task_definitions.size(), 1u);
  EXPECT_EQ(task_definitions[0].title(), "Quick Title");
  EXPECT_EQ(task_definitions[0].description(), "Quick Instructions");
  ASSERT_EQ(task_definitions[0].task_steps_size(), 1);
  EXPECT_EQ(task_definitions[0].task_steps(0).description(),
            "Quick Instructions");
}

TEST_F(TaskStoreImplTest, SeedFromFileDetailedSyntax) {
  ResetTaskStore();

  base::test::ScopedCommandLine scoped_command_line;
  base::FilePath seed_file = profile_path().AppendASCII("seed_detailed.json");
  std::string json = R"([
    {
      "url": "https://example.com/detailed",
      "title": "Detailed Title",
      "instructions": "Top Level Description",
      "steps": [
        {
          "description": "Step 1 Desc",
          "expected_data_keys": ["key1"]
        },
        {
          "description": "Step 2 Desc",
          "expected_data_keys": ["key2"]
        }
      ],
      "anchored_message": "Anchored Message"
    }
  ])";
  ASSERT_TRUE(base::WriteFile(seed_file, json));

  scoped_command_line.GetProcessCommandLine()->AppendSwitchPath(
      switches::kTaskDefinitionFile, seed_file);

  RecreateTaskStore();

  base::test::TestFuture<std::vector<TaskDefinition>> future;
  task_store().GetTaskDefinitionsByUrl("https://example.com/detailed",
                                       future.GetCallback());
  auto task_definitions = future.Get();

  ASSERT_EQ(task_definitions.size(), 1u);
  EXPECT_EQ(task_definitions[0].title(), "Detailed Title");
  EXPECT_EQ(task_definitions[0].description(), "Top Level Description");
  ASSERT_EQ(task_definitions[0].task_steps_size(), 2);
  EXPECT_EQ(task_definitions[0].task_steps(0).description(), "Step 1 Desc");
  EXPECT_EQ(task_definitions[0].task_steps(0).parameters(0).key(), "key1");
  EXPECT_EQ(task_definitions[0].task_steps(1).description(), "Step 2 Desc");
  EXPECT_EQ(task_definitions[0].task_steps(1).parameters(0).key(), "key2");
}

}  // namespace

}  // namespace record_replay
