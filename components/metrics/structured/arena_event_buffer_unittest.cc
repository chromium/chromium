// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/arena_event_buffer.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/metrics/structured/lib/event_buffer.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {
namespace {

// Creates an event for testing. The serialized size of this event is about 9
// bytes.
StructuredEventProto TestEvent(uint64_t id) {
  StructuredEventProto event;
  event.set_device_project_id(id);
  return event;
}

EventsProto ReadEvents(const base::FilePath& path) {
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(path, &content));
  EventsProto proto;
  EXPECT_TRUE(proto.MergeFromString(content));
  return proto;
}

}  // namespace

class ArenaEventBufferTest : public testing::Test {
 public:
  const base::TimeDelta kWriteDelay = base::Seconds(0);

  ArenaEventBufferTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("proto_file"));
  }

  base::FilePath GetAltPath() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("alt_proto_file"));
  }

  std::unique_ptr<ArenaEventBuffer> CreateTestBuffer(int32_t max_size) {
    return std::make_unique<ArenaEventBuffer>(GetPath(), kWriteDelay, max_size);
  }

  void Wait() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir temp_dir_;
};

TEST_F(ArenaEventBufferTest, OkEvent) {
  std::unique_ptr<ArenaEventBuffer> buffer = CreateTestBuffer(/*max_size=*/128);
  Wait();

  EXPECT_EQ(buffer->AddEvent(TestEvent(1)), Result::kOk);
}

TEST_F(ArenaEventBufferTest, FullEvent) {
  std::unique_ptr<ArenaEventBuffer> buffer = CreateTestBuffer(/*max_size=*/128);
  Wait();

  EXPECT_EQ(buffer->AddEvent(TestEvent(1)), Result::kOk);

  // Create an event that is larger then the heuristic.
  auto event2 = TestEvent(2);

  // Add 10 metrics
  for (int i = 0; i < 10; ++i) {
    auto* metric = event2.add_metrics();
    metric->set_name_hash(i);
    metric->set_value_string("metric value");
  }

  EXPECT_EQ(buffer->AddEvent(event2), Result::kFull);
}

TEST_F(ArenaEventBufferTest, Purge) {
  std::unique_ptr<ArenaEventBuffer> buffer = CreateTestBuffer(/*max_size=*/128);
  Wait();

  EXPECT_EQ(buffer->AddEvent(TestEvent(1)), Result::kOk);

  buffer->Purge();
  Wait();

  EXPECT_EQ(buffer->resource_info().used_size_bytes, 0);

  EXPECT_FALSE(base::PathExists(GetPath()));
}

TEST_F(ArenaEventBufferTest, UpdatePath) {
  EventsProto events;
  events.mutable_events()->Add(TestEvent(2));

  std::string content;
  ASSERT_TRUE(events.SerializeToString(&content));

  ASSERT_TRUE(base::WriteFile(GetAltPath(), content));

  std::unique_ptr<ArenaEventBuffer> buffer = CreateTestBuffer(/*max_size=*/512);
  Wait();

  EXPECT_EQ(buffer->AddEvent(TestEvent(1)), Result::kOk);

  base::FilePath new_path = GetAltPath();

  buffer->UpdatePath(new_path);
  Wait();
  EXPECT_EQ(buffer->proto()->events_size(), 2);

  EXPECT_EQ(buffer->AddEvent(TestEvent(1)), Result::kOk);
  EXPECT_EQ(buffer->proto()->events_size(), 3);
}

TEST_F(ArenaEventBufferTest, PeriodicEventBackup) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(kEventStorageManager,
                                              {{
                                                  "event_backup_time_s",
                                                  "3"  // seconds
                                              }});
  std::unique_ptr<ArenaEventBuffer> buffer = CreateTestBuffer(/*max_size=*/512);
  Wait();

  // Add an event.
  buffer->AddEvent(TestEvent(1));
  EXPECT_EQ(buffer->proto()->events_size(), 1);

  // Wait for 3 seconds for the timer to trigger a backup.
  task_environment_.FastForwardBy(base::Seconds(3));
  Wait();

  // Read the events from disk to see if the file has the expected content.
  EventsProto events = ReadEvents(GetPath());
  ASSERT_EQ(events.events_size(), 1);
  const auto& event = events.events(0);
  EXPECT_EQ(event.device_project_id(), 1ul);
}

}  // namespace metrics::structured
