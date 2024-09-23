// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/lib/key_data_file_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/metrics/structured/lib/histogram_util.h"
#include "components/metrics/structured/lib/proto/key.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::structured {

namespace {

// 32 byte long test key, matching the size of a real key.
constexpr char kKey[] = "abcdefghijklmnopqrstuvwxyzabcdef";

// These project, event, and metric names are used for testing.
// - project: TestProjectOne
//   - event: TestEventOne
//     - metric: TestMetricOne
//     - metric: TestMetricTwo
// - project: TestProjectTwo

// The name hash of "TestProjectOne".
constexpr uint64_t kProjectOneHash = UINT64_C(16881314472396226433);
// The name hash of "TestProjectTwo".
constexpr uint64_t kProjectTwoHash = UINT64_C(5876808001962504629);

// The name hash of "TestMetricOne".
constexpr uint64_t kMetricOneHash = UINT64_C(637929385654885975);
// The name hash of "TestMetricTwo".
constexpr uint64_t kMetricTwoHash = UINT64_C(14083999144141567134);

// The hex-encoded frst 8 bytes of SHA256(kKey), ie. the user ID for key kKey.
constexpr char kUserId[] = "2070DF23E0D95759";

// Test values and their hashes. Hashes are the first 8 bytes of:
// HMAC_SHA256(concat(hex(kMetricNHash), kValueN), kKey)
constexpr char kValueOne[] = "value one";
constexpr char kValueTwo[] = "value two";
constexpr char kValueOneHash[] = "805B8790DC69B773";
constexpr char kValueTwoHash[] = "87CEF12FB15E0B3A";

constexpr base::TimeDelta kKeyRotationPeriod = base::Days(90);

std::string HashToHex(const uint64_t hash) {
  return base::HexEncode(&hash, sizeof(uint64_t));
}

}  // namespace

class KeyDataFileDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    // Move the mock date forward from day 0, because KeyDataFileDelegate
    // assumes that day 0 is a bug.
    task_environment_.AdvanceClock(base::Days(1000));
  }

  void TearDown() override { ResetKeyData(); }

  void ResetKeyData() {
    // Manually deconstruct objects in the right order to avoid dangling raw
    // pointer.
    key_data_file_ = nullptr;
    key_data_.reset();
  }

  void ResetState() {
    ResetKeyData();
    base::DeleteFile(GetPath());
    ASSERT_FALSE(base::PathExists(GetPath()));
  }

  base::FilePath GetPath() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("keys"));
  }

  void MakeKeyDataFileDelegate() {
    auto key_data_file = std::make_unique<KeyDataFileDelegate>(
        GetPath(), base::Seconds(0), base::DoNothing());
    key_data_file_ = key_data_file.get();
    key_data_ = std::make_unique<KeyData>(std::move(key_data_file));
    Wait();
  }

  void SaveKeyData() {
    key_data_file_->WriteNowForTesting();
    Wait();
    ASSERT_TRUE(base::PathExists(GetPath()));
  }

  base::TimeDelta Today() {
    return base::Time::Now() - base::Time::UnixEpoch();
  }

  // Read the on-disk file and return the information about the key for
  // |project_name_hash|. Fails if a key does not exist.
  KeyProto GetKey(const uint64_t project_name_hash) {
    std::string proto_str;
    CHECK(base::ReadFileToString(GetPath(), &proto_str));
    KeyDataProto proto;
    CHECK(proto.ParseFromString(proto_str));

    const auto it = proto.keys().find(project_name_hash);
    CHECK(it != proto.keys().end());
    return it->second;
  }

  // Write a KeyDataProto to disk with a single key described by the
  // arguments.
  void SetupKey(const uint64_t project_name_hash,
                const std::string& key,
                const base::TimeDelta last_rotation,
                const base::TimeDelta rotation_period) {
    // It's a test logic error for the key data to exist when calling SetupKey,
    // because it will desync the in-memory proto from the underlying storage.
    ASSERT_FALSE(key_data_);

    KeyDataProto proto;
    KeyProto& key_proto = (*proto.mutable_keys())[project_name_hash];
    key_proto.set_key(key);
    key_proto.set_last_rotation(last_rotation.InDays());
    key_proto.set_rotation_period(rotation_period.InDays());

    ASSERT_TRUE(base::WriteFile(GetPath(), proto.SerializeAsString()));
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  void ExpectNoErrors() {
    histogram_tester_.ExpectTotalCount("UMA.StructuredMetrics.InternalError",
                                       0);
  }

  void ExpectKeyValidation(const int valid,
                           const int created,
                           const int rotated) {
    static const std::string histogram =
        "UMA.StructuredMetrics.KeyValidationState";
    histogram_tester_.ExpectBucketCount(histogram, KeyValidationState::kValid,
                                        valid);
    histogram_tester_.ExpectBucketCount(histogram, KeyValidationState::kCreated,
                                        created);
    histogram_tester_.ExpectBucketCount(histogram, KeyValidationState::kRotated,
                                        rotated);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<KeyData> key_data_;
  raw_ptr<KeyDataFileDelegate> key_data_file_;
};

// If there is no key store file present, check that new keys are generated for
// each project, and those keys are of the right length and different from each
// other.
TEST_F(KeyDataFileDelegateTest, GeneratesKeysForProjects) {
  // Make key data and use two keys, in order to generate them.
  MakeKeyDataFileDelegate();
  key_data_->Id(kProjectOneHash, kKeyRotationPeriod);
  key_data_->Id(kProjectTwoHash, kKeyRotationPeriod);
  SaveKeyData();

  const std::string key_one = GetKey(kProjectOneHash).key();
  const std::string key_two = GetKey(kProjectTwoHash).key();

  EXPECT_EQ(key_one.size(), 32ul);
  EXPECT_EQ(key_two.size(), 32ul);
  EXPECT_NE(key_one, key_two);

  ExpectNoErrors();
  ExpectKeyValidation(/*valid=*/0, /*created=*/2, /*rotated=*/0);
}

// When repeatedly initialized with no key store file present, ensure the keys
// generated each time are distinct.
TEST_F(KeyDataFileDelegateTest, GeneratesDistinctKeys) {
  base::flat_set<std::string> keys;

  for (int i = 1; i <= 10; ++i) {
    // Reset on-disk and in-memory state, regenerate the key, and save it to
    // disk.
    ResetState();
    MakeKeyDataFileDelegate();
    key_data_->Id(kProjectOneHash, kKeyRotationPeriod);
    SaveKeyData();

    keys.insert(GetKey(kProjectOneHash).key());
    ExpectKeyValidation(/*valid=*/0, /*created=*/i, /*rotated=*/0);
  }

  ExpectNoErrors();
  EXPECT_EQ(keys.size(), 10ul);
}

// If there is an existing key store file, check that its keys are not replaced.
TEST_F(KeyDataFileDelegateTest, ReuseExistingKeys) {
  // Create a file with one key.
  MakeKeyDataFileDelegate();
  const uint64_t id_one = key_data_->Id(kProjectOneHash, kKeyRotationPeriod);
  SaveKeyData();
  ExpectKeyValidation(/*valid=*/0, /*created=*/1, /*rotated=*/0);
  const std::string key_one = GetKey(kProjectOneHash).key();

  // Reset the in-memory state, leave the on-disk state intact.
  ResetKeyData();

  // Open the file again and check we use the same key.
  MakeKeyDataFileDelegate();
  const uint64_t id_two = key_data_->Id(kProjectOneHash, kKeyRotationPeriod);
  SaveKeyData();
  ExpectKeyValidation(/*valid=*/1, /*created=*/1, /*rotated=*/0);
  const std::string key_two = GetKey(kProjectOneHash).key();

  EXPECT_EQ(id_one, id_two);
  EXPECT_EQ(key_one, key_two);
}

// Check that different events have different hashes for the same metric and
// value.
TEST_F(KeyDataFileDelegateTest, DifferentEventsDifferentHashes) {
  MakeKeyDataFileDelegate();
  EXPECT_NE(key_data_->HmacMetric(kProjectOneHash, kMetricOneHash, "value",
                                  kKeyRotationPeriod),
            key_data_->HmacMetric(kProjectTwoHash, kMetricOneHash, "value",
                                  kKeyRotationPeriod));
  ExpectNoErrors();
}

// Check that an event has different hashes for different metrics with the same
// value.
TEST_F(KeyDataFileDelegateTest, DifferentMetricsDifferentHashes) {
  MakeKeyDataFileDelegate();
  EXPECT_NE(key_data_->HmacMetric(kProjectOneHash, kMetricOneHash, "value",
                                  kKeyRotationPeriod),
            key_data_->HmacMetric(kProjectOneHash, kMetricTwoHash, "value",
                                  kKeyRotationPeriod));
  ExpectNoErrors();
}

// Check that an event has different hashes for different values of the same
// metric.
TEST_F(KeyDataFileDelegateTest, DifferentValuesDifferentHashes) {
  MakeKeyDataFileDelegate();
  EXPECT_NE(key_data_->HmacMetric(kProjectOneHash, kMetricOneHash, "first",
                                  kKeyRotationPeriod),
            key_data_->HmacMetric(kProjectOneHash, kMetricOneHash, "second",
                                  kKeyRotationPeriod));
  ExpectNoErrors();
}

// Ensure that KeyDataFileDelegate::UserId is the expected value of SHA256(key).
TEST_F(KeyDataFileDelegateTest, CheckUserIDs) {
  SetupKey(kProjectOneHash, kKey, Today(), kKeyRotationPeriod);

  MakeKeyDataFileDelegate();
  EXPECT_EQ(HashToHex(key_data_->Id(kProjectOneHash, kKeyRotationPeriod)),
            kUserId);
  EXPECT_NE(HashToHex(key_data_->Id(kProjectTwoHash, kKeyRotationPeriod)),
            kUserId);
  ExpectKeyValidation(/*valid=*/1, /*created=*/1, /*rotated=*/0);
  ExpectNoErrors();
}

// Ensure that KeyDataFileDelegate::Hash returns expected values for a known key
// and value.
TEST_F(KeyDataFileDelegateTest, CheckHashes) {
  SetupKey(kProjectOneHash, kKey, Today(), kKeyRotationPeriod);

  MakeKeyDataFileDelegate();
  EXPECT_EQ(HashToHex(key_data_->HmacMetric(kProjectOneHash, kMetricOneHash,
                                            kValueOne, kKeyRotationPeriod)),
            kValueOneHash);
  EXPECT_EQ(HashToHex(key_data_->HmacMetric(kProjectOneHash, kMetricTwoHash,
                                            kValueTwo, kKeyRotationPeriod)),
            kValueTwoHash);
  ExpectKeyValidation(/*valid=*/2, /*created=*/0, /*rotated=*/0);
  ExpectNoErrors();
}

// Check that keys for a event are correctly rotated after a given rotation
// period.
TEST_F(KeyDataFileDelegateTest, KeysRotated) {
  const base::TimeDelta start_day = Today();
  SetupKey(kProjectOneHash, kKey, start_day, kKeyRotationPeriod);

  MakeKeyDataFileDelegate();
  const uint64_t first_id = key_data_->Id(kProjectOneHash, kKeyRotationPeriod);
  EXPECT_EQ(key_data_->LastKeyRotation(kProjectOneHash)->InDays(),
            start_day.InDays());
  ExpectKeyValidation(/*valid=*/1, /*created=*/0, /*rotated=*/0);

  {
    // Advancing by |kKeyRotationPeriod|-1 days, the key should not be rotated.
    task_environment_.AdvanceClock(kKeyRotationPeriod - base::Days(1));
    EXPECT_EQ(key_data_->Id(kProjectOneHash, kKeyRotationPeriod), first_id);
    EXPECT_EQ(key_data_->LastKeyRotation(kProjectOneHash)->InDays(),
              start_day.InDays());
    SaveKeyData();

    ASSERT_EQ(GetKey(kProjectOneHash).last_rotation(), start_day.InDays());
    ExpectKeyValidation(/*valid=*/2, /*created=*/0, /*rotated=*/0);
  }

  {
    // Advancing by another |key_rotation_period|+1 days, the key should be
    // rotated and the last rotation day should be incremented by
    // |key_rotation_period|.
    task_environment_.AdvanceClock(kKeyRotationPeriod + base::Days(1));
    EXPECT_NE(key_data_->Id(kProjectOneHash, kKeyRotationPeriod), first_id);
    SaveKeyData();

    const base::TimeDelta expected_last_key_rotation =
        start_day + 2 * kKeyRotationPeriod;
    EXPECT_EQ(GetKey(kProjectOneHash).last_rotation(),
              expected_last_key_rotation.InDays());
    EXPECT_EQ(key_data_->LastKeyRotation(kProjectOneHash)->InDays(),
              expected_last_key_rotation.InDays());
    ExpectKeyValidation(/*valid=*/2, /*created=*/0, /*rotated=*/1);

    ASSERT_EQ(GetKey(kProjectOneHash).rotation_period(),
              kKeyRotationPeriod.InDays());
  }

  {
    // Advancing by |2* kKeyRotationPeriod| days, the last rotation day should
    // now 4 periods of |kKeyRotationPeriod| days ahead.
    task_environment_.AdvanceClock(kKeyRotationPeriod * 2);
    key_data_->Id(kProjectOneHash, kKeyRotationPeriod);
    SaveKeyData();

    const base::TimeDelta expected_last_key_rotation =
        start_day + 4 * kKeyRotationPeriod;
    EXPECT_EQ(GetKey(kProjectOneHash).last_rotation(),
              expected_last_key_rotation.InDays());
    EXPECT_EQ(key_data_->LastKeyRotation(kProjectOneHash)->InDays(),
              expected_last_key_rotation.InDays());
    ExpectKeyValidation(/*valid=*/2, /*created=*/0, /*rotated=*/2);
  }
}

// Check that keys with updated rotations are correctly rotated.
TEST_F(KeyDataFileDelegateTest, KeysWithUpdatedRotations) {
  const base::TimeDelta first_key_rotation_period = base::Days(60);
  const base::TimeDelta start_day = Today();

  SetupKey(kProjectOneHash, kKey, start_day, first_key_rotation_period);

  MakeKeyDataFileDelegate();
  const uint64_t first_id =
      key_data_->Id(kProjectOneHash, first_key_rotation_period);
  EXPECT_EQ(key_data_->LastKeyRotation(kProjectOneHash)->InDays(),
            start_day.InDays());
  ExpectKeyValidation(/*valid=*/1, /*created=*/0, /*rotated=*/0);

  // Advance days by |new_key_rotation_period| + 1. This should fall within the
  // rotation of the |new_key_rotation_period| but outside
  // |first_key_rotation_period|.
  const base::TimeDelta new_key_rotation_period = base::Days(50);
  task_environment_.AdvanceClock(new_key_rotation_period + base::Days(1));
  const uint64_t second_id =
      key_data_->Id(kProjectOneHash, new_key_rotation_period);
  EXPECT_NE(first_id, second_id);
  SaveKeyData();

  // Key should have been rotated with new_key_rotation_period.
  const base::TimeDelta expected_last_key_rotation =
      start_day + new_key_rotation_period;
  EXPECT_EQ(GetKey(kProjectOneHash).last_rotation(),
            expected_last_key_rotation.InDays());
  EXPECT_EQ(key_data_->LastKeyRotation(kProjectOneHash)->InDays(),
            expected_last_key_rotation.InDays());
  ExpectKeyValidation(/*valid=*/1, /*created=*/0, /*rotated=*/1);
}

}  // namespace metrics::structured
