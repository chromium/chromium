// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/raw_eid_generator_impl.h"

#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

const int32_t kNumBytesInEidValue = 2;

// Midnight on 1/1/2020.
const int64_t kPeriodStartTimestampMs = 1577836800000L;

}  //  namespace

class SecureChannelRawEidGeneratorImplTest : public testing::Test {
 protected:
  SecureChannelRawEidGeneratorImplTest() {}
};

TEST_F(SecureChannelRawEidGeneratorImplTest, ProducesTwoByteValue) {
  RawEidGeneratorImpl generator;
  std::string eid =
      generator.GenerateEid("eidSeed", kPeriodStartTimestampMs, nullptr);
  EXPECT_EQ((size_t)kNumBytesInEidValue, eid.length());
}

TEST_F(SecureChannelRawEidGeneratorImplTest, Deterministic) {
  RawEidGeneratorImpl generator;
  std::string eid1 =
      generator.GenerateEid("eidSeed", kPeriodStartTimestampMs, nullptr);
  std::string eid2 =
      generator.GenerateEid("eidSeed", kPeriodStartTimestampMs, nullptr);
  EXPECT_EQ((size_t)kNumBytesInEidValue, eid1.length());
  EXPECT_EQ((size_t)kNumBytesInEidValue, eid2.length());
  EXPECT_EQ(eid1, eid2);
}

TEST_F(SecureChannelRawEidGeneratorImplTest, ChangingSeedChangesOutput) {
  RawEidGeneratorImpl generator;
  std::string eid1 =
      generator.GenerateEid("eidSeed1", kPeriodStartTimestampMs, nullptr);
  std::string eid2 =
      generator.GenerateEid("eidSeed2", kPeriodStartTimestampMs, nullptr);
  EXPECT_EQ((size_t)kNumBytesInEidValue, eid1.length());
  EXPECT_EQ((size_t)kNumBytesInEidValue, eid2.length());
  EXPECT_NE(eid1, eid2);
}

TEST_F(SecureChannelRawEidGeneratorImplTest, ChangingTimestampChangesOutput) {
  RawEidGeneratorImpl generator;
  std::string eid1 =
      generator.GenerateEid("eidSeed", kPeriodStartTimestampMs, nullptr);
  std::string eid2 =
      generator.GenerateEid("eidSeed", kPeriodStartTimestampMs + 1, nullptr);
  EXPECT_EQ((size_t)kNumBytesInEidValue, eid1.length());
  EXPECT_EQ((size_t)kNumBytesInEidValue, eid2.length());
  EXPECT_NE(eid1, eid2);
}

TEST_F(SecureChannelRawEidGeneratorImplTest,
       ChangingExtraEntropyChangesOutput) {
  RawEidGeneratorImpl generator;
  std::string eid1 =
      generator.GenerateEid("eidSeed", kPeriodStartTimestampMs, nullptr);
  std::string extra_entropy = "extraEntropy";
  std::string eid2 =
      generator.GenerateEid("eidSeed", kPeriodStartTimestampMs, &extra_entropy);
  EXPECT_EQ((size_t)kNumBytesInEidValue, eid1.length());
  EXPECT_EQ((size_t)kNumBytesInEidValue, eid2.length());
  EXPECT_NE(eid1, eid2);
}

TEST_F(SecureChannelRawEidGeneratorImplTest,
       ChangingTimestampWithLongExtraEntropy) {
  RawEidGeneratorImpl generator;
  std::string long_extra_entropy =
      "reallyReallyReallyReallyReallyReallyReallyLongExtraEntropy";
  std::string eid1 = generator.GenerateEid("eidSeed", kPeriodStartTimestampMs,
                                           &long_extra_entropy);
  std::string extra_entropy = "extraEntropy";
  std::string eid2 = generator.GenerateEid(
      "eidSeed", kPeriodStartTimestampMs + 1, &long_extra_entropy);
  EXPECT_NE(eid1, eid2);
}

// Tests that "known test vectors" are correct. This ensures that the same test
// vectors produce the same outputs on other platforms.
TEST_F(SecureChannelRawEidGeneratorImplTest, EnsureTestVectorsPass) {
  const int8_t test_eid_seed1[] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                                   12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                                   23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
  const int64_t test_timestamp1 = 2468101214L;
  const int8_t test_entropy1[] = {2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                                  13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                                  24, 25, 26, 27, 28, 29, 30, 31, 32, 33};

  std::string test_eid_seed1_str(reinterpret_cast<const char*>(test_eid_seed1),
                                 sizeof(test_eid_seed1));
  std::string test_entropy1_str(reinterpret_cast<const char*>(test_entropy1),
                                sizeof(test_entropy1));

  const int8_t test_eid_seed2[] = {3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
                                   14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                                   25, 26, 27, 28, 29, 30, 31, 32, 33, 34};
  const int64_t test_timestamp2 = 51015202530L;
  const int8_t test_entropy2[] = {4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
                                  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                                  26, 27, 28, 29, 30, 31, 32, 33, 34, 35};

  std::string test_eid_seed2_str(reinterpret_cast<const char*>(test_eid_seed2),
                                 sizeof(test_eid_seed2));
  std::string test_entropy2_str(reinterpret_cast<const char*>(test_entropy2),
                                sizeof(test_entropy2));

  RawEidGeneratorImpl generator;

  std::string test_eid_without_entropy1 =
      generator.GenerateEid(test_eid_seed1_str, test_timestamp1, nullptr);
  EXPECT_EQ("\x7d\x2c", test_eid_without_entropy1);

  std::string test_eid_with_entropy1 = generator.GenerateEid(
      test_eid_seed1_str, test_timestamp1, &test_entropy1_str);
  EXPECT_EQ("\xdc\xf3", test_eid_with_entropy1);

  std::string test_eid_without_entropy2 =
      generator.GenerateEid(test_eid_seed2_str, test_timestamp2, nullptr);
  EXPECT_EQ("\x02\xdd", test_eid_without_entropy2);

  std::string test_eid_with_entropy2 = generator.GenerateEid(
      test_eid_seed2_str, test_timestamp2, &test_entropy2_str);
  EXPECT_EQ("\xee\xcc", test_eid_with_entropy2);
}

}  // namespace ash::secure_channel
