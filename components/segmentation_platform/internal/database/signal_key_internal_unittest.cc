// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/signal_key_internal.h"

#include <sstream>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {
void VerifyEqual(SignalKeyInternal a, SignalKeyInternal b) {
  ASSERT_EQ(0, memcmp(&a, &b, sizeof(SignalKeyInternal)));
}

void VerifyNotEqual(SignalKeyInternal a, SignalKeyInternal b) {
  ASSERT_NE(0, memcmp(&a, &b, sizeof(SignalKeyInternal)));
}

void VerifyEqual(SignalKeyInternal::Prefix a, SignalKeyInternal::Prefix b) {
  ASSERT_EQ(0, memcmp(&a, &b, sizeof(SignalKeyInternal::Prefix)));
}

void VerifyNotEqual(SignalKeyInternal::Prefix a, SignalKeyInternal::Prefix b) {
  ASSERT_NE(0, memcmp(&a, &b, sizeof(SignalKeyInternal::Prefix)));
}

TEST(SignalKeyInternalTest, TestKeyConversionToAndFromBinary) {
  SignalKeyInternal key;
  key.prefix.kind = 'u';
  key.prefix.name_hash = 42;
  key.time_range_end_sec = 1577836800000;
  key.time_range_start_sec = 1609459200000;

  std::string binary = SignalKeyInternalToBinary(key);
  SignalKeyInternal result;
  EXPECT_TRUE(SignalKeyInternalFromBinary(binary, &result));
  VerifyEqual(key, result);
}

TEST(SignalKeyInternalTest, TestKeyConversionFailureFromBinary) {
  SignalKeyInternal key;
  key.prefix.kind = 'u';
  key.prefix.name_hash = 42;
  key.time_range_end_sec = 1577836800000;
  key.time_range_start_sec = 1609459200000;
  const std::string binary = SignalKeyInternalToBinary(key);

  {
    const std::string shorter = binary.substr(0, binary.size() - 1);
    SignalKeyInternal result;
    EXPECT_FALSE(SignalKeyInternalFromBinary(shorter, &result));
    VerifyEqual(SignalKeyInternal{}, result);
  }

  {
    std::string longer = binary;
    longer.append("x");
    SignalKeyInternal result;
    EXPECT_FALSE(SignalKeyInternalFromBinary(longer, &result));
    VerifyEqual(SignalKeyInternal{}, result);
  }

  {
    const std::string empty;
    SignalKeyInternal result;
    EXPECT_FALSE(SignalKeyInternalFromBinary(empty, &result));
    VerifyEqual(SignalKeyInternal{}, result);
  }
}

TEST(SignalKeyInternalTest, TestPrefixConversionToAndFromBinary) {
  SignalKeyInternal::Prefix prefix;
  prefix.kind = 'u';
  prefix.name_hash = 42;

  std::string binary = SignalKeyInternalPrefixToBinary(prefix);
  SignalKeyInternal::Prefix result;
  EXPECT_TRUE(SignalKeyInternalPrefixFromBinary(binary, &result));
  VerifyEqual(prefix, result);
}

TEST(SignalKeyInternalTest, TestPrefixConversionFailureFromBinary) {
  SignalKeyInternal::Prefix prefix;
  prefix.kind = 'u';
  prefix.name_hash = 42;
  std::string binary = SignalKeyInternalPrefixToBinary(prefix);

  {
    std::string shorter = binary.substr(0, binary.size() - 1);
    SignalKeyInternal::Prefix result;
    EXPECT_FALSE(SignalKeyInternalPrefixFromBinary(shorter, &result));
    VerifyEqual(SignalKeyInternal::Prefix{}, result);
  }

  {
    std::string longer = binary;
    longer.append("x");
    SignalKeyInternal::Prefix result;
    EXPECT_FALSE(SignalKeyInternalPrefixFromBinary(longer, &result));
    VerifyEqual(SignalKeyInternal::Prefix{}, result);
  }

  {
    const std::string empty;
    SignalKeyInternal::Prefix result;
    EXPECT_FALSE(SignalKeyInternalPrefixFromBinary(empty, &result));
    VerifyEqual(SignalKeyInternal::Prefix{}, result);
  }
}

TEST(SignalKeyInternalTest, TestChangingAnyKeyFieldMakesNotEqual) {
  SignalKeyInternal original;
  original.prefix.kind = 'u';
  original.prefix.name_hash = 42;
  original.time_range_end_sec = 1577836800000;
  original.time_range_start_sec = 1609459200000;

  SignalKeyInternal copy = original;
  SignalKeyInternal result;
  EXPECT_TRUE(
      SignalKeyInternalFromBinary(SignalKeyInternalToBinary(copy), &result));
  VerifyEqual(original, result);

  SignalKeyInternal different_kind = original;
  different_kind.prefix.kind = 'r';
  EXPECT_TRUE(SignalKeyInternalFromBinary(
      SignalKeyInternalToBinary(different_kind), &result));
  VerifyNotEqual(original, result);

  SignalKeyInternal different_name_hash = original;
  different_name_hash.prefix.name_hash = 84;
  EXPECT_TRUE(SignalKeyInternalFromBinary(
      SignalKeyInternalToBinary(different_name_hash), &result));
  VerifyNotEqual(original, result);

  SignalKeyInternal different_time_range_end_sec = original;
  different_time_range_end_sec.time_range_end_sec = 1546300800000;
  EXPECT_TRUE(SignalKeyInternalFromBinary(
      SignalKeyInternalToBinary(different_time_range_end_sec), &result));
  VerifyNotEqual(original, result);

  SignalKeyInternal different_time_range_start_sec = original;
  different_time_range_start_sec.time_range_start_sec = 1546300800000;
  EXPECT_TRUE(SignalKeyInternalFromBinary(
      SignalKeyInternalToBinary(different_time_range_start_sec), &result));
  VerifyNotEqual(original, result);
}

TEST(SignalKeyInternalTest, TestChangingAnyPrefixFieldMakesNotEqual) {
  SignalKeyInternal::Prefix original;
  original.kind = 'u';
  original.name_hash = 42;

  SignalKeyInternal::Prefix copy = original;
  SignalKeyInternal::Prefix result;
  EXPECT_TRUE(SignalKeyInternalPrefixFromBinary(
      SignalKeyInternalPrefixToBinary(copy), &result));
  VerifyEqual(original, result);

  SignalKeyInternal::Prefix different_kind = original;
  different_kind.kind = 'r';
  EXPECT_TRUE(SignalKeyInternalPrefixFromBinary(
      SignalKeyInternalPrefixToBinary(different_kind), &result));
  VerifyNotEqual(original, result);

  SignalKeyInternal::Prefix different_name_hash = original;
  different_name_hash.name_hash = 84;
  EXPECT_TRUE(SignalKeyInternalPrefixFromBinary(
      SignalKeyInternalPrefixToBinary(different_name_hash), &result));
  VerifyNotEqual(original, result);
}

TEST(SignalKeyInternalTest, TestKeyDebugStringRepresentation) {
  SignalKeyInternal key1;
  key1.prefix.kind = 'u';
  key1.prefix.name_hash = 1;
  key1.time_range_end_sec = 3;
  key1.time_range_start_sec = 2;

  EXPECT_EQ("{{u:1}:3:2}", SignalKeyInternalToDebugString(key1));
  std::stringstream key1_buffer;
  key1_buffer << key1;
  EXPECT_EQ("{{u:1}:3:2}", key1_buffer.str());

  SignalKeyInternal key2;
  key2.prefix.kind = 'u';
  key2.prefix.name_hash = 1;
  key2.time_range_end_sec = -2;
  key2.time_range_start_sec = -3;
  EXPECT_EQ("{{u:1}:-2:-3}", SignalKeyInternalToDebugString(key2));
  std::stringstream key2_buffer;
  key2_buffer << key2;
  EXPECT_EQ("{{u:1}:-2:-3}", key2_buffer.str());
}

TEST(SignalKeyInternalTest, TestPrefixDebugStringRepresentation) {
  SignalKeyInternal::Prefix prefix;
  prefix.kind = 'u';
  prefix.name_hash = 1;
  EXPECT_EQ("{u:1}", SignalKeyInternalPrefixToDebugString(prefix));
  std::stringstream prefix_buffer;
  prefix_buffer << prefix;
  EXPECT_EQ("{u:1}", prefix_buffer.str());
}

TEST(SignalKeyInternalTest, TestBinaryKeyLexicographicalComparison) {
  // The members prefix.name_hash, time_range_end_sec, and time_range_start_sec
  // are not lexicographically comparable using their in-memory representation
  // on little endian systems. Verify that the resulting binary key still is
  // lexicographically comparable since big endian should be used.
  // It is important to use multiple bytes when creating test scenarios, since
  // both little endian and big endian store each individual byte the same way.
  SignalKeyInternal original;
  original.prefix.kind = 'u';
  original.prefix.name_hash = 1 << 8;
  original.time_range_end_sec = 1 << 24;
  original.time_range_start_sec = 1 << 8;
  std::string original_binary = SignalKeyInternalToBinary(original);

  SignalKeyInternal other{{'u', {}, 1 << 8}, 1 << 24, 1 << 8};
  std::string other_binary = SignalKeyInternalToBinary(other);
  EXPECT_EQ(original_binary, other_binary);

  SignalKeyInternal kind_smaller{{'a', {}, 1 << 8}, 1 << 24, 1 << 8};
  std::string kind_smaller_binary = SignalKeyInternalToBinary(kind_smaller);
  EXPECT_GT(original_binary, kind_smaller_binary);

  SignalKeyInternal kind_larger{{'z', {}, 1 << 8}, 1 << 24, 1 << 8};
  std::string kind_larger_binary = SignalKeyInternalToBinary(kind_larger);
  EXPECT_LT(original_binary, kind_larger_binary);

  SignalKeyInternal name_hash_smaller{{'u', {}, 1}, 1 << 24, 1 << 8};
  std::string name_hash_smaller_binary =
      SignalKeyInternalToBinary(name_hash_smaller);
  EXPECT_GT(original_binary, name_hash_smaller_binary);

  SignalKeyInternal name_hash_larger{{'u', {}, 1 << 16}, 1 << 24, 1 << 8};
  std::string name_hash_larger_binary =
      SignalKeyInternalToBinary(name_hash_larger);
  EXPECT_LT(original_binary, name_hash_larger_binary);

  SignalKeyInternal range_end_smaller{{'u', {}, 1 << 8}, 1 << 16, 1 << 8};
  std::string range_end_smaller_binary =
      SignalKeyInternalToBinary(range_end_smaller);
  EXPECT_GT(original_binary, range_end_smaller_binary);

  SignalKeyInternal range_end_larger{{'u', {}, 1 << 8}, 1LL << 32, 1 << 8};
  std::string range_end_larger_binary =
      SignalKeyInternalToBinary(range_end_larger);
  EXPECT_LT(original_binary, range_end_larger_binary);

  SignalKeyInternal range_start_smaller{{'u', {}, 1 << 8}, 1 << 24, 1};
  std::string range_start_smaller_binary =
      SignalKeyInternalToBinary(range_start_smaller);
  EXPECT_GT(original_binary, range_start_smaller_binary);

  SignalKeyInternal range_start_larger{{'u', {}, 1 << 8}, 1 << 24, 1 << 16};
  std::string range_start_larger_binary =
      SignalKeyInternalToBinary(range_start_larger);
  EXPECT_LT(original_binary, range_start_larger_binary);
}

TEST(SignalKeyInternalTest, TestBinaryKeyFieldOrder) {
  // This test changes one field at a time, and ensures that all fields expected
  // to be later in the binary representation are set to values that would fail
  // the comparison if they were in a different order.
  SignalKeyInternal original;
  original.prefix.kind = 'u';
  original.prefix.name_hash = 1 << 8;
  original.time_range_end_sec = 1 << 8;
  original.time_range_start_sec = 1 << 8;
  std::string original_binary = SignalKeyInternalToBinary(original);

  // First value should be prefix.kind.
  SignalKeyInternal kind_smaller{{'a', {}, 1 << 16}, 1 << 16, 1 << 16};
  std::string kind_smaller_binary = SignalKeyInternalToBinary(kind_smaller);
  EXPECT_GT(original_binary, kind_smaller_binary);

  SignalKeyInternal kind_larger{{'z', {}, 1}, 1, 1};
  std::string kind_larger_binary = SignalKeyInternalToBinary(kind_larger);
  EXPECT_LT(original_binary, kind_larger_binary);

  // Second value should be prefix.name_hash.
  SignalKeyInternal name_hash_smaller{{'u', {}, 1}, 1 << 16, 1 << 16};
  std::string name_hash_smaller_binary =
      SignalKeyInternalToBinary(name_hash_smaller);
  EXPECT_GT(original_binary, name_hash_smaller_binary);

  SignalKeyInternal name_hash_larger{{'u', {}, 1 << 16}, 1, 1};
  std::string name_hash_larger_binary =
      SignalKeyInternalToBinary(name_hash_larger);
  EXPECT_LT(original_binary, name_hash_larger_binary);

  // Third value should be time_range_end_sec.
  SignalKeyInternal time_range_end_sec_smaller{{'u', {}, 1 << 8}, 1, 1 << 16};
  std::string time_range_end_sec_smaller_binary =
      SignalKeyInternalToBinary(time_range_end_sec_smaller);
  EXPECT_GT(original_binary, time_range_end_sec_smaller_binary);

  SignalKeyInternal time_range_end_sec_larger{{'u', {}, 1 << 8}, 1 << 16, 1};
  std::string time_range_end_sec_larger_binary =
      SignalKeyInternalToBinary(time_range_end_sec_larger);
  EXPECT_LT(original_binary, time_range_end_sec_larger_binary);

  // Fourth value should be time_range_start_sec.
  SignalKeyInternal time_range_start_sec_smaller{{'u', {}, 1 << 8}, 1 << 8, 1};
  std::string time_range_start_sec_smaller_binary =
      SignalKeyInternalToBinary(time_range_start_sec_smaller);
  EXPECT_GT(original_binary, time_range_start_sec_smaller_binary);

  SignalKeyInternal time_range_start_sec_larger{
      {'u', {}, 1 << 8}, 1 << 8, 1 << 16};
  std::string time_range_start_sec_larger_binary =
      SignalKeyInternalToBinary(time_range_start_sec_larger);
  EXPECT_LT(original_binary, time_range_start_sec_larger_binary);
}

TEST(SignalKeyInternalTest, TestBinaryPrefixLexicographicalComparison) {
  // The members prefix.name_hash is not lexicographically comparable using
  // their in-memory representation on little endian systems. Verify that the
  // resulting binary prefix still is lexicographically comparable since big
  // endian should be used.
  // It is important to use multiple bytes when creating test scenarios, since
  // both little endian and big endian store each individual byte the same way.
  SignalKeyInternal::Prefix original;
  original.kind = 'u';
  original.name_hash = 1 << 8;
  std::string original_binary = SignalKeyInternalPrefixToBinary(original);

  SignalKeyInternal::Prefix other{'u', {}, 1 << 8};
  std::string other_binary = SignalKeyInternalPrefixToBinary(other);
  EXPECT_EQ(original_binary, other_binary);

  SignalKeyInternal::Prefix kind_smaller{'a', {}, 1 << 8};
  std::string kind_smaller_binary =
      SignalKeyInternalPrefixToBinary(kind_smaller);
  EXPECT_GT(original_binary, kind_smaller_binary);

  SignalKeyInternal::Prefix kind_larger{'z', {}, 1 << 8};
  std::string kind_larger_binary = SignalKeyInternalPrefixToBinary(kind_larger);
  EXPECT_LT(original_binary, kind_larger_binary);

  SignalKeyInternal::Prefix name_hash_smaller{'u', {}, 1};
  std::string name_hash_smaller_binary =
      SignalKeyInternalPrefixToBinary(name_hash_smaller);
  EXPECT_GT(original_binary, name_hash_smaller_binary);

  SignalKeyInternal::Prefix name_hash_larger{'u', {}, 1 << 16};
  std::string name_hash_larger_binary =
      SignalKeyInternalPrefixToBinary(name_hash_larger);
  EXPECT_LT(original_binary, name_hash_larger_binary);
}

TEST(SignalKeyInternalTest, TestBinaryPrefixFieldOrder) {
  // This test changes one field at a time, and ensures that all fields expected
  // to be later in the binary representation are set to values that would fail
  // the comparison if they were in a different order.
  SignalKeyInternal::Prefix original;
  original.kind = 'u';
  original.name_hash = 1 << 8;
  std::string original_binary = SignalKeyInternalPrefixToBinary(original);

  // First value should be kind.
  SignalKeyInternal::Prefix kind_smaller{'a', {}, 1 << 16};
  std::string kind_smaller_binary =
      SignalKeyInternalPrefixToBinary(kind_smaller);
  EXPECT_GT(original_binary, kind_smaller_binary);

  SignalKeyInternal::Prefix kind_larger{'z', {}, 1};
  std::string kind_larger_binary = SignalKeyInternalPrefixToBinary(kind_larger);
  EXPECT_LT(original_binary, kind_larger_binary);

  // Second value should be name_hash.
  SignalKeyInternal::Prefix name_hash_smaller{'u', {}, 1};
  std::string name_hash_smaller_binary =
      SignalKeyInternalPrefixToBinary(name_hash_smaller);
  EXPECT_GT(original_binary, name_hash_smaller_binary);

  SignalKeyInternal::Prefix name_hash_larger{'u', {}, 1 << 16};
  std::string name_hash_larger_binary =
      SignalKeyInternalPrefixToBinary(name_hash_larger);
  EXPECT_LT(original_binary, name_hash_larger_binary);
}

}  // namespace

}  // namespace segmentation_platform
