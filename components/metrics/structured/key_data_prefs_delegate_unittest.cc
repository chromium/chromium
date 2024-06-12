// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/key_data_prefs_delegate.h"

#include <memory>
#include <string_view>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/lib/histogram_util.h"
#include "components/metrics/structured/lib/key_data.h"
#include "components/metrics/structured/lib/key_util.h"
#include "components/metrics/structured/lib/proto/key.pb.h"
#include "components/metrics/structured/structured_metrics_validator.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::structured {

namespace {
constexpr char kTestPrefName[] = "TestPref";

// 32 byte long test key, matching the size of a real key.
constexpr char kKey[] = "abcdefghijklmnopqrstuvwxyzabcdef";

// These project, event, and metric names are used for testing.
// - project: TestProjectOne
//   - event: TestEventOne
//     - metric: TestMetricOne
//     - metric: TestMetricTwo
// - project: TestProjectTwo

// The name hash of "TestProjectOne".
constexpr uint64_t kProjectOneHash = 16881314472396226433ull;
// The name hash of "TestProjectTwo".
constexpr uint64_t kProjectTwoHash = 5876808001962504629ull;

// The name hash of "TestMetricOne".
constexpr uint64_t kMetricOneHash = 637929385654885975ull;
// The name hash of "TestMetricTwo".
constexpr uint64_t kMetricTwoHash = 14083999144141567134ull;

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

class KeyDataPrefsDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    prefs_.registry()->RegisterDictionaryPref(kTestPrefName);
    // Move the mock date forward from day 0, because KeyDataFileDelegate
    // assumes that day 0 is a bug.
    task_environment_.AdvanceClock(base::Days(1000));
  }

  void CreateKeyData() {
    auto delegate =
        std::make_unique<KeyDataPrefsDelegate>(&prefs_, kTestPrefName);
    delegate_ = delegate.get();
    key_data_ = std::make_unique<KeyData>(std::move(delegate));
  }

  void ResetKeyData() {
    delegate_ = nullptr;
    key_data_.reset();
  }

  // Read the key directly from the prefs.
  KeyProto GetKey(const uint64_t project_name_hash) {
    auto* validators = validator::Validators::Get();

    std::string_view project_name =
        validators->GetProjectName(project_name_hash).value();

    const base::Value::Dict& keys_dict = prefs_.GetDict(kTestPrefName);

    const base::Value::Dict* value = keys_dict.FindDict(project_name);

    std::optional<KeyProto> key = util::CreateKeyProtoFromValue(*value);

    return std::move(key).value();
  }

  base::TimeDelta Today() {
    return base::Time::Now() - base::Time::UnixEpoch();
  }

  // Write a KeyDataProto to prefs with a single key described by the
  // arguments.
  bool SetupKey(const uint64_t project_name_hash,
                const std::string& key,
                const base::TimeDelta last_rotation,
                const base::TimeDelta rotation_period) {
    // It's a test logic error for the key data to exist when calling SetupKey,
    // because it will desync the in-memory proto from the underlying storage.
    if (key_data_) {
      return false;
    }

    KeyProto key_proto;
    key_proto.set_key(key);
    key_proto.set_last_rotation(last_rotation.InDays());
    key_proto.set_rotation_period(rotation_period.InDays());

    ScopedDictPrefUpdate pref_updater(&prefs_, kTestPrefName);

    base::Value::Dict& dict = pref_updater.Get();
    const validator::Validators* validators = validator::Validators::Get();
    auto project_name = validators->GetProjectName(project_name_hash);

    auto value = util::CreateValueFromKeyProto(key_proto);

    dict.Set(*project_name, std::move(value));
    return true;
  }

  void ExpectKeyValidation(const int valid,
                           const int created,
                           const int rotated) {
    static constexpr char kHistogram[] =
        "UMA.StructuredMetrics.KeyValidationState";
    histogram_tester_.ExpectBucketCount(kHistogram, KeyValidationState::kValid,
                                        valid);
    histogram_tester_.ExpectBucketCount(kHistogram,
                                        KeyValidationState::kCreated, created);
    histogram_tester_.ExpectBucketCount(kHistogram,
                                        KeyValidationState::kRotated, rotated);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple prefs_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<KeyData> key_data_;
  raw_ptr<KeyDataPrefsDelegate> delegate_;
};

// If there is no key store file present, check that new keys are generated for
// each project, and those keys are of the right length and different from each
// other.
TEST_F(KeyDataPrefsDelegateTest, GeneratesKeysForProjects) {
  // Make key data and use two keys, in order to generate them.
  CreateKeyData();
  key_data_->Id(kProjectOneHash, kKeyRotationPeriod);
  key_data_->Id(kProjectTwoHash, kKeyRotationPeriod);

  const std::string key_one = GetKey(kProjectOneHash).key();
  const std::string key_two = GetKey(kProjectTwoHash).key();

  EXPECT_EQ(key_one.size(), 32ul);
  EXPECT_EQ(key_two.size(), 32ul);
  EXPECT_NE(key_one, key_two);

  ExpectKeyValidation(/*valid=*/0, /*created=*/2, /*rotated=*/0);
}

// If there is an existing key store file, check that its keys are not replaced.
TEST_F(KeyDataPrefsDelegateTest, ReuseExistingKeys) {
  // Create a file with one key.
  CreateKeyData();
  const uint64_t id_one = key_data_->Id(kProjectOneHash, kKeyRotationPeriod);
  ExpectKeyValidation(/*valid=*/0, /*created=*/1, /*rotated=*/0);
  const std::string key_one = GetKey(kProjectOneHash).key();

  // Reset the in-memory state, leave the on-disk state intact.
  ResetKeyData();

  // Open the file again and check we use the same key.
  CreateKeyData();
  const uint64_t id_two = key_data_->Id(kProjectOneHash, kKeyRotationPeriod);
  ExpectKeyValidation(/*valid=*/1, /*created=*/1, /*rotated=*/0);
  const std::string key_two = GetKey(kProjectOneHash).key();

  EXPECT_EQ(id_one, id_two);
  EXPECT_EQ(key_one, key_two);
}

// Check that different events have different hashes for the same metric and
// value.
TEST_F(KeyDataPrefsDelegateTest, DifferentEventsDifferentHashes) {
  CreateKeyData();
  EXPECT_NE(key_data_->HmacMetric(kProjectOneHash, kMetricOneHash, "value",
                                  kKeyRotationPeriod),
            key_data_->HmacMetric(kProjectTwoHash, kMetricOneHash, "value",
                                  kKeyRotationPeriod));
}

// Check that an event has different hashes for different metrics with the same
// value.
TEST_F(KeyDataPrefsDelegateTest, DifferentMetricsDifferentHashes) {
  CreateKeyData();
  EXPECT_NE(key_data_->HmacMetric(kProjectOneHash, kMetricOneHash, "value",
                                  kKeyRotationPeriod),
            key_data_->HmacMetric(kProjectOneHash, kMetricTwoHash, "value",
                                  kKeyRotationPeriod));
}

// Check that an event has different hashes for different values of the same
// metric.
TEST_F(KeyDataPrefsDelegateTest, DifferentValuesDifferentHashes) {
  CreateKeyData();
  EXPECT_NE(key_data_->HmacMetric(kProjectOneHash, kMetricOneHash, "first",
                                  kKeyRotationPeriod),
            key_data_->HmacMetric(kProjectOneHash, kMetricOneHash, "second",
                                  kKeyRotationPeriod));
}

// Ensure that KeyDataFileDelegate::UserId is the expected value of SHA256(key).
TEST_F(KeyDataPrefsDelegateTest, CheckUserIDs) {
  ASSERT_TRUE(SetupKey(kProjectOneHash, kKey, Today(), kKeyRotationPeriod));

  CreateKeyData();
  EXPECT_EQ(HashToHex(key_data_->Id(kProjectOneHash, kKeyRotationPeriod)),
            kUserId);
  EXPECT_NE(HashToHex(key_data_->Id(kProjectTwoHash, kKeyRotationPeriod)),
            kUserId);
}

// Ensure that KeyDataFileDelegate::Hash returns expected values for a known
// key / and value.
TEST_F(KeyDataPrefsDelegateTest, CheckHashes) {
  ASSERT_TRUE(SetupKey(kProjectOneHash, kKey, Today(), kKeyRotationPeriod));

  CreateKeyData();
  EXPECT_EQ(HashToHex(key_data_->HmacMetric(kProjectOneHash, kMetricOneHash,
                                            kValueOne, kKeyRotationPeriod)),
            kValueOneHash);
  EXPECT_EQ(HashToHex(key_data_->HmacMetric(kProjectOneHash, kMetricTwoHash,
                                            kValueTwo, kKeyRotationPeriod)),
            kValueTwoHash);
}

//// Check that keys for a event are correctly rotated after a given rotation
//// period.
TEST_F(KeyDataPrefsDelegateTest, KeysRotated) {
  const base::TimeDelta start_day = Today();
  ASSERT_TRUE(SetupKey(kProjectOneHash, kKey, start_day, kKeyRotationPeriod));

  CreateKeyData();
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

    ASSERT_EQ(GetKey(kProjectOneHash).last_rotation(), start_day.InDays());
    ExpectKeyValidation(/*valid=*/2, /*created=*/0, /*rotated=*/0);
  }

  {
    // Advancing by another |key_rotation_period|+1 days, the key should be
    // rotated and the last rotation day should be incremented by
    // |key_rotation_period|.
    task_environment_.AdvanceClock(kKeyRotationPeriod + base::Days(1));
    EXPECT_NE(key_data_->Id(kProjectOneHash, kKeyRotationPeriod), first_id);

    base::TimeDelta expected_last_key_rotation =
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

    base::TimeDelta expected_last_key_rotation =
        start_day + 4 * kKeyRotationPeriod;
    EXPECT_EQ(GetKey(kProjectOneHash).last_rotation(),
              expected_last_key_rotation.InDays());
    EXPECT_EQ(key_data_->LastKeyRotation(kProjectOneHash)->InDays(),
              expected_last_key_rotation.InDays());
    ExpectKeyValidation(/*valid=*/2, /*created=*/0, /*rotated=*/2);
  }
}

//// Check that keys with updated rotations are correctly rotated.
TEST_F(KeyDataPrefsDelegateTest, KeysWithUpdatedRotations) {
  base::TimeDelta first_key_rotation_period = base::Days(60);

  const base::TimeDelta start_day = Today();
  ASSERT_TRUE(
      SetupKey(kProjectOneHash, kKey, start_day, first_key_rotation_period));

  CreateKeyData();
  const uint64_t first_id =
      key_data_->Id(kProjectOneHash, first_key_rotation_period);
  EXPECT_EQ(key_data_->LastKeyRotation(kProjectOneHash)->InDays(),
            start_day.InDays());
  ExpectKeyValidation(/*valid=*/1, /*created=*/0, /*rotated=*/0);

  // Advance days by |new_key_rotation_period| + 1. This should fall within
  // the rotation of the |new_key_rotation_period| but outside
  // |first_key_rotation_period|.
  const base::TimeDelta new_key_rotation_period = base::Days(50);
  task_environment_.AdvanceClock(
      base::Days(new_key_rotation_period.InDays() + 1));
  const uint64_t second_id =
      key_data_->Id(kProjectOneHash, new_key_rotation_period);
  EXPECT_NE(first_id, second_id);

  // Key should have been rotated with new_key_rotation_period.
  base::TimeDelta expected_last_key_rotation =
      start_day + new_key_rotation_period;
  EXPECT_EQ(GetKey(kProjectOneHash).last_rotation(),
            expected_last_key_rotation.InDays());
  EXPECT_EQ(key_data_->LastKeyRotation(kProjectOneHash)->InDays(),
            expected_last_key_rotation.InDays());
  ExpectKeyValidation(/*valid=*/1, /*created=*/0, /*rotated=*/1);
}

TEST_F(KeyDataPrefsDelegateTest, Purge) {
  const base::TimeDelta start_day = Today();
  ASSERT_TRUE(SetupKey(kProjectOneHash, kKey, start_day, kKeyRotationPeriod));

  CreateKeyData();
  EXPECT_EQ(delegate_->proto_.keys().size(), 1ul);

  key_data_->Purge();
  EXPECT_EQ(delegate_->proto_.keys().size(), 0ul);

  const base::Value::Dict& keys_dict = prefs_.GetDict(kTestPrefName);
  EXPECT_EQ(keys_dict.size(), 0ul);
}

}  // namespace metrics::structured
