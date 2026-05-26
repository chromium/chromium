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

}  // namespace record_replay
