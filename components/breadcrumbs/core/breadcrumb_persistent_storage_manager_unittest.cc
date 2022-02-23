// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_persistent_storage_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "components/breadcrumbs/core/breadcrumb_persistent_storage_util.h"
#include "testing/platform_test.h"

namespace breadcrumbs {

namespace {

// Estimate number of events too large to fit in the persisted file. 6 is based
// on the event string format which is slightly smaller than each event.
constexpr unsigned long kEventCountTooManyForPersisting =
    kPersistedFilesizeInBytes / 6.0;

// Validates that the events in `persisted_events` are contiguous and that the
// `last_logged_event` matches the last persisted event.
bool ValidatePersistedEvents(const std::string& last_logged_event,
                             std::vector<std::string> persisted_events) {
  if (persisted_events.empty())
    return false;

  // The newest event is at the back of the vector.
  const std::string last_event = persisted_events.back();
  if (last_event.find(last_logged_event) == std::string::npos)
    return false;

  // The oldest event is at the front of the vector.
  const std::string first_event = persisted_events.front();
  int first_event_index;
  if (!base::StringToInt(first_event.substr(first_event.find_last_of(' ') + 1),
                         &first_event_index)) {
    // `first_event_index` could not be parsed.
    return false;
  }

  int last_event_index;
  if (!base::StringToInt(last_event.substr(last_event.find_last_of(' ') + 1),
                         &last_event_index)) {
    // `last_event_index` could not be parsed.
    return false;
  }

  // Validate no events are missing from within the persisted range.
  return last_event_index - first_event_index + 1 ==
         static_cast<int>(persisted_events.size());
}

// Creates and returns a new file at `path`, overwriting any existing file.
base::File CreateFile(const base::FilePath& path) {
  constexpr int kFlags =
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE;
  return base::File(path, kFlags);
}

}  // namespace

class BreadcrumbPersistentStorageManagerTest : public PlatformTest {
 protected:
  BreadcrumbPersistentStorageManagerTest() {
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    persistent_storage_ = std::make_unique<BreadcrumbPersistentStorageManager>(
        scoped_temp_dir_.GetPath());
    breadcrumb_manager_service_.StartPersisting(persistent_storage_.get());
  }

  ~BreadcrumbPersistentStorageManagerTest() override {
    breadcrumb_manager_service_.StopPersisting();
  }

  // Calls `GetStoredEvents()` and wait for its posted tasks to complete.
  std::vector<std::string> GetPersistedEvents() {
    base::RunLoop run_loop;
    persistent_storage_->GetStoredEvents(
        base::BindOnce(&BreadcrumbPersistentStorageManagerTest::WaitForEvents,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return events_;
  }

  // Run `quit_closure` to signal to the run loop that the function being tested
  // has completed, and sets `events_` to the `events` vector received from the
  // function being tested (so that tests may make assertions about the events).
  void WaitForEvents(base::OnceClosure quit_closure,
                     std::vector<std::string> events) {
    events_.clear();
    events_ = events;
    std::move(quit_closure).Run();
  }

  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir scoped_temp_dir_;
  BreadcrumbManagerKeyedService breadcrumb_manager_service_{
      /*is_off_the_record=*/false};
  std::unique_ptr<BreadcrumbPersistentStorageManager> persistent_storage_;

  // Stores events returned by BreadcrumbPersistentStorageManager::GetEvents().
  std::vector<std::string> events_;
};

// Ensures that logged events are persisted.
TEST_F(BreadcrumbPersistentStorageManagerTest, PersistEvents) {
  breadcrumb_manager_service_.AddEvent("event");

  // Advance clock to trigger writing final events.
  task_env_.FastForwardBy(base::Minutes(1));

  const auto events = GetPersistedEvents();
  ASSERT_EQ(1ul, events.size());
  EXPECT_NE(std::string::npos, events.front().find("event"));
}

// Ensures that persisted events do not grow too large for a single large
// event bucket when events are logged very quickly one after the other.
TEST_F(BreadcrumbPersistentStorageManagerTest, PersistLargeBucket) {
  std::string event;
  unsigned long event_count = 0;
  while (event_count < kEventCountTooManyForPersisting) {
    event = base::StringPrintf("event %lu", event_count);
    breadcrumb_manager_service_.AddEvent(event);
    task_env_.FastForwardBy(base::Milliseconds(1));

    event_count++;
  }

  // Advance clock to trigger writing final events.
  task_env_.FastForwardBy(base::Minutes(1));

  const auto events = GetPersistedEvents();
  EXPECT_LT(events.size(), event_count);
  EXPECT_TRUE(ValidatePersistedEvents(event, events));
}

// Ensures that persisted events do not grow too large for events logged a few
// seconds apart from each other.
TEST_F(BreadcrumbPersistentStorageManagerTest, PersistManyEventsOverTime) {
  std::string event;
  unsigned long event_count = 0;
  while (event_count < kEventCountTooManyForPersisting) {
    event = base::StringPrintf("event %lu", event_count);
    breadcrumb_manager_service_.AddEvent(event);
    task_env_.FastForwardBy(base::Seconds(1));

    event_count++;
  }

  // Advance clock to trigger writing final events.
  task_env_.FastForwardBy(base::Minutes(1));

  const auto events = GetPersistedEvents();
  ASSERT_GT(events.size(), 0ul);
  EXPECT_LT(events.size(), event_count);
  EXPECT_TRUE(ValidatePersistedEvents(event, events));
}

// Ensures that old events are removed from the persisted file when old buckets
// are dropped.
TEST_F(BreadcrumbPersistentStorageManagerTest,
       OldEventsRemovedFromPersistedFile) {
  std::string event;
  unsigned long event_counter = 0;
  constexpr int kNumEventsPerBucket = 200;
  constexpr int kNumEvents = kNumEventsPerBucket * 3;
  while (event_counter < kNumEvents) {
    event = base::StringPrintf("event %lu", event_counter);
    breadcrumb_manager_service_.AddEvent(event);
    event_counter++;

    if (event_counter % kNumEventsPerBucket == 0)
      task_env_.FastForwardBy(base::Hours(1));
  }

  // Advance clock to trigger writing final events.
  task_env_.FastForwardBy(base::Minutes(1));

  // The exact number of events could vary based on changes in the
  // implementation. The important part of this test is to verify that a single
  // event bucket will not grow unbounded and it will be limited to a value
  // smaller than the overall total number of events which have been logged.
  const auto events = GetPersistedEvents();
  EXPECT_LT(events.size(), event_counter);
  EXPECT_TRUE(ValidatePersistedEvents(event, events));
}

// Ensures that events are read correctly if the persisted file becomes
// corrupted by losing the EOF token or if kPersistedFilesizeInBytes is reduced.
TEST_F(BreadcrumbPersistentStorageManagerTest,
       GetStoredEventsAfterFilesizeReduction) {
  const base::FilePath breadcrumbs_file_path =
      GetBreadcrumbPersistentStorageFilePath(scoped_temp_dir_.GetPath());
  base::File file = CreateFile(breadcrumbs_file_path);
  ASSERT_TRUE(file.IsValid());

  // Simulate an old persisted file larger than the current one.
  const size_t old_filesize = kPersistedFilesizeInBytes * 1.2;
  std::string past_breadcrumbs;
  unsigned long written_events = 0;
  while (past_breadcrumbs.length() < old_filesize) {
    past_breadcrumbs += "08:27 event\n";
    written_events++;
  }

  ASSERT_TRUE(file.WriteAndCheck(
      /*offset=*/0, base::as_bytes(base::make_span(past_breadcrumbs))));
  ASSERT_TRUE(file.Flush());
  file.Close();

  const auto events = GetPersistedEvents();
  EXPECT_GT(events.size(), 1ul);
  EXPECT_LT(events.size(), written_events);
}

using BreadcrumbPersistentStorageManagerFilenameTest = PlatformTest;

TEST_F(BreadcrumbPersistentStorageManagerFilenameTest,
       MigrateOldBreadcrumbFiles) {
  base::test::TaskEnvironment task_env;
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath scoped_temp_dir_path = scoped_temp_dir.GetPath();

  // Create breadcrumb file and temp file with old filenames.
  const base::FilePath old_breadcrumb_file_path =
      scoped_temp_dir_path.Append(FILE_PATH_LITERAL("old breadcrumb file"));
  const base::FilePath old_temp_file_path =
      scoped_temp_dir_path.Append(FILE_PATH_LITERAL("old temp file"));
  base::File old_breadcrumb_file = CreateFile(old_breadcrumb_file_path);
  base::File old_temp_file = CreateFile(old_temp_file_path);
  ASSERT_TRUE(old_breadcrumb_file.IsValid());
  ASSERT_TRUE(old_temp_file.IsValid());
  old_temp_file.Close();

  // Write some test data to the breadcrumb file.
  const std::string test_data = "breadcrumb file test data";
  ASSERT_NE(-1,
            old_breadcrumb_file.Write(0, test_data.c_str(), test_data.size()));
  old_breadcrumb_file.Close();

  BreadcrumbPersistentStorageManager persistent_storage(
      scoped_temp_dir_path, old_breadcrumb_file_path, old_temp_file_path);
  task_env.RunUntilIdle();

  // The old files should have been removed, and the new breadcrumb file
  // should be present.
  EXPECT_FALSE(base::PathExists(old_breadcrumb_file_path));
  EXPECT_FALSE(base::PathExists(old_temp_file_path));
  base::File new_file(
      GetBreadcrumbPersistentStorageFilePath(scoped_temp_dir_path),
      base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_TRUE(new_file.IsValid());
  const size_t test_data_size = test_data.size();
  char new_file_data[test_data_size];
  EXPECT_EQ(static_cast<int>(test_data_size),
            new_file.Read(0, new_file_data, test_data_size));
  EXPECT_EQ(test_data, std::string(new_file_data, test_data_size));
}

}  // namespace breadcrumbs
