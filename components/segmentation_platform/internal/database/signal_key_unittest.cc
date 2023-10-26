// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/signal_key.h"

#include <cmath>
#include <cstring>

#include "base/logging.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/signal_key_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {
int CompareBinaryKeys(const SignalKey& a, const SignalKey& b) {
  std::string a_key = a.ToBinary();
  std::string b_key = b.ToBinary();
  CHECK_EQ(a_key.size(), b_key.size());
  return std::memcmp(a_key.data(), b_key.data(), a_key.size());
}

bool Equal(const SignalKey& k1, const SignalKey& k2) {
  // Log comparison debugging info for failing tests.
  VLOG(0) << k1 << " ==? " << k2;
  return k1.kind() == k2.kind() && k1.name_hash() == k2.name_hash() &&
         k1.range_start() == k2.range_start() &&
         k1.range_end() == k2.range_end() && CompareBinaryKeys(k1, k2) == 0;
}
}  // namespace

class SignalKeyTest : public testing::Test {
 public:
  SignalKeyTest() = default;
  ~SignalKeyTest() override = default;

  void VerifyConversion(const SignalKey& key) {
    std::string binary_key = key.ToBinary();
    SignalKey result;
    EXPECT_TRUE(SignalKey::FromBinary(binary_key, &result));
    EXPECT_TRUE(Equal(key, result));
  }

 protected:
  void SetUp() override {
    test_clock_.SetNow(base::Time::UnixEpoch() + base::Hours(8));
  }

  base::SimpleTestClock test_clock_;
};

TEST_F(SignalKeyTest, TestConvertToAndFromBinary) {
  VerifyConversion(SignalKey(SignalKey::Kind::USER_ACTION, 1, test_clock_.Now(),
                             test_clock_.Now() + base::Seconds(10)));
  VerifyConversion(SignalKey(SignalKey::Kind::HISTOGRAM_VALUE, 2,
                             base::Time::Now(),
                             test_clock_.Now() + base::Seconds(20)));
  VerifyConversion(SignalKey(SignalKey::Kind::HISTOGRAM_ENUM, 3,
                             base::Time::Now(),
                             test_clock_.Now() + base::Seconds(30)));
}

TEST_F(SignalKeyTest, TestValidity) {
  SignalKey valid_key(SignalKey::Kind::USER_ACTION, 42, test_clock_.Now(),
                      test_clock_.Now() + base::Seconds(10));
  EXPECT_TRUE(valid_key.IsValid());

  // A default constructed key should not be valid.
  SignalKey default_constructed_key;
  EXPECT_FALSE(default_constructed_key.IsValid());

  // Verify that each individual field is tested for validity.
  SignalKey invalid_key1(SignalKey::Kind::UNKNOWN, 42, test_clock_.Now(),
                         test_clock_.Now());
  EXPECT_FALSE(invalid_key1.IsValid());

  SignalKey invalid_key2(SignalKey::Kind::USER_ACTION, 0, test_clock_.Now(),
                         test_clock_.Now());
  EXPECT_FALSE(invalid_key2.IsValid());

  SignalKey invalid_key3(SignalKey::Kind::USER_ACTION, 42, base::Time(),
                         test_clock_.Now());
  EXPECT_FALSE(invalid_key3.IsValid());

  SignalKey invalid_key4(SignalKey::Kind::USER_ACTION, 42, test_clock_.Now(),
                         base::Time());
  EXPECT_FALSE(invalid_key4.IsValid());
}

TEST_F(SignalKeyTest, TestUsesSafeBinaryFormat) {
  // By testing that the underlying format is the binary version of
  // SignalKeyInternal, we can ensure API guarantees based on SignalKeyInternal.
  SignalKey key(SignalKey::Kind::USER_ACTION, 42, test_clock_.Now(),
                test_clock_.Now() + base::Seconds(10));

  std::string binary_key = key.ToBinary();
  SignalKeyInternal internal_key;
  EXPECT_TRUE(SignalKeyInternalFromBinary(binary_key, &internal_key));
  EXPECT_EQ('u', internal_key.prefix.kind);
  EXPECT_EQ(42UL, internal_key.prefix.name_hash);
  EXPECT_EQ(11644502400, internal_key.time_range_start_sec);
  EXPECT_EQ(11644502410, internal_key.time_range_end_sec);
}

TEST_F(SignalKeyTest, TestGetPrefixInBinary) {
  SignalKey key(SignalKey::Kind::USER_ACTION, 42, test_clock_.Now(),
                test_clock_.Now() + base::Seconds(10));

  std::string binary_prefix = key.GetPrefixInBinary();
  SignalKeyInternal::Prefix prefix;
  EXPECT_TRUE(SignalKeyInternalPrefixFromBinary(binary_prefix, &prefix));
  EXPECT_EQ('u', prefix.kind);
  EXPECT_EQ(42UL, prefix.name_hash);
}

TEST_F(SignalKeyTest, EarliestEndTimeComesFirst) {
  SignalKey early(SignalKey::Kind::USER_ACTION, 42, test_clock_.Now(),
                  test_clock_.Now());

  SignalKey late(SignalKey::Kind::USER_ACTION, 42, test_clock_.Now(),
                 test_clock_.Now() + base::Seconds(20));

  EXPECT_LT(CompareBinaryKeys(early, late), 0);
}

TEST_F(SignalKeyTest, EqualKeysHaveEqualBinaryKeys) {
  SignalKey a(SignalKey::Kind::USER_ACTION, 42, test_clock_.Now(),
              test_clock_.Now());
  SignalKey b(SignalKey::Kind::USER_ACTION, 42, test_clock_.Now(),
              test_clock_.Now());

  EXPECT_EQ(0, CompareBinaryKeys(a, b));
}

TEST_F(SignalKeyTest, EndTimeMoreSignificantThanStartTime) {
  SignalKey early_end(SignalKey::Kind::USER_ACTION, 42,
                      test_clock_.Now() + base::Seconds(20),
                      test_clock_.Now() + base::Seconds(20));
  SignalKey early_start(SignalKey::Kind::USER_ACTION, 42,
                        test_clock_.Now() + base::Seconds(10),
                        test_clock_.Now() + base::Seconds(30));

  EXPECT_LT(CompareBinaryKeys(early_end, early_start), 0);
}

TEST_F(SignalKeyTest, OrderByStartTimeIfEverythingElseIsEqual) {
  SignalKey early_start(SignalKey::Kind::USER_ACTION, 42,
                        test_clock_.Now() + base::Seconds(10),
                        test_clock_.Now());
  SignalKey late_start(SignalKey::Kind::USER_ACTION, 42,
                       test_clock_.Now() + base::Seconds(20),
                       test_clock_.Now());

  EXPECT_LT(CompareBinaryKeys(early_start, late_start), 0);
}

TEST_F(SignalKeyTest, DifferentNameHashGivesDifferentKey) {
  SignalKey a(SignalKey::Kind::USER_ACTION, 42, test_clock_.Now(),
              test_clock_.Now());
  SignalKey b(SignalKey::Kind::USER_ACTION, 84, test_clock_.Now(),
              test_clock_.Now());

  EXPECT_NE(0, CompareBinaryKeys(a, b));
}

TEST_F(SignalKeyTest, DifferentKindGivesDifferentKey) {
  SignalKey a(SignalKey::Kind::USER_ACTION, 42, test_clock_.Now(),
              test_clock_.Now());
  SignalKey b(SignalKey::Kind::HISTOGRAM_VALUE, 42, test_clock_.Now(),
              test_clock_.Now());

  EXPECT_NE(0, CompareBinaryKeys(a, b));
}

TEST_F(SignalKeyTest, TestKeyDebugStringRepresentation) {
  SignalKey key(SignalKey::Kind::USER_ACTION, 42, test_clock_.Now(),
                test_clock_.Now() + base::Seconds(10));

  EXPECT_EQ(
      "{kind=1, name_hash=42, range_start=1970-01-01 08:00:00.000000 UTC, "
      "range_end=1970-01-01 08:00:10.000000 UTC}",
      key.ToDebugString());
  std::stringstream key_buffer;
  key_buffer << key;
  EXPECT_EQ(
      "{kind=1, name_hash=42, range_start=1970-01-01 08:00:00.000000 UTC, "
      "range_end=1970-01-01 08:00:10.000000 UTC}",
      key_buffer.str());
}

}  // namespace segmentation_platform
