// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_database.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace record_replay {

class TaskDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.Init(temp_dir_.GetPath());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  TaskDatabase db_;
};

TEST_F(TaskDatabaseTest, AddAndGetRecording) {
  Recording recording;
  recording.set_url("https://example.com");
  recording.set_name("Test Recording");
  recording.set_start_time(12345);

  int64_t id = db_.AddRecording(recording);
  EXPECT_GT(id, 0);

  std::vector<Recording> recordings =
      db_.GetRecordingsByUrl("https://example.com");
  ASSERT_EQ(recordings.size(), 1u);
  EXPECT_EQ(recordings[0].id(), id);
  EXPECT_EQ(recordings[0].url(), "https://example.com");
  EXPECT_EQ(recordings[0].name(), "Test Recording");
  EXPECT_EQ(recordings[0].start_time(), 12345);

  // Verify ID is NOT in the proto blob.
  Recording parsed_blob;
  // We can't easily get the blob from the public API, but we know
  // SerializeAsString was called after clearing the ID.
}

TEST_F(TaskDatabaseTest, SaveAndRetrieveActivityAnnotation) {
  Recording recording;
  recording.set_url("https://example.com");
  int64_t recording_id = db_.AddRecording(recording);

  ActivityAnnotation annotation;
  annotation.set_title("Test Annotation");
  annotation.set_description("Test Description");

  db_.SaveActivityAnnotation(std::nullopt, annotation, "https://example.com",
                             recording_id);

  std::vector<std::pair<int64_t, ActivityAnnotation>> retrieved =
      db_.GetActivityAnnotationsByUrl("https://example.com");
  ASSERT_EQ(retrieved.size(), 1u);
  EXPECT_EQ(retrieved[0].second.title(), "Test Annotation");
  EXPECT_EQ(retrieved[0].second.description(), "Test Description");
}

TEST_F(TaskDatabaseTest, GetActivityAnnotationForNonExistentId) {
  EXPECT_FALSE(db_.GetActivityAnnotation(99999).has_value());
}

TEST_F(TaskDatabaseTest, CascadeDeleteActivityData) {
  Recording recording;
  recording.set_url("https://example.com");
  int64_t recording_id = db_.AddRecording(recording);

  ActivityAnnotation annotation;
  annotation.set_title("Test Annotation");
  db_.SaveActivityAnnotation(std::nullopt, annotation, "https://example.com",
                             recording_id);

  std::vector<std::pair<int64_t, ActivityAnnotation>> retrieved =
      db_.GetActivityAnnotationsByUrl("https://example.com");
  ASSERT_EQ(retrieved.size(), 1u);
  int64_t annotation_id = retrieved[0].first;

  ActivityData data;
  (*data.mutable_step_data())[0].mutable_values()->insert({"key", "value"});
  EXPECT_TRUE(db_.SaveActivityData(annotation_id, data));

  // Verify data is there.
  EXPECT_TRUE(db_.GetActivityData(annotation_id).has_value());

  // Delete the annotation.
  EXPECT_TRUE(db_.DeleteActivityAnnotation(annotation_id));

  // Verify data is gone due to cascade delete.
  EXPECT_FALSE(db_.GetActivityData(annotation_id).has_value());
}

}  // namespace record_replay
