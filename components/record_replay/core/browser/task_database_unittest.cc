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

  // Close the main database connection to release file locks before raw
  // verification.
  db_.reset();

  // Verify ID is NOT in the proto blob stored on disk.
  {
    sql::Database raw_db(sql::Database::Tag("ReplayTasks"));
    ASSERT_TRUE(raw_db.Open(temp_dir_.GetPath().Append(
        FILE_PATH_LITERAL("ReplayTaskDatabase.db"))));
    sql::Statement statement(
        raw_db.GetUniqueStatement("SELECT proto FROM Recordings WHERE id=?"));
    statement.BindInt64(0, id);
    ASSERT_TRUE(statement.Step());

    Recording disk_recording;
    ASSERT_TRUE(
        disk_recording.ParseFromString(statement.ColumnBlobAsString(0)));
    EXPECT_FALSE(disk_recording.has_id());
  }
}

TEST_F(TaskDatabaseTest, SaveAndRetrieveTaskDefinition) {
  Recording recording;
  recording.set_url("https://example.com");
  int64_t recording_id = db_->AddRecording(recording);

  TaskDefinition task_definition;
  task_definition.set_title("Test Task");
  task_definition.set_description("Test Description");

  db_->SaveTaskDefinition(std::nullopt, task_definition, "https://example.com",
                          recording_id);

  std::vector<std::pair<int64_t, TaskDefinition>> retrieved =
      db_->GetTaskDefinitionsByUrl("https://example.com");
  ASSERT_EQ(retrieved.size(), 1u);
  EXPECT_EQ(retrieved[0].second.title(), "Test Task");
  EXPECT_EQ(retrieved[0].second.description(), "Test Description");
}

TEST_F(TaskDatabaseTest, GetTaskDefinitionForNonExistentId) {
  EXPECT_FALSE(db_->GetTaskDefinition(99999).has_value());
}

TEST_F(TaskDatabaseTest, CascadeDeleteTaskData) {
  Recording recording;
  recording.set_url("https://example.com");
  int64_t recording_id = db_->AddRecording(recording);

  TaskDefinition task_definition;
  task_definition.set_title("Test Task");
  db_->SaveTaskDefinition(std::nullopt, task_definition, "https://example.com",
                          recording_id);

  std::vector<std::pair<int64_t, TaskDefinition>> retrieved =
      db_->GetTaskDefinitionsByUrl("https://example.com");
  ASSERT_EQ(retrieved.size(), 1u);
  int64_t task_definition_id = retrieved[0].first;

  TaskData data;
  (*data.mutable_step_data())[0].mutable_values()->insert({"key", "value"});
  EXPECT_TRUE(db_->SaveTaskData(task_definition_id, data));

  // Verify data is there.
  EXPECT_TRUE(db_->GetTaskData(task_definition_id).has_value());

  // Delete the task definition.
  EXPECT_TRUE(db_->DeleteTaskDefinition(task_definition_id));

  // Verify data is gone due to cascade delete.
  EXPECT_FALSE(db_->GetTaskData(task_definition_id).has_value());
}

}  // namespace record_replay
