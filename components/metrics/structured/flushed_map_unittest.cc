// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/flushed_map.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/metrics/structured/lib/event_buffer.h"
#include "components/metrics/structured/proto/event_storage.pb.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/structured_data.pb.h"

namespace metrics::structured {
namespace {

class TestEventBuffer : public EventBuffer<StructuredEventProto> {
 public:
  TestEventBuffer() : EventBuffer<StructuredEventProto>(ResourceInfo(1024)) {}

  // EventBuffer:
  Result AddEvent(StructuredEventProto event) override {
    events_.mutable_events()->Add(std::move(event));
    return Result::kOk;
  }

  void Purge() override { events_.Clear(); }

  google::protobuf::RepeatedPtrField<StructuredEventProto> Serialize()
      override {
    return events_.events();
  }

  void Flush(const base::FilePath& path, FlushedCallback callback) override {
    std::string content;
    EXPECT_TRUE(events_.SerializeToString(&content));
    EXPECT_TRUE(base::WriteFile(path, content));
    std::move(callback).Run(FlushedKey{
        .size = static_cast<int32_t>(content.size()),
        .path = path,
        .creation_time = base::Time::Now(),
    });
  }

  uint64_t Size() override { return events_.events_size(); }

  const EventsProto& events() const { return events_; }

 private:
  EventsProto events_;
};

StructuredEventProto BuildTestEvent(int id) {
  StructuredEventProto event;
  event.set_device_project_id(id);
  return event;
}

TestEventBuffer BuildTestBuffer(const std::vector<int>& ids) {
  TestEventBuffer buffer;

  for (int id : ids) {
    EXPECT_EQ(buffer.AddEvent(BuildTestEvent(id)), Result::kOk);
  }

  return buffer;
}

EventsProto BuildTestEvents(const std::vector<int>& ids) {
  EventsProto events;

  for (int id : ids) {
    events.mutable_events()->Add(BuildTestEvent(id));
  }

  return events;
}

}  // namespace

class FlushedMapTest : public testing::Test {
 public:
  FlushedMapTest() = default;
  ~FlushedMapTest() override = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetDir() const {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("events"));
  }

  FlushedMap BuildFlushedMap(int32_t max_size = 2048) {
    return FlushedMap(GetDir(), max_size);
  }

  EventsProto ReadEventsProto(const base::FilePath& path) {
    std::string content;
    EXPECT_TRUE(base::ReadFileToString(path, &content));

    EventsProto events;
    EXPECT_TRUE(events.MergeFromString(content));
    return events;
  }

  void WriteToDisk(const base::FilePath& path, EventsProto&& events) {
    std::string content;
    EXPECT_TRUE(events.SerializeToString(&content));
    EXPECT_TRUE(base::WriteFile(path, content));
  }

  void Wait() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

  base::ScopedTempDir temp_dir_;

 protected:
};

TEST_F(FlushedMapTest, FlushFile) {
  FlushedMap map = BuildFlushedMap();
  Wait();
  auto buffer = BuildTestBuffer({1, 2, 3});

  map.Flush(buffer,
            base::BindLambdaForTesting(
                [&](base::expected<FlushedKey, FlushError> key) {
                  EXPECT_TRUE(key.has_value());
                  EXPECT_TRUE(base::PathExists(base::FilePath(key->path)));
                }));
  Wait();

  const std::vector<FlushedKey>& keys = map.keys();
  EXPECT_EQ(keys.size(), 1ul);

  int64_t file_size;
  EXPECT_TRUE(base::GetFileSize(keys[0].path, &file_size));
  EXPECT_EQ(keys[0].size, static_cast<int32_t>(file_size));
}

TEST_F(FlushedMapTest, ReadFile) {
  FlushedMap map = BuildFlushedMap();
  Wait();

  auto buffer = BuildTestBuffer({1, 2, 3});
  const EventsProto& events = buffer.events();

  map.Flush(buffer,
            base::BindLambdaForTesting(
                [&](base::expected<FlushedKey, FlushError> key) {
                  EXPECT_TRUE(key.has_value());
                  EXPECT_TRUE(base::PathExists(base::FilePath(key->path)));
                }));
  Wait();

  auto key = map.keys().front();

  std::optional<EventsProto> read_events = map.ReadKey(key);
  EXPECT_EQ(read_events->events_size(), events.events_size());

  for (int i = 0; i < read_events->events_size(); ++i) {
    EXPECT_EQ(read_events->events(i).device_project_id(),
              events.events(i).device_project_id());
  }
}

TEST_F(FlushedMapTest, UniqueFlushes) {
  FlushedMap map = BuildFlushedMap();
  Wait();

  auto events = BuildTestBuffer({1, 2, 3});
  map.Flush(events,
            base::BindLambdaForTesting(
                [&](base::expected<FlushedKey, FlushError> key) {
                  EXPECT_TRUE(key.has_value());
                  EXPECT_TRUE(base::PathExists(base::FilePath(key->path)));
                }));
  Wait();

  auto events2 = BuildTestBuffer({4, 5, 6});
  map.Flush(events2,
            base::BindLambdaForTesting(
                [&](base::expected<FlushedKey, FlushError> key) {
                  EXPECT_TRUE(key.has_value());
                  EXPECT_TRUE(base::PathExists(base::FilePath(key->path)));
                }));
  Wait();

  EXPECT_EQ(map.keys().size(), 2ul);
  const auto& key1 = map.keys()[0];
  const auto& key2 = map.keys()[1];

  EXPECT_NE(key1.path, key2.path);
}

TEST_F(FlushedMapTest, DeleteKey) {
  FlushedMap map = BuildFlushedMap();
  Wait();

  auto events = BuildTestBuffer({1, 2, 3});
  map.Flush(events, base::BindLambdaForTesting(
                        [&](base::expected<FlushedKey, FlushError> key) {
                          EXPECT_TRUE(key.has_value());
                          EXPECT_TRUE(base::PathExists(key->path));
                        }));
  Wait();

  const std::vector<FlushedKey>& keys = map.keys();
  auto key = map.keys().front();
  EXPECT_EQ(keys.size(), 1ul);
  EXPECT_TRUE(base::PathExists(key.path));

  map.DeleteKey(key);
  Wait();

  EXPECT_FALSE(base::PathExists(key.path));
}

TEST_F(FlushedMapTest, LoadPreviousSessionKeys) {
  EXPECT_TRUE(base::CreateDirectory(GetDir()));
  auto events = BuildTestEvents({1, 2, 3});
  auto events2 = BuildTestEvents({4, 5, 6});

  base::FilePath path1 = GetDir().Append(FILE_PATH_LITERAL("events"));
  base::FilePath path2 = GetDir().Append(FILE_PATH_LITERAL("events2"));

  WriteToDisk(path1, std::move(events));
  // Force a small difference in the creation time of the two files.
  base::PlatformThreadBase::Sleep(base::Seconds(1));
  WriteToDisk(path2, std::move(events2));

  FlushedMap map = BuildFlushedMap();
  Wait();

  const std::vector<FlushedKey>& keys = map.keys();
  EXPECT_EQ(keys.size(), 2ul);

  // The order the files are loaded in is unknown.
  std::vector<base::FilePath> paths;
  for (const auto& key : keys) {
    paths.push_back(base::FilePath(key.path));
  }

  EXPECT_THAT(paths, testing::ElementsAre(path1, path2));
}

TEST_F(FlushedMapTest, ExceedQuota) {
  FlushedMap map = BuildFlushedMap(/*max_size=*/64);
  Wait();

  auto events = BuildTestBuffer({1, 2, 3});
  map.Flush(events, base::BindLambdaForTesting(
                        [&](base::expected<FlushedKey, FlushError> key) {
                          EXPECT_TRUE(key.has_value());
                          EXPECT_TRUE(base::PathExists(key->path));
                        }));
  Wait();

  auto events2 = BuildTestBuffer({1, 2, 3});
  map.Flush(events2, base::BindLambdaForTesting(
                         [&](base::expected<FlushedKey, FlushError> key) {
                           EXPECT_FALSE(key.has_value());
                           const FlushError err = key.error();
                           EXPECT_EQ(err, kQuotaExceeded);
                         }));
  Wait();
}

}  // namespace metrics::structured
