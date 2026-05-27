// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_database.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace record_replay {

class TaskDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_ = std::make_unique<TaskDatabase>();
    db_->Init(temp_dir_.GetPath());
  }

  void TearDown() override { db_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TaskDatabase> db_;
};

TEST_F(TaskDatabaseTest, AddAndGetRecording) {
  Recording recording;
  recording.set_url("https://example.com");
  recording.set_name("Test Recording");
  recording.set_start_time(12345);

  int64_t id = db_->AddRecording(recording);
  EXPECT_GT(id, 0);

  std::vector<Recording> recordings =
      db_->GetRecordingsByUrl("https://example.com");
  ASSERT_EQ(recordings.size(), 1u);
  EXPECT_EQ(recordings[0].id(), id);
  EXPECT_EQ(recordings[0].url(), "https://example.com");
  EXPECT_EQ(recordings[0].name(), "Test Recording");
  EXPECT_EQ(recordings[0].start_time(), 12345);

  db_.reset();

  {
    sql::Database raw_db(sql::Database::Tag("ReplayTasks"));
    ASSERT_TRUE(raw_db.Open(temp_dir_.GetPath().Append(
        FILE_PATH_LITERAL("ReplayTaskDatabase.db"))));
    sql::Statement statement(
        raw_db.GetUniqueStatement("SELECT proto FROM recordings WHERE id=?"));
    statement.BindInt64(0, id);
    ASSERT_TRUE(statement.Step());

    Recording disk_recording;
    ASSERT_TRUE(
        disk_recording.ParseFromString(statement.ColumnBlobAsString(0)));
    EXPECT_FALSE(disk_recording.has_id());
  }
}

TEST_F(TaskDatabaseTest, SaveAndRetrieveTaskDefinition) {
  TaskDefinition definition;
  definition.set_title("Train Booking");
  definition.set_url("https://www.amtrak.com");
  definition.set_description("Templatized input for train booking");

  // Step 0
  TaskStep* step0 = definition.add_task_steps();
  step0->set_step_index(0);
  step0->set_description("Enter origin");
  step0->set_url("https://www.amtrak.com");

  TaskParameter* param0 = step0->add_parameters();
  param0->set_key("origin");
  param0->set_name("Origin Station");
  param0->set_type("string");
  param0->set_description("Where the journey starts");

  // Step 1
  TaskStep* step1 = definition.add_task_steps();
  step1->set_step_index(1);
  step1->set_description("Enter destination");
  step1->set_url("https://www.amtrak.com/destination");

  TaskParameter* param1 = step1->add_parameters();
  param1->set_key("destination");
  param1->set_name("Destination Station");
  param1->set_type("string");
  param1->set_description("Where the journey ends");

  int64_t def_id = db_->SaveTaskDefinition(std::nullopt, definition);
  EXPECT_GT(def_id, 0);

  std::optional<TaskDefinition> retrieved = db_->GetTaskDefinition(def_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->id(), def_id);
  EXPECT_EQ(retrieved->title(), "Train Booking");
  EXPECT_EQ(retrieved->url(), "https://www.amtrak.com/");
  EXPECT_EQ(retrieved->description(), "Templatized input for train booking");

  ASSERT_EQ(retrieved->task_steps_size(), 2);

  // Verify Step 0
  EXPECT_GT(retrieved->task_steps(0).id(), 0);
  EXPECT_EQ(retrieved->task_steps(0).step_index(), 0);
  EXPECT_EQ(retrieved->task_steps(0).description(), "Enter origin");
  EXPECT_EQ(retrieved->task_steps(0).url(), "https://www.amtrak.com/");
  ASSERT_EQ(retrieved->task_steps(0).parameters_size(), 1);
  EXPECT_GT(retrieved->task_steps(0).parameters(0).id(), 0);
  EXPECT_EQ(retrieved->task_steps(0).parameters(0).key(), "origin");
  EXPECT_EQ(retrieved->task_steps(0).parameters(0).name(), "Origin Station");
  EXPECT_EQ(retrieved->task_steps(0).parameters(0).type(), "string");

  // Verify Step 1
  EXPECT_GT(retrieved->task_steps(1).id(), 0);
  EXPECT_EQ(retrieved->task_steps(1).step_index(), 1);
  EXPECT_EQ(retrieved->task_steps(1).description(), "Enter destination");
  EXPECT_EQ(retrieved->task_steps(1).url(),
            "https://www.amtrak.com/destination");
  ASSERT_EQ(retrieved->task_steps(1).parameters_size(), 1);
  EXPECT_GT(retrieved->task_steps(1).parameters(0).id(), 0);
  EXPECT_EQ(retrieved->task_steps(1).parameters(0).key(), "destination");
  EXPECT_EQ(retrieved->task_steps(1).parameters(0).name(),
            "Destination Station");
  EXPECT_EQ(retrieved->task_steps(1).parameters(0).type(), "string");

  // Lookup by URL
  std::vector<TaskDefinition> by_url =
      db_->GetTaskDefinitionsByUrl("https://www.amtrak.com");
  ASSERT_EQ(by_url.size(), 1u);
  EXPECT_EQ(by_url[0].id(), def_id);
}

TEST_F(TaskDatabaseTest, GetTaskDefinitionForNonExistentId) {
  EXPECT_FALSE(db_->GetTaskDefinition(99999).has_value());
}

TEST_F(TaskDatabaseTest, DeleteTaskDefinitionCascades) {
  TaskDefinition definition;
  definition.set_title("Amtrak");
  definition.set_url("https://www.amtrak.com");

  TaskStep* step = definition.add_task_steps();
  step->set_step_index(0);
  step->set_description("Step 1");
  step->set_url("https://www.amtrak.com");

  TaskParameter* param = step->add_parameters();
  param->set_key("p1");
  param->set_name("param 1");
  param->set_type("string");

  int64_t def_id = db_->SaveTaskDefinition(std::nullopt, definition);
  EXPECT_GT(def_id, 0);

  std::optional<TaskDefinition> saved = db_->GetTaskDefinition(def_id);
  ASSERT_TRUE(saved.has_value());

  // Create an observation.
  TaskObservation obs;
  *obs.mutable_definition() = std::move(*saved);
  obs.set_start_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  obs.set_end_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  obs.set_execution_source(ExecutionSource::MANUAL);
  obs.mutable_definition()
      ->mutable_task_steps(0)
      ->mutable_parameters(0)
      ->set_value("Chicago");

  int64_t obs_id = db_->SaveObservation(obs);
  EXPECT_GT(obs_id, 0);

  // Delete definition.
  EXPECT_TRUE(db_->DeleteTaskDefinition(def_id));

  // Verify definition is gone.
  EXPECT_FALSE(db_->GetTaskDefinition(def_id).has_value());

  // Close the main connection to release SQLite locks and file locks on
  // disk.
  db_.reset();

  // Open the database raw to verify cascade deletes cleaned up child rows on
  // disk.
  {
    sql::Database raw_db(sql::Database::Tag("ReplayTasks"));
    ASSERT_TRUE(raw_db.Open(temp_dir_.GetPath().Append(
        FILE_PATH_LITERAL("ReplayTaskDatabase.db"))));

    sql::Statement check_step(
        raw_db.GetUniqueStatement("SELECT COUNT(*) FROM task_steps"));
    ASSERT_TRUE(check_step.Step());
    EXPECT_EQ(check_step.ColumnInt(0), 0);

    sql::Statement check_param(
        raw_db.GetUniqueStatement("SELECT COUNT(*) FROM task_parameters"));
    ASSERT_TRUE(check_param.Step());
    EXPECT_EQ(check_param.ColumnInt(0), 0);

    sql::Statement check_obs(
        raw_db.GetUniqueStatement("SELECT COUNT(*) FROM task_observations"));
    ASSERT_TRUE(check_obs.Step());
    EXPECT_EQ(check_obs.ColumnInt(0), 0);

    sql::Statement check_val(raw_db.GetUniqueStatement(
        "SELECT COUNT(*) FROM task_parameter_values"));
    ASSERT_TRUE(check_val.Step());
    EXPECT_EQ(check_val.ColumnInt(0), 0);
  }
}

TEST_F(TaskDatabaseTest, StepReorderingUniqueness) {
  TaskDefinition definition;
  definition.set_title("Steps Test");
  definition.set_url("https://example.com");

  TaskStep* step0 = definition.add_task_steps();
  step0->set_step_index(0);
  step0->set_description("Step A");
  step0->set_url("https://example.com/a");

  TaskStep* step1 = definition.add_task_steps();
  step1->set_step_index(1);
  step1->set_description("Step B");
  step1->set_url("https://example.com/b");

  int64_t def_id = db_->SaveTaskDefinition(std::nullopt, definition);
  EXPECT_GT(def_id, 0);

  std::optional<TaskDefinition> saved = db_->GetTaskDefinition(def_id);
  ASSERT_TRUE(saved.has_value());
  int64_t step0_id = saved->task_steps(0).id();
  int64_t step1_id = saved->task_steps(1).id();

  // Reorder Steps: Swap their indices in-place (Step A gets index 1, Step B
  // gets index 0).
  TaskDefinition reordered = *saved;
  reordered.mutable_task_steps(0)->set_step_index(1);
  reordered.mutable_task_steps(1)->set_step_index(0);

  int64_t reorder_res = db_->SaveTaskDefinition(def_id, reordered);
  EXPECT_EQ(reorder_res, def_id);

  // Verify swapped states but preserved surrogate primary row keys.
  std::optional<TaskDefinition> retrieved = db_->GetTaskDefinition(def_id);
  ASSERT_TRUE(retrieved.has_value());
  ASSERT_EQ(retrieved->task_steps_size(), 2);

  // Step at retrieved index 0 has step_index 0, url /b, and id step1_id.
  EXPECT_EQ(retrieved->task_steps(0).step_index(), 0);
  EXPECT_EQ(retrieved->task_steps(0).url(), "https://example.com/b");
  EXPECT_EQ(retrieved->task_steps(0).id(), step1_id);

  // Step at retrieved index 1 has step_index 1, url /a, and id step0_id.
  EXPECT_EQ(retrieved->task_steps(1).step_index(), 1);
  EXPECT_EQ(retrieved->task_steps(1).url(), "https://example.com/a");
  EXPECT_EQ(retrieved->task_steps(1).id(), step0_id);
}

TEST_F(TaskDatabaseTest, SaveTaskDefinitionConstraintViolationRollback) {
  TaskDefinition definition;
  definition.set_title("Constraints Test");
  definition.set_url("https://example.com");

  TaskStep* step0 = definition.add_task_steps();
  step0->set_step_index(0);
  step0->set_description("Step A");
  step0->set_url("https://example.com/a");

  TaskParameter* param0 = step0->add_parameters();
  param0->set_key("p1");
  param0->set_name("Param 1");
  param0->set_type("string");

  TaskParameter* param1 = step0->add_parameters();
  param1->set_key("p1");  // Duplicate key!
  param1->set_name("Param 1 Duplicate");
  param1->set_type("string");

  // Save should fail and return 0 due to unique constraint index rollback.
  int64_t result = db_->SaveTaskDefinition(std::nullopt, definition);
  EXPECT_EQ(result, 0);

  // Verify that no definition was inserted into the database.
  std::vector<TaskDefinition> defs =
      db_->GetTaskDefinitionsByUrl("https://example.com");
  EXPECT_EQ(defs.size(), 0u);
}

TEST_F(TaskDatabaseTest, DeleteStepCascadesParameters) {
  TaskDefinition definition;
  definition.set_title("Cascading Test");
  definition.set_url("https://example.com");

  // Step 0
  TaskStep* step0 = definition.add_task_steps();
  step0->set_step_index(0);
  step0->set_description("Step 0");
  step0->set_url("https://example.com/0");

  TaskParameter* param0 = step0->add_parameters();
  param0->set_key("p0");
  param0->set_name("Param 0");
  param0->set_type("string");

  // Step 1
  TaskStep* step1 = definition.add_task_steps();
  step1->set_step_index(1);
  step1->set_description("Step 1");
  step1->set_url("https://example.com/1");

  TaskParameter* param1 = step1->add_parameters();
  param1->set_key("p1");
  param1->set_name("Param 1");
  param1->set_type("string");

  // Save definition.
  int64_t def_id = db_->SaveTaskDefinition(std::nullopt, definition);
  EXPECT_GT(def_id, 0);

  std::optional<TaskDefinition> saved = db_->GetTaskDefinition(def_id);
  ASSERT_TRUE(saved.has_value());
  ASSERT_EQ(saved->task_steps_size(), 2);
  EXPECT_EQ(saved->task_steps(0).parameters_size(), 1);
  EXPECT_EQ(saved->task_steps(1).parameters_size(), 1);

  // Now update the definition: remove the second step (which triggers
  // DeleteStepById).
  TaskDefinition updated = *saved;
  updated.mutable_task_steps()->RemoveLast();
  ASSERT_EQ(updated.task_steps_size(), 1);

  int64_t update_res = db_->SaveTaskDefinition(def_id, updated);
  EXPECT_EQ(update_res, def_id);

  // Close the main connection to verify DB state raw on disk.
  db_.reset();

  {
    sql::Database raw_db(sql::Database::Tag("ReplayTasks"));
    ASSERT_TRUE(raw_db.Open(temp_dir_.GetPath().Append(
        FILE_PATH_LITERAL("ReplayTaskDatabase.db"))));

    // Verify step 1 was deleted: only 1 step left in database.
    sql::Statement check_step(
        raw_db.GetUniqueStatement("SELECT COUNT(*) FROM task_steps"));
    ASSERT_TRUE(check_step.Step());
    EXPECT_EQ(check_step.ColumnInt(0), 1);

    // Verify parameter associated with step 1 was also deleted: only 1
    // parameter left.
    sql::Statement check_param(
        raw_db.GetUniqueStatement("SELECT COUNT(*) FROM task_parameters"));
    ASSERT_TRUE(check_param.Step());
    EXPECT_EQ(check_param.ColumnInt(0), 1);
  }
}

TEST_F(TaskDatabaseTest, SaveAndRetrieveObservations) {
  TaskDefinition definition;
  definition.set_title("Amtrak Booking");
  definition.set_url("https://www.amtrak.com");

  TaskStep* step = definition.add_task_steps();
  step->set_step_index(0);
  step->set_description("Select seats");
  step->set_url("https://www.amtrak.com/seatSelection");

  TaskParameter* param = step->add_parameters();
  param->set_key("seat_type");
  param->set_name("Seat Preference");
  param->set_type("string");

  int64_t def_id = db_->SaveTaskDefinition(std::nullopt, definition);
  EXPECT_GT(def_id, 0);

  std::optional<TaskDefinition> saved_definition =
      db_->GetTaskDefinition(def_id);
  ASSERT_TRUE(saved_definition.has_value());

  // Save execution log 1
  TaskObservation obs1;
  *obs1.mutable_definition() = *saved_definition;
  obs1.set_start_time((base::Time::Now() - base::Minutes(10))
                          .ToDeltaSinceWindowsEpoch()
                          .InMicroseconds());
  obs1.set_end_time((base::Time::Now() - base::Minutes(10))
                        .ToDeltaSinceWindowsEpoch()
                        .InMicroseconds());
  obs1.set_execution_source(ExecutionSource::AUTOMATIC);
  obs1.mutable_definition()
      ->mutable_task_steps(0)
      ->mutable_parameters(0)
      ->set_value("Window");

  int64_t obs_id1 = db_->SaveObservation(obs1);
  EXPECT_GT(obs_id1, 0);

  // Save execution log 2 (More recent)
  TaskObservation obs2;
  *obs2.mutable_definition() = *saved_definition;
  obs2.set_start_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  obs2.set_end_time(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  obs2.set_execution_source(ExecutionSource::MANUAL);
  obs2.mutable_definition()
      ->mutable_task_steps(0)
      ->mutable_parameters(0)
      ->set_value("Aisle");

  int64_t obs_id2 = db_->SaveObservation(obs2);
  EXPECT_GT(obs_id2, 0);

  // Fetch Observations
  std::vector<TaskObservation> observations =
      db_->GetObservationsForDefinition(def_id);
  ASSERT_EQ(observations.size(), 2u);

  // Must be sorted descending chronological
  EXPECT_EQ(observations[0].id(), obs_id2);
  EXPECT_EQ(observations[0].start_time(), obs2.start_time());
  EXPECT_EQ(observations[0].execution_source(), ExecutionSource::MANUAL);
  EXPECT_EQ(observations[0].definition().task_steps(0).parameters(0).value(),
            "Aisle");

  EXPECT_EQ(observations[1].id(), obs_id1);
  EXPECT_EQ(observations[1].start_time(), obs1.start_time());
  EXPECT_EQ(observations[1].execution_source(), ExecutionSource::AUTOMATIC);
  EXPECT_EQ(observations[1].definition().task_steps(0).parameters(0).value(),
            "Window");
}

TEST_F(TaskDatabaseTest, PruneOldObservations) {
  base::ScopedTempDir local_temp_dir;
  ASSERT_TRUE(local_temp_dir.CreateUniqueTempDir());

  int64_t def_id = 0;
  int64_t recent_obs_time = 0;

  {
    TaskDatabase writer_db;
    writer_db.Init(local_temp_dir.GetPath());

    TaskDefinition definition;
    definition.set_title("Seed Amtrak");
    definition.set_url("https://www.amtrak.com");
    def_id = writer_db.SaveTaskDefinition(std::nullopt, definition);

    std::optional<TaskDefinition> saved = writer_db.GetTaskDefinition(def_id);
    ASSERT_TRUE(saved.has_value());

    // Write recent execution log (10 days old)
    TaskObservation obs1;
    *obs1.mutable_definition() = *saved;
    recent_obs_time = (base::Time::Now() - base::Days(10))
                          .ToDeltaSinceWindowsEpoch()
                          .InMicroseconds();
    obs1.set_start_time(recent_obs_time);
    obs1.set_end_time(recent_obs_time);
    obs1.set_execution_source(ExecutionSource::AUTOMATIC);
    EXPECT_GT(writer_db.SaveObservation(obs1), 0);

    // Write stale execution log (366 days old)
    TaskObservation obs2;
    *obs2.mutable_definition() = *saved;
    int64_t old_obs_time = (base::Time::Now() - base::Days(366))
                               .ToDeltaSinceWindowsEpoch()
                               .InMicroseconds();
    obs2.set_start_time(old_obs_time);
    obs2.set_end_time(old_obs_time);
    obs2.set_execution_source(ExecutionSource::MANUAL);
    EXPECT_GT(writer_db.SaveObservation(obs2), 0);

    EXPECT_EQ(writer_db.GetObservationsForDefinition(def_id).size(), 2u);
  }

  // Init triggers PruneOldObservations(base::Days(365)) synchronously on
  // startup.
  TaskDatabase reader_db;
  reader_db.Init(local_temp_dir.GetPath());

  std::vector<TaskObservation> retrieved =
      reader_db.GetObservationsForDefinition(def_id);
  ASSERT_EQ(retrieved.size(), 1u);
  EXPECT_EQ(retrieved[0].start_time(), recent_obs_time);
}

}  // namespace record_replay
