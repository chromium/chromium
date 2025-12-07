// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/event_storage_migration.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/feature_engagement/internal/event_model_impl.h"
#include "components/feature_engagement/internal/event_storage_validator.h"
#include "components/feature_engagement/internal/event_store.h"
#include "components/feature_engagement/internal/in_memory_event_store.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "components/feature_engagement/internal/test/event_util.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

// Name of the histogram that records the status of the migration of events from
// profile storage to event storage.
const char kEventStorageMigrationStatusHistogramName[] =
    "InProductHelp.EventStorageMigration.Status";

// Verifies that two event maps are equal. It checks that they have the same
// size and that for every key in map1, the corresponding Event value in map2
// is identical.
void VerifyEventMapsEqual(const std::map<std::string, Event>& map1,
                          const std::map<std::string, Event>& map2) {
  ASSERT_EQ(map1.size(), map2.size());
  for (const auto& pair1 : map1) {
    const auto& it = map2.find(pair1.first);
    ASSERT_NE(map2.end(), it);
    test::VerifyEventsEqual(&pair1.second, &it->second);
  }
}

class MockEventStorageValidator : public EventStorageValidator {
 public:
  MockEventStorageValidator() = default;
  ~MockEventStorageValidator() override = default;

  bool ShouldStore(const std::string& event_name) const override {
    return true;
  }

  bool ShouldKeep(const std::string& event_name,
                  uint32_t event_day,
                  uint32_t current_day) const override {
    return true;
  }
};

}  // namespace

class EventStorageMigrationTest : public testing::Test {
 public:
  EventStorageMigrationTest() {
    load_callback_ = base::BindOnce(
        &EventStorageMigrationTest::MigrationCallback, base::Unretained(this));
  }

  EventStorageMigrationTest(const EventStorageMigrationTest&) = delete;
  EventStorageMigrationTest& operator=(const EventStorageMigrationTest&) =
      delete;
  ~EventStorageMigrationTest() override = default;

  void SetUp() override {
    profile_db_ =
        std::make_unique<leveldb_proto::test::FakeDB<Event>>(&profile_events_);
    device_db_ =
        std::make_unique<leveldb_proto::test::FakeDB<Event>>(&device_events_);
    auto event_store = std::make_unique<InMemoryEventStore>();
    auto storage_validator = std::make_unique<MockEventStorageValidator>();
    device_event_model_ = std::make_unique<EventModelImpl>(
        std::move(event_store), std::move(storage_validator));

    event_storage_migration_ = std::make_unique<EventStorageMigration>(
        profile_db_.get(), device_db_.get(), device_event_model_.get());

    profile_db_->Init(base::DoNothing());
    device_db_->Init(base::DoNothing());
  }

  void TearDown() override {
    device_events_.clear();
    profile_events_.clear();
    event_storage_migration_ = nullptr;
  }

 protected:
  void MigrationCallback(bool success) { migration_success_ = success; }

  std::unique_ptr<EventStorageMigration> event_storage_migration_;
  std::map<std::string, Event> profile_events_;
  std::map<std::string, Event> device_events_;
  std::unique_ptr<leveldb_proto::test::FakeDB<Event>> profile_db_;
  std::unique_ptr<leveldb_proto::test::FakeDB<Event>> device_db_;
  std::unique_ptr<EventModelImpl> device_event_model_;

  bool migration_success_;
  EventStorageMigration::MigrationCallback load_callback_;
};

TEST_F(EventStorageMigrationTest,
       SuccessfullyMigrateProfileEventsToDeviceStorage) {
  base::HistogramTester histogram_tester;

  // Populate fake Event entries.
  Event event1;
  event1.set_name("event1");
  test::SetEventCountForDay(&event1, 1, 1);

  Event event2;
  event2.set_name("event2");
  test::SetEventCountForDay(&event2, 1, 3);
  test::SetEventCountForDay(&event2, 2, 5);

  profile_events_.insert(std::pair<std::string, Event>(event1.name(), event1));
  profile_events_.insert(std::pair<std::string, Event>(event2.name(), event2));

  event_storage_migration_->Migrate(std::move(load_callback_), 6);
  profile_db_->LoadCallback(true);
  device_db_->UpdateCallback(true);

  // Validate that the events from profile db have been copied to device db.
  VerifyEventMapsEqual(device_events_, profile_events_);
  EXPECT_TRUE(migration_success_);
  histogram_tester.ExpectBucketCount(
      kEventStorageMigrationStatusHistogramName,
      static_cast<int>(
          EventStorageMigration::EventStorageMigrationStatus::kCompleted),
      1);
}

TEST_F(EventStorageMigrationTest, NoEventsToMigrate) {
  base::HistogramTester histogram_tester;

  event_storage_migration_->Migrate(std::move(load_callback_), 6);
  profile_db_->LoadCallback(true);
  device_db_->UpdateCallback(true);

  // Validate that the events from profile db have been copied to device db.
  VerifyEventMapsEqual(device_events_, profile_events_);
  EXPECT_TRUE(migration_success_);
  histogram_tester.ExpectBucketCount(
      kEventStorageMigrationStatusHistogramName,
      static_cast<int>(
          EventStorageMigration::EventStorageMigrationStatus::kCompleted),
      1);
}

TEST_F(EventStorageMigrationTest, EventLoadingErrorDuringMigration) {
  base::HistogramTester histogram_tester;

  // Populate fake Event entries.
  Event event1;
  event1.set_name("event1");
  test::SetEventCountForDay(&event1, 1, 1);

  Event event2;
  event2.set_name("event2");
  test::SetEventCountForDay(&event2, 1, 3);
  test::SetEventCountForDay(&event2, 2, 5);

  profile_events_.insert(std::pair<std::string, Event>(event1.name(), event1));
  profile_events_.insert(std::pair<std::string, Event>(event2.name(), event2));

  event_storage_migration_->Migrate(std::move(load_callback_), 6);
  profile_db_->LoadCallback(false);
  EXPECT_FALSE(migration_success_);
  histogram_tester.ExpectBucketCount(
      kEventStorageMigrationStatusHistogramName,
      static_cast<int>(
          EventStorageMigration::EventStorageMigrationStatus::kFailedToLoad),
      1);
}

TEST_F(EventStorageMigrationTest, WritingEventErrorDuringMigration) {
  base::HistogramTester histogram_tester;

  // Populate fake Event entries.
  Event event1;
  event1.set_name("event1");
  test::SetEventCountForDay(&event1, 1, 1);

  Event event2;
  event2.set_name("event2");
  test::SetEventCountForDay(&event2, 1, 3);
  test::SetEventCountForDay(&event2, 2, 5);

  profile_events_.insert(std::pair<std::string, Event>(event1.name(), event1));
  profile_events_.insert(std::pair<std::string, Event>(event2.name(), event2));

  event_storage_migration_->Migrate(std::move(load_callback_), 6);
  profile_db_->LoadCallback(true);
  device_db_->UpdateCallback(false);
  EXPECT_FALSE(migration_success_);
  histogram_tester.ExpectBucketCount(
      kEventStorageMigrationStatusHistogramName,
      static_cast<int>(
          EventStorageMigration::EventStorageMigrationStatus::kFailedToWrite),
      1);
}

}  // namespace feature_engagement
