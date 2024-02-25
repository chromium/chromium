// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/signal_database_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/proto/signal.pb.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

bool IsWithinOneSecond(base::Time t1, base::Time t2) {
  return (t1 - t2).magnitude() < base::Seconds(1);
}

void CheckVectorsEqual(const std::vector<SignalDatabase::Sample>& expected_list,
                       const std::vector<SignalDatabase::Sample>& actual_list) {
  EXPECT_EQ(expected_list.size(), actual_list.size());
  unsigned int equal_count = 0;
  for (const auto& expected : expected_list) {
    for (const auto& actual : actual_list) {
      if (expected.second == actual.second &&
          IsWithinOneSecond(expected.first, actual.first)) {
        equal_count++;
      }
    }
  }

  EXPECT_EQ(equal_count, actual_list.size());
}

void CheckVectorsEqual(
    const std::vector<SignalDatabase::DbEntry>& expected_list,
    const std::vector<SignalDatabase::DbEntry>& actual_list) {
  EXPECT_EQ(expected_list.size(), actual_list.size());
  unsigned int equal_count = 0;
  for (const auto& expected : expected_list) {
    for (const auto& actual : actual_list) {
      if (expected.name_hash == actual.name_hash &&
          expected.type == actual.type && expected.value == actual.value &&
          IsWithinOneSecond(expected.time, actual.time)) {
        equal_count++;
      }
    }
  }

  EXPECT_EQ(equal_count, actual_list.size());
}

class SignalDatabaseImplTest : public testing::Test {
 public:
  SignalDatabaseImplTest() {
    // To avoid going before epoc in processing.
    test_clock_.Advance(base::Days(100));
    feature_list_.InitAndEnableFeature(
        features::kSegmentationPlatformSignalDbCache);
  }
  ~SignalDatabaseImplTest() override = default;

  void OnGetSamples(std::vector<SignalDatabase::DbEntry> samples) {
    get_samples_result_.swap(samples);
  }

  void OnAllGetSamples(std::vector<SignalDatabase::DbEntry> samples) {
    get_all_samples_result_.swap(samples);
  }

 protected:
  void SetUpDB() {
    DCHECK(!db_);
    DCHECK(!signal_db_);

    auto db = std::make_unique<leveldb_proto::test::FakeDB<proto::SignalData>>(
        &db_entries_);
    db_ = db.get();
    signal_db_ = std::make_unique<SignalDatabaseImpl>(
        std::move(db), &test_clock_,
        task_environment_.GetMainThreadTaskRunner());

    signal_db_->Initialize(base::DoNothing());
    db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    db_->LoadCallback(true);

    test_clock_.SetNow(base::Time::Now().UTCMidnight() + base::Hours(8));
  }

  void TearDown() override {
    db_entries_.clear();
    db_ = nullptr;
    signal_db_.reset();
    task_environment_.RunUntilIdle();
  }

  void ExpectGetSamples(
      proto::SignalType type,
      uint64_t name_hash,
      base::Time start_time,
      const std::vector<SignalDatabase::DbEntry>& expected_list) {
    ExpectGetSamples(type, name_hash, start_time, test_clock_.Now(),
                     expected_list);
  }

  void ExpectGetSamples(
      proto::SignalType type,
      uint64_t name_hash,
      base::Time start_time,
      base::Time end_time,
      const std::vector<SignalDatabase::DbEntry>& expected_list) {
    signal_db_->GetSamples(type, name_hash, start_time, end_time,
                           base::BindOnce(&SignalDatabaseImplTest::OnGetSamples,
                                          base::Unretained(this)));
    db_->LoadCallback(true);
    CheckVectorsEqual(expected_list, get_samples_result_);
  }

  void ExpectGetAllSamples(
      const std::vector<SignalDatabase::DbEntry>& expected_list) {
    const auto& samples = *signal_db_->GetAllSamples();
    CheckVectorsEqual(expected_list, samples);
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::SimpleTestClock test_clock_;
  std::vector<SignalDatabase::DbEntry> get_samples_result_;
  std::vector<SignalDatabase::DbEntry> get_all_samples_result_;
  std::map<std::string, proto::SignalData> db_entries_;
  raw_ptr<leveldb_proto::test::FakeDB<proto::SignalData>> db_{nullptr};
  std::unique_ptr<SignalDatabaseImpl> signal_db_;
};

TEST_F(SignalDatabaseImplTest, WriteSampleAndRead) {
  SetUpDB();
  base::Time now = base::Time::Now().UTCMidnight() + base::Hours(8);

  uint64_t name_hash = 1234;
  proto::SignalType signal_type = proto::SignalType::HISTOGRAM_VALUE;

  // No entries to begin with.
  ExpectGetSamples(signal_type, name_hash, now.UTCMidnight(), {});
  ExpectGetAllSamples({});

  // Write a sample.
  int32_t value = 10;
  base::Time timestamp = now - base::Hours(1);
  test_clock_.SetNow(timestamp);
  signal_db_->WriteSample(signal_type, name_hash, value, base::DoNothing());
  db_->UpdateCallback(true);

  // Read back the sample and verify.
  ExpectGetSamples(signal_type, name_hash, now.UTCMidnight(),
                   {SignalDatabase::DbEntry{.type = signal_type,
                                            .name_hash = name_hash,
                                            .time = timestamp,
                                            .value = value}});
  ExpectGetAllSamples({SignalDatabase::DbEntry{.type = signal_type,
                                               .name_hash = name_hash,
                                               .time = timestamp,
                                               .value = value}});
  EXPECT_EQ(1u, db_entries_.size());

  // Write another sample right away. Both the values should be persisted
  // correctly without being overwritten.
  int32_t value2 = 20;
  signal_db_->WriteSample(signal_type, name_hash, value2, base::DoNothing());
  db_->UpdateCallback(true);

  std::vector<SignalDatabase::DbEntry> expected = {
      SignalDatabase::DbEntry{.type = signal_type,
                              .name_hash = name_hash,
                              .time = timestamp,
                              .value = value},
      SignalDatabase::DbEntry{.type = signal_type,
                              .name_hash = name_hash,
                              .time = timestamp,
                              .value = value2}};
  ExpectGetSamples(signal_type, name_hash, now.UTCMidnight(), expected);
  ExpectGetAllSamples(expected);
  EXPECT_EQ(1u, db_entries_.size());
}

TEST_F(SignalDatabaseImplTest, WriteSampleAndReadWithPrefixMismatch) {
  SetUpDB();
  base::Time now = base::Time::Now().UTCMidnight() + base::Hours(8);

  uint64_t name_hash_1 = 1234;
  uint64_t name_hash_2 = name_hash_1;
  uint64_t name_hash_3 = 1235;
  uint64_t name_hash_4 = name_hash_3;
  proto::SignalType signal_type_1 = proto::SignalType::HISTOGRAM_VALUE;
  proto::SignalType signal_type_2 = proto::SignalType::USER_ACTION;
  proto::SignalType signal_type_3 = proto::SignalType::HISTOGRAM_VALUE;
  proto::SignalType signal_type_4 = proto::SignalType::USER_ACTION;

  // Write a sample for signal 1.
  int32_t value = 10;
  base::Time timestamp = now - base::Hours(1);
  test_clock_.SetNow(timestamp);
  signal_db_->WriteSample(signal_type_1, name_hash_1, value, base::DoNothing());
  db_->UpdateCallback(true);

  // Read samples for signal 2 and verify.
  ExpectGetSamples(signal_type_2, name_hash_2, now.UTCMidnight(), {});
  // Read samples for signal 3 and verify.
  ExpectGetSamples(signal_type_3, name_hash_3, now.UTCMidnight(), {});
  // Read samples for signal 4 and verify.
  ExpectGetSamples(signal_type_4, name_hash_4, now.UTCMidnight(), {});
  ExpectGetAllSamples({
      SignalDatabase::DbEntry{.type = signal_type_1,
                              .name_hash = name_hash_1,
                              .time = timestamp,
                              .value = value},
  });
}

TEST_F(SignalDatabaseImplTest, DeleteSamples) {
  SetUpDB();

  proto::SignalType signal_type = proto::SignalType::USER_ACTION;
  uint64_t name_hash = 1234;
  base::Time timestamp1 = test_clock_.Now() - base::Hours(3);
  base::Time timestamp2 = timestamp1 + base::Hours(1);
  base::Time timestamp3 = timestamp2 + base::Hours(1);

  // Write two samples, at timestamp1 and timestamp3.
  test_clock_.SetNow(timestamp1);
  signal_db_->WriteSample(signal_type, name_hash, std::nullopt,
                          base::DoNothing());
  db_->UpdateCallback(true);
  EXPECT_EQ(1u, db_entries_.size());

  test_clock_.SetNow(timestamp3);
  signal_db_->WriteSample(signal_type, name_hash, std::nullopt,
                          base::DoNothing());
  db_->UpdateCallback(true);
  EXPECT_EQ(2u, db_entries_.size());

  // Now delete samples till timestamp2 and verify.
  signal_db_->DeleteSamples(signal_type, name_hash, timestamp2,
                            base::DoNothing());
  db_->LoadCallback(true);
  db_->UpdateCallback(true);
  EXPECT_EQ(1u, db_entries_.size());

  // Now delete samples till timestamp3 and verify.
  signal_db_->DeleteSamples(signal_type, name_hash, timestamp3,
                            base::DoNothing());
  db_->LoadCallback(true);
  db_->UpdateCallback(true);
  EXPECT_EQ(0u, db_entries_.size());

  // Try deleting again for the same period.
  signal_db_->DeleteSamples(signal_type, name_hash, timestamp3,
                            base::DoNothing());
  db_->LoadCallback(true);
  db_->UpdateCallback(true);
  EXPECT_EQ(0u, db_entries_.size());
}

TEST_F(SignalDatabaseImplTest, WriteMultipleSamplesAndRunCompaction) {
  // Set up three consecutive date timestamps, each at 8:00AM.
  base::Time day1 =
      base::Time::Now().UTCMidnight() + base::Hours(8) - base::Days(2);
  base::Time day2 = day1 + base::Days(1);
  base::Time day3 = day2 + base::Days(1);
  base::Time end_of_day1 =
      day1.UTCMidnight() + base::Days(1) - base::Seconds(1);
  base::Time end_of_day2 = end_of_day1 + base::Days(1);
  base::Time end_of_day3 = end_of_day2 + base::Days(1);

  SetUpDB();
  EXPECT_EQ(0u, db_entries_.size());

  proto::SignalType signal_type = proto::SignalType::USER_ACTION;
  uint64_t name_hash = 1234;

  // Collect two samples on day1, and one on day2.
  base::Time timestamp_day1_1 = day1 + base::Hours(1);
  base::Time timestamp_day1_2 = day1 + base::Hours(2);
  base::Time timestamp_day2_1 = day2 + base::Hours(2);

  test_clock_.SetNow(timestamp_day1_1);
  signal_db_->WriteSample(signal_type, name_hash, std::nullopt,
                          base::DoNothing());
  db_->UpdateCallback(true);

  test_clock_.SetNow(timestamp_day1_2);
  signal_db_->WriteSample(signal_type, name_hash, std::nullopt,
                          base::DoNothing());
  db_->UpdateCallback(true);

  test_clock_.SetNow(timestamp_day2_1);
  signal_db_->WriteSample(signal_type, name_hash, std::nullopt,
                          base::DoNothing());
  db_->UpdateCallback(true);
  std::vector<SignalDatabase::DbEntry> all_cached_samples = {
      SignalDatabase::DbEntry{.type = signal_type,
                              .name_hash = name_hash,
                              .time = timestamp_day1_1,
                              .value = 0},
      SignalDatabase::DbEntry{.type = signal_type,
                              .name_hash = name_hash,
                              .time = timestamp_day1_2,
                              .value = 0},
      SignalDatabase::DbEntry{.type = signal_type,
                              .name_hash = name_hash,
                              .time = timestamp_day2_1,
                              .value = 0}};

  EXPECT_EQ(3u, db_entries_.size());

  // Verify samples for the day1. There should be two of them.
  ExpectGetSamples(signal_type, name_hash, day1.UTCMidnight(),
                   day2.UTCMidnight(),
                   {
                       SignalDatabase::DbEntry{.type = signal_type,
                                               .name_hash = name_hash,
                                               .time = timestamp_day1_1,
                                               .value = 0},
                       SignalDatabase::DbEntry{.type = signal_type,
                                               .name_hash = name_hash,
                                               .time = timestamp_day1_2,
                                               .value = 0},
                   });
  ExpectGetAllSamples(all_cached_samples);

  // Compact samples for the day1 and verify. We will have two samples, but one
  // less entry.
  signal_db_->CompactSamplesForDay(signal_type, name_hash, end_of_day1,
                                   base::DoNothing());
  db_->LoadCallback(true);
  db_->UpdateCallback(true);
  signal_db_->CompactSamplesForDay(signal_type, name_hash, end_of_day1,
                                   base::DoNothing());
  db_->LoadCallback(true);
  signal_db_->CompactSamplesForDay(signal_type, name_hash, end_of_day1,
                                   base::DoNothing());
  db_->LoadCallback(true);

  ExpectGetSamples(signal_type, name_hash, day1.UTCMidnight(),
                   day2.UTCMidnight(),
                   {
                       SignalDatabase::DbEntry{.type = signal_type,
                                               .name_hash = name_hash,
                                               .time = timestamp_day1_1,
                                               .value = 0},
                       SignalDatabase::DbEntry{.type = signal_type,
                                               .name_hash = name_hash,
                                               .time = timestamp_day1_2,
                                               .value = 0},
                   });
  ExpectGetAllSamples(all_cached_samples);

  EXPECT_EQ(2u, db_entries_.size());

  // Compact samples for the day2 and verify.
  signal_db_->CompactSamplesForDay(signal_type, name_hash, end_of_day2,
                                   base::DoNothing());
  db_->LoadCallback(true);

  ExpectGetSamples(signal_type, name_hash, day2.UTCMidnight(),
                   day3.UTCMidnight(),
                   {
                       SignalDatabase::DbEntry{.type = signal_type,
                                               .name_hash = name_hash,
                                               .time = timestamp_day2_1,
                                               .value = 0},
                   });
  ExpectGetAllSamples(all_cached_samples);

  if (all_cached_samples.size() > 0) {
    return;
  }
  EXPECT_EQ(2u, db_entries_.size());

  // Compact samples for the day3 and verify. There should be no change since
  // there are no samples.
  signal_db_->CompactSamplesForDay(signal_type, name_hash, end_of_day3,
                                   base::DoNothing());
  db_->LoadCallback(true);
  signal_db_->CompactSamplesForDay(signal_type, name_hash, end_of_day3,
                                   base::DoNothing());
  db_->LoadCallback(true);

  ExpectGetSamples(signal_type, name_hash, day3.UTCMidnight(),
                   day3.UTCMidnight() + base::Days(1), {});
  ExpectGetAllSamples(all_cached_samples);

  EXPECT_EQ(2u, db_entries_.size());

  // Read a range of samples not aligned to midnight.
  ExpectGetSamples(signal_type, name_hash, timestamp_day1_1 + base::Hours(1),
                   timestamp_day2_1 - base::Hours(1),
                   {
                       SignalDatabase::DbEntry{.type = signal_type,
                                               .name_hash = name_hash,
                                               .time = timestamp_day1_2,
                                               .value = 0},
                   });
  ExpectGetAllSamples(all_cached_samples);
}

}  // namespace segmentation_platform
