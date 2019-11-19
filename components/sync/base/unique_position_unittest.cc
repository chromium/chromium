// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/unique_position.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

// This function exploits internal knowledge of how the protobufs are serialized
// to help us build UniquePositions from strings described in this file.
static UniquePosition FromBytes(const std::string& bytes) {
  sync_pb::UniquePosition proto;
  proto.set_value(bytes);
  return UniquePosition::FromProto(proto);
}

class UniquePositionTest : public ::testing::Test {
 protected:
  // Accessor to fetch the length of the position's internal representation
  // We try to avoid having any test expectations on it because this is an
  // implementation detail.
  //
  // If you run the tests with --v=1, we'll print out some of the lengths
  // so you can see how well the algorithm performs in various insertion
  // scenarios.
  size_t GetLength(const UniquePosition& pos) {
    return pos.ToProto().ByteSize();
  }

const size_t kMinLength = UniquePosition::kSuffixLength;
const size_t kGenericPredecessorLength = kMinLength + 2;
const size_t kGenericSuccessorLength = kMinLength + 1;
const size_t kBigPositionLength = kMinLength;
const size_t kSmallPositionLength = kMinLength;

// Be careful when adding more prefixes to this list.
// We have to manually ensure each has a unique suffix.
const UniquePosition kGenericPredecessor =
    FromBytes((std::string(kGenericPredecessorLength, '\x23') + '\xFF'));
const UniquePosition kGenericSuccessor =
    FromBytes(std::string(kGenericSuccessorLength, '\xAB') + '\xFF');
const UniquePosition kBigPosition =
    FromBytes(std::string(kBigPositionLength - 1, '\xFF') + '\xFE' + '\xFF');
const UniquePosition kBigPositionLessTwo =
    FromBytes(std::string(kBigPositionLength - 1, '\xFF') + '\xFC' + '\xFF');
const UniquePosition kBiggerPosition =
    FromBytes(std::string(kBigPositionLength, '\xFF') + '\xFF');
const UniquePosition kSmallPosition =
    FromBytes(std::string(kSmallPositionLength - 1, '\x00') + '\x01' + '\xFF');
const UniquePosition kSmallPositionPlusOne =
    FromBytes(std::string(kSmallPositionLength - 1, '\x00') + '\x02' + '\xFF');
const UniquePosition kHugePosition = FromBytes(
    std::string(UniquePosition::kCompressBytesThreshold, '\xFF') + '\xAB');

const UniquePosition kPositionArray[7] = {
    kGenericPredecessor,   kGenericSuccessor, kBigPosition,
    kBigPositionLessTwo,   kBiggerPosition,   kSmallPosition,
    kSmallPositionPlusOne,
};

const UniquePosition kSortedPositionArray[7] = {
    kSmallPosition,    kSmallPositionPlusOne, kGenericPredecessor,
    kGenericSuccessor, kBigPositionLessTwo,   kBigPosition,
    kBiggerPosition,
};

const size_t kNumPositions = base::size(kPositionArray);
const size_t kNumSortedPositions = base::size(kSortedPositionArray);
};

static constexpr char kMinSuffix[] = {
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x01'};
static_assert(base::size(kMinSuffix) == UniquePosition::kSuffixLength,
              "Wrong size of kMinSuffix.");

static constexpr char kMaxSuffix[] = {
    '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF',
    '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF',
    '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF',
    '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF'};
static_assert(base::size(kMaxSuffix) == UniquePosition::kSuffixLength,
              "Wrong size of kMaxSuffix.");

static constexpr char kNormalSuffix[] = {
    '\x68', '\x44', '\x6C', '\x6B', '\x32', '\x58', '\x78',
    '\x34', '\x69', '\x70', '\x46', '\x34', '\x79', '\x49',
    '\x44', '\x4F', '\x66', '\x4C', '\x58', '\x41', '\x31',
    '\x34', '\x68', '\x59', '\x56', '\x43', '\x6F', '\x3D'};
static_assert(base::size(kNormalSuffix) == UniquePosition::kSuffixLength,
              "Wrong size of kNormalSuffix.");

::testing::AssertionResult LessThan(const char* m_expr,
                                    const char* n_expr,
                                    const UniquePosition& m,
                                    const UniquePosition& n) {
  if (m.LessThan(n))
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << m_expr << " is not less than "
                                       << n_expr << " (" << m.ToDebugString()
                                       << " and " << n.ToDebugString() << ")";
}

::testing::AssertionResult Equals(const char* m_expr,
                                  const char* n_expr,
                                  const UniquePosition& m,
                                  const UniquePosition& n) {
  if (m.Equals(n))
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << m_expr << " is not equal to "
                                       << n_expr << " (" << m.ToDebugString()
                                       << " != " << n.ToDebugString() << ")";
}

// Test that the code can read the uncompressed serialization format.
TEST_F(UniquePositionTest, DeserializeObsoleteUncompressedPosition) {
  // We no longer support the encoding data in this format.  This hard-coded
  // input is a serialization of kGenericPredecessor created by an older version
  // of this code.
  const char kSerializedCstr[] = {
      '\x0a', '\x1f', '\x23', '\x23', '\x23', '\x23', '\x23', '\x23', '\x23',
      '\x23', '\x23', '\x23', '\x23', '\x23', '\x23', '\x23', '\x23', '\x23',
      '\x23', '\x23', '\x23', '\x23', '\x23', '\x23', '\x23', '\x23', '\x23',
      '\x23', '\x23', '\x23', '\x23', '\x23', '\xff'};
  const std::string serialized(kSerializedCstr, sizeof(kSerializedCstr));

  sync_pb::UniquePosition proto;
  proto.ParseFromString(serialized);

  // Double-check that this test is testing what we think it tests.
  EXPECT_TRUE(proto.has_value());
  EXPECT_FALSE(proto.has_compressed_value());
  EXPECT_FALSE(proto.has_uncompressed_length());

  UniquePosition pos = UniquePosition::FromProto(proto);
  EXPECT_PRED_FORMAT2(Equals, kGenericPredecessor, pos);
}

// Test that the code can read the gzip serialization format.
TEST_F(UniquePositionTest, DeserializeObsoleteGzippedPosition) {
  // We no longer support the encoding data in this format.  This hard-coded
  // input is a serialization of kHugePosition created by an older version of
  // this code.
  const char kSerializedCstr[] = {
      '\x12', '\x0d', '\x78', '\x9c', '\xfb', '\xff', '\x7f', '\x60', '\xc1',
      '\x6a', '\x00', '\xa2', '\x4c', '\x80', '\x2c', '\x18', '\x81', '\x01'};
  const std::string serialized(kSerializedCstr, sizeof(kSerializedCstr));

  sync_pb::UniquePosition proto;
  proto.ParseFromString(serialized);

  // Double-check that this test is testing what we think it tests.
  EXPECT_FALSE(proto.has_value());
  EXPECT_TRUE(proto.has_compressed_value());
  EXPECT_TRUE(proto.has_uncompressed_length());

  UniquePosition pos = UniquePosition::FromProto(proto);
  EXPECT_PRED_FORMAT2(Equals, kHugePosition, pos);
}

class RelativePositioningTest : public UniquePositionTest {};

struct PositionLessThan {
  bool operator()(const UniquePosition& a, const UniquePosition& b) {
    return a.LessThan(b);
  }
};

// Returns true iff the given position's suffix matches the input parameter.
static bool IsSuffixInUse(const UniquePosition& pos,
                          const std::string& suffix) {
  return pos.GetSuffixForTest() == suffix;
}

// Test some basic properties of comparison and equality.
TEST_F(RelativePositioningTest, ComparisonSanityTest1) {
  const UniquePosition& a = kPositionArray[0];
  ASSERT_TRUE(a.IsValid());

  // Necessarily true for any non-invalid positions.
  EXPECT_TRUE(a.Equals(a));
  EXPECT_FALSE(a.LessThan(a));
}

// Test some more properties of comparison and equality.
TEST_F(RelativePositioningTest, ComparisonSanityTest2) {
  const UniquePosition& a = kPositionArray[0];
  const UniquePosition& b = kPositionArray[1];

  // These should pass for the specific a and b we have chosen (a < b).
  EXPECT_FALSE(a.Equals(b));
  EXPECT_TRUE(a.LessThan(b));
  EXPECT_FALSE(b.LessThan(a));
}

// Exercise comparision functions by sorting and re-sorting the list.
TEST_F(RelativePositioningTest, SortPositions) {
  ASSERT_EQ(kNumPositions, kNumSortedPositions);
  UniquePosition positions[base::size(kPositionArray)];
  for (size_t i = 0; i < kNumPositions; ++i) {
    positions[i] = kPositionArray[i];
  }

  std::sort(&positions[0], &positions[kNumPositions], PositionLessThan());
  for (size_t i = 0; i < kNumPositions; ++i) {
    EXPECT_TRUE(positions[i].Equals(kSortedPositionArray[i]))
        << "i: " << i << ", " << positions[i].ToDebugString()
        << " != " << kSortedPositionArray[i].ToDebugString();
  }
}

// Some more exercise for the comparison function.
TEST_F(RelativePositioningTest, ReverseSortPositions) {
  ASSERT_EQ(kNumPositions, kNumSortedPositions);
  UniquePosition positions[base::size(kPositionArray)];
  for (size_t i = 0; i < kNumPositions; ++i) {
    positions[i] = kPositionArray[i];
  }

  std::reverse(&positions[0], &positions[kNumPositions]);
  std::sort(&positions[0], &positions[kNumPositions], PositionLessThan());
  for (size_t i = 0; i < kNumPositions; ++i) {
    EXPECT_TRUE(positions[i].Equals(kSortedPositionArray[i]))
        << "i: " << i << ", " << positions[i].ToDebugString()
        << " != " << kSortedPositionArray[i].ToDebugString();
  }
}

class PositionInsertTest : public RelativePositioningTest,
                           public ::testing::WithParamInterface<std::string> {};

// Exercise InsertBetween with various insertion operations.
TEST_P(PositionInsertTest, InsertBetween) {
  const std::string suffix = GetParam();
  ASSERT_TRUE(UniquePosition::IsValidSuffix(suffix));

  for (size_t i = 0; i < kNumSortedPositions; ++i) {
    const UniquePosition& predecessor = kSortedPositionArray[i];
    // Verify our suffixes are unique before we continue.
    if (IsSuffixInUse(predecessor, suffix))
      continue;

    for (size_t j = i + 1; j < kNumSortedPositions; ++j) {
      const UniquePosition& successor = kSortedPositionArray[j];

      // Another guard against non-unique suffixes.
      if (IsSuffixInUse(successor, suffix))
        continue;

      UniquePosition midpoint =
          UniquePosition::Between(predecessor, successor, suffix);

      EXPECT_PRED_FORMAT2(LessThan, predecessor, midpoint);
      EXPECT_PRED_FORMAT2(LessThan, midpoint, successor);
    }
  }
}

TEST_P(PositionInsertTest, InsertBefore) {
  const std::string suffix = GetParam();
  for (size_t i = 0; i < kNumSortedPositions; ++i) {
    const UniquePosition& successor = kSortedPositionArray[i];
    // Verify our suffixes are unique before we continue.
    if (IsSuffixInUse(successor, suffix))
      continue;

    UniquePosition before = UniquePosition::Before(successor, suffix);

    EXPECT_PRED_FORMAT2(LessThan, before, successor);
  }
}

TEST_P(PositionInsertTest, InsertAfter) {
  const std::string suffix = GetParam();
  for (size_t i = 0; i < kNumSortedPositions; ++i) {
    const UniquePosition& predecessor = kSortedPositionArray[i];
    // Verify our suffixes are unique before we continue.
    if (IsSuffixInUse(predecessor, suffix))
      continue;

    UniquePosition after = UniquePosition::After(predecessor, suffix);

    EXPECT_PRED_FORMAT2(LessThan, predecessor, after);
  }
}

TEST_P(PositionInsertTest, StressInsertAfter) {
  // Use two different suffixes to not violate our suffix uniqueness guarantee.
  const std::string& suffix_a = GetParam();
  std::string suffix_b = suffix_a;
  suffix_b[10] = suffix_b[10] ^ 0xff;

  UniquePosition pos = UniquePosition::InitialPosition(suffix_a);
  for (int i = 0; i < 1024; i++) {
    const std::string& suffix = (i % 2 == 0) ? suffix_b : suffix_a;
    UniquePosition next_pos = UniquePosition::After(pos, suffix);
    ASSERT_PRED_FORMAT2(LessThan, pos, next_pos);
    pos = next_pos;
  }

  VLOG(1) << "Length: " << GetLength(pos);
}

TEST_P(PositionInsertTest, StressInsertBefore) {
  // Use two different suffixes to not violate our suffix uniqueness guarantee.
  const std::string& suffix_a = GetParam();
  std::string suffix_b = suffix_a;
  suffix_b[10] = suffix_b[10] ^ 0xff;

  UniquePosition pos = UniquePosition::InitialPosition(suffix_a);
  for (int i = 0; i < 1024; i++) {
    const std::string& suffix = (i % 2 == 0) ? suffix_b : suffix_a;
    UniquePosition prev_pos = UniquePosition::Before(pos, suffix);
    ASSERT_PRED_FORMAT2(LessThan, prev_pos, pos);
    pos = prev_pos;
  }

  VLOG(1) << "Length: " << GetLength(pos);
}

TEST_P(PositionInsertTest, StressLeftInsertBetween) {
  // Use different suffixes to not violate our suffix uniqueness guarantee.
  const std::string& suffix_a = GetParam();
  std::string suffix_b = suffix_a;
  suffix_b[10] = suffix_b[10] ^ 0xff;
  std::string suffix_c = suffix_a;
  suffix_c[10] = suffix_c[10] ^ 0xf0;

  UniquePosition right_pos = UniquePosition::InitialPosition(suffix_c);
  UniquePosition left_pos = UniquePosition::Before(right_pos, suffix_a);
  for (int i = 0; i < 1024; i++) {
    const std::string& suffix = (i % 2 == 0) ? suffix_b : suffix_a;
    UniquePosition new_pos =
        UniquePosition::Between(left_pos, right_pos, suffix);
    ASSERT_PRED_FORMAT2(LessThan, left_pos, new_pos);
    ASSERT_PRED_FORMAT2(LessThan, new_pos, right_pos);
    left_pos = new_pos;
  }

  VLOG(1) << "Lengths: " << GetLength(left_pos) << ", " << GetLength(right_pos);
}

TEST_P(PositionInsertTest, StressRightInsertBetween) {
  // Use different suffixes to not violate our suffix uniqueness guarantee.
  const std::string& suffix_a = GetParam();
  std::string suffix_b = suffix_a;
  suffix_b[10] = suffix_b[10] ^ 0xff;
  std::string suffix_c = suffix_a;
  suffix_c[10] = suffix_c[10] ^ 0xf0;

  UniquePosition right_pos = UniquePosition::InitialPosition(suffix_a);
  UniquePosition left_pos = UniquePosition::Before(right_pos, suffix_c);
  for (int i = 0; i < 1024; i++) {
    const std::string& suffix = (i % 2 == 0) ? suffix_b : suffix_a;
    UniquePosition new_pos =
        UniquePosition::Between(left_pos, right_pos, suffix);
    ASSERT_PRED_FORMAT2(LessThan, left_pos, new_pos);
    ASSERT_PRED_FORMAT2(LessThan, new_pos, right_pos);
    right_pos = new_pos;
  }

  VLOG(1) << "Lengths: " << GetLength(left_pos) << ", " << GetLength(right_pos);
}

// Generates suffixes similar to those generated by the directory.
// This may become obsolete if the suffix generation code is modified.
class SuffixGenerator {
 public:
  explicit SuffixGenerator(const std::string& cache_guid)
      : cache_guid_(cache_guid), next_id_(-65535) {}

  std::string NextSuffix() {
    // This is not entirely realistic, but that should be OK.  The current
    // suffix format is a base64'ed SHA1 hash, which should be fairly close to
    // random anyway.
    std::string input = cache_guid_ + base::NumberToString(next_id_--);
    std::string output;
    base::Base64Encode(base::SHA1HashString(input), &output);
    return output;
  }

 private:
  const std::string cache_guid_;
  int64_t next_id_;
};

// Cache guids generated in the same style as real clients.
static const char kCacheGuidStr1[] = "tuiWdG8hV+8y4RT9N5Aikg==";
static const char kCacheGuidStr2[] = "yaKb7zHtY06aue9a0vlZgw==";

class PositionScenariosTest : public UniquePositionTest {
 public:
  PositionScenariosTest()
      : generator1_(
            std::string(kCacheGuidStr1, base::size(kCacheGuidStr1) - 1)),
        generator2_(
            std::string(kCacheGuidStr2, base::size(kCacheGuidStr2) - 1)) {}

  std::string NextClient1Suffix() { return generator1_.NextSuffix(); }

  std::string NextClient2Suffix() { return generator2_.NextSuffix(); }

 private:
  SuffixGenerator generator1_;
  SuffixGenerator generator2_;
};

// One client creating new bookmarks, always inserting at the end.
TEST_F(PositionScenariosTest, OneClientInsertAtEnd) {
  UniquePosition pos = UniquePosition::InitialPosition(NextClient1Suffix());
  for (int i = 0; i < 1024; i++) {
    const std::string suffix = NextClient1Suffix();
    UniquePosition new_pos = UniquePosition::After(pos, suffix);
    ASSERT_PRED_FORMAT2(LessThan, pos, new_pos);
    pos = new_pos;
  }

  VLOG(1) << "Length: " << GetLength(pos);

  // Normally we wouldn't want to make an assertion about the internal
  // representation of our data, but we make an exception for this case.
  // If this scenario causes lengths to explode, we have a big problem.
  EXPECT_LT(GetLength(pos), 500U);
}

// Two clients alternately inserting entries at the end, with a strong
// bias towards insertions by the first client.
TEST_F(PositionScenariosTest, TwoClientsInsertAtEnd_A) {
  UniquePosition pos = UniquePosition::InitialPosition(NextClient1Suffix());
  for (int i = 0; i < 1024; i++) {
    std::string suffix;
    if (i % 5 == 0) {
      suffix = NextClient2Suffix();
    } else {
      suffix = NextClient1Suffix();
    }

    UniquePosition new_pos = UniquePosition::After(pos, suffix);
    ASSERT_PRED_FORMAT2(LessThan, pos, new_pos);
    pos = new_pos;
  }

  VLOG(1) << "Length: " << GetLength(pos);
  EXPECT_LT(GetLength(pos), 500U);
}

// Two clients alternately inserting entries at the end.
TEST_F(PositionScenariosTest, TwoClientsInsertAtEnd_B) {
  UniquePosition pos = UniquePosition::InitialPosition(NextClient1Suffix());
  for (int i = 0; i < 1024; i++) {
    std::string suffix;
    if (i % 2 == 0) {
      suffix = NextClient1Suffix();
    } else {
      suffix = NextClient2Suffix();
    }

    UniquePosition new_pos = UniquePosition::After(pos, suffix);
    ASSERT_PRED_FORMAT2(LessThan, pos, new_pos);
    pos = new_pos;
  }

  VLOG(1) << "Length: " << GetLength(pos);
  EXPECT_LT(GetLength(pos), 500U);
}

INSTANTIATE_TEST_SUITE_P(
    MinSuffix,
    PositionInsertTest,
    ::testing::Values(std::string(kMinSuffix, base::size(kMinSuffix))));
INSTANTIATE_TEST_SUITE_P(
    MaxSuffix,
    PositionInsertTest,
    ::testing::Values(std::string(kMaxSuffix, base::size(kMaxSuffix))));
INSTANTIATE_TEST_SUITE_P(
    NormalSuffix,
    PositionInsertTest,
    ::testing::Values(std::string(kNormalSuffix, base::size(kNormalSuffix))));

class PositionFromIntTest : public UniquePositionTest {
 public:
  PositionFromIntTest()
      : generator_(
            std::string(kCacheGuidStr1, base::size(kCacheGuidStr1) - 1)) {}

 protected:
  static const int64_t kTestValues[];
  static const size_t kNumTestValues;

  std::string NextSuffix() { return generator_.NextSuffix(); }

 private:
  SuffixGenerator generator_;
};

const int64_t PositionFromIntTest::kTestValues[] = {0LL,
                                                    1LL,
                                                    -1LL,
                                                    2LL,
                                                    -2LL,
                                                    3LL,
                                                    -3LL,
                                                    0x79LL,
                                                    -0x79LL,
                                                    0x80LL,
                                                    -0x80LL,
                                                    0x81LL,
                                                    -0x81LL,
                                                    0xFELL,
                                                    -0xFELL,
                                                    0xFFLL,
                                                    -0xFFLL,
                                                    0x100LL,
                                                    -0x100LL,
                                                    0x101LL,
                                                    -0x101LL,
                                                    0xFA1AFELL,
                                                    -0xFA1AFELL,
                                                    0xFFFFFFFELL,
                                                    -0xFFFFFFFELL,
                                                    0xFFFFFFFFLL,
                                                    -0xFFFFFFFFLL,
                                                    0x100000000LL,
                                                    -0x100000000LL,
                                                    0x100000001LL,
                                                    -0x100000001LL,
                                                    0xFFFFFFFFFFLL,
                                                    -0xFFFFFFFFFFLL,
                                                    0x112358132134LL,
                                                    -0x112358132134LL,
                                                    0xFEFFBEEFABC1234LL,
                                                    -0xFEFFBEEFABC1234LL,
                                                    INT64_MAX,
                                                    INT64_MIN,
                                                    INT64_MIN + 1,
                                                    INT64_MAX - 1};

const size_t PositionFromIntTest::kNumTestValues =
    base::size(PositionFromIntTest::kTestValues);

TEST_F(PositionFromIntTest, IsValid) {
  for (size_t i = 0; i < kNumTestValues; ++i) {
    const UniquePosition pos =
        UniquePosition::FromInt64(kTestValues[i], NextSuffix());
    EXPECT_TRUE(pos.IsValid()) << "i = " << i << "; " << pos.ToDebugString();
  }
}

TEST_F(PositionFromIntTest, RoundTripConversion) {
  for (size_t i = 0; i < kNumTestValues; ++i) {
    const int64_t expected_value = kTestValues[i];
    const UniquePosition pos =
        UniquePosition::FromInt64(kTestValues[i], NextSuffix());
    const int64_t value = pos.ToInt64();
    EXPECT_EQ(expected_value, value) << "i = " << i;
  }
}

template <typename T, typename LessThan = std::less<T>>
class IndexedLessThan {
 public:
  explicit IndexedLessThan(const T* values) : values_(values) {}

  bool operator()(int i1, int i2) {
    return less_than_(values_[i1], values_[i2]);
  }

 private:
  const T* values_;
  LessThan less_than_;
};

TEST_F(PositionFromIntTest, ConsistentOrdering) {
  UniquePosition positions[kNumTestValues];
  std::vector<int> original_ordering(kNumTestValues);
  std::vector<int> int64_ordering(kNumTestValues);
  std::vector<int> position_ordering(kNumTestValues);
  for (size_t i = 0; i < kNumTestValues; ++i) {
    positions[i] = UniquePosition::FromInt64(kTestValues[i], NextSuffix());
    original_ordering[i] = int64_ordering[i] = position_ordering[i] = i;
  }

  std::sort(int64_ordering.begin(), int64_ordering.end(),
            IndexedLessThan<int64_t>(kTestValues));
  std::sort(position_ordering.begin(), position_ordering.end(),
            IndexedLessThan<UniquePosition, PositionLessThan>(positions));
  EXPECT_NE(original_ordering, int64_ordering);
  EXPECT_EQ(int64_ordering, position_ordering);
}

class CompressedPositionTest : public UniquePositionTest {
 public:
  CompressedPositionTest() {
    positions_.push_back(MakePosition(  // Prefix starts with 256 0x00s
        std::string("\x00\x00\x00\x00\xFF\xFF\xFE\xFF"
                    "\x01",
                    9),
        MakeSuffix('\x04')));
    positions_.push_back(MakePosition(  // Prefix starts with four 0x00s
        std::string("\x00\x00\x00\x00\xFF\xFF\xFF\xFB"
                    "\x01",
                    9),
        MakeSuffix('\x03')));
    positions_.push_back(MakePosition(  // Prefix starts with four 0xFFs
        std::string("\xFF\xFF\xFF\xFF\x00\x00\x00\x04"
                    "\x01",
                    9),
        MakeSuffix('\x01')));
    positions_.push_back(MakePosition(  // Prefix starts with 256 0xFFs
        std::string("\xFF\xFF\xFF\xFF\x00\x00\x01\x00"
                    "\x01",
                    9),
        MakeSuffix('\x02')));
  }

 private:
  UniquePosition MakePosition(const std::string& compressed_prefix,
                              const std::string& compressed_suffix);
  std::string MakeSuffix(char unique_value);

 protected:
  std::vector<UniquePosition> positions_;
};

UniquePosition CompressedPositionTest::MakePosition(
    const std::string& compressed_prefix,
    const std::string& compressed_suffix) {
  sync_pb::UniquePosition proto;
  proto.set_custom_compressed_v1(
      std::string(compressed_prefix + compressed_suffix));
  return UniquePosition::FromProto(proto);
}

std::string CompressedPositionTest::MakeSuffix(char unique_value) {
  // We're dealing in compressed positions in this test.  That means the
  // suffix should be compressed, too.  To avoid complication, we use suffixes
  // that don't have any repeating digits, and therefore are identical in
  // compressed and uncompressed form.
  std::string suffix;
  for (size_t i = 0; i < UniquePosition::kSuffixLength; ++i) {
    suffix.push_back(static_cast<char>(i));
  }
  suffix[UniquePosition::kSuffixLength - 1] = unique_value;
  return suffix;
}

// Make sure that serialization and deserialization routines are correct.
TEST_F(CompressedPositionTest, SerializeAndDeserialize) {
  for (std::vector<UniquePosition>::const_iterator it = positions_.begin();
       it != positions_.end(); ++it) {
    SCOPED_TRACE("iteration: " + it->ToDebugString());

    UniquePosition deserialized = UniquePosition::FromProto(it->ToProto());

    EXPECT_PRED_FORMAT2(Equals, *it, deserialized);
  }
}

// Test that deserialization failures of protobufs where we know none of its
// fields is not catastrophic.  This may happen if all the fields currently
// known to this client become deprecated in the future.
TEST_F(CompressedPositionTest, DeserializeProtobufFromTheFuture) {
  sync_pb::UniquePosition proto;
  UniquePosition deserialized = UniquePosition::FromProto(proto);
  EXPECT_FALSE(deserialized.IsValid());
}

// Make sure the comparison functions are working correctly.
// This requires values in the test harness to be hard-coded in ascending order.
TEST_F(CompressedPositionTest, OrderingTest) {
  for (size_t i = 0; i < positions_.size() - 1; ++i) {
    EXPECT_PRED_FORMAT2(LessThan, positions_[i], positions_[i + 1]);
  }
}

}  // namespace

}  // namespace syncer
