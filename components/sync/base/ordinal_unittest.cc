// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/ordinal.h"

#include <cctype>
#include <vector>

#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

struct TestOrdinalTraits {
  static const uint8_t kZeroDigit = '0';
  static const uint8_t kMaxDigit = '3';
  static const size_t kMinLength = 1;
};

struct LongOrdinalTraits {
  static const uint8_t kZeroDigit = '0';
  static const uint8_t kMaxDigit = '9';
  static const size_t kMinLength = 5;
};

struct LargeOrdinalTraits {
  static const uint8_t kZeroDigit = 0;
  static const uint8_t kMaxDigit = UINT8_MAX;
  static const size_t kMinLength = 1;
};

using TestOrdinal = Ordinal<TestOrdinalTraits>;
using LongOrdinal = Ordinal<LongOrdinalTraits>;
using LargeOrdinal = Ordinal<LargeOrdinalTraits>;

static_assert(TestOrdinal::kZeroDigit == '0',
              "incorrect TestOrdinal zero digit");
static_assert(TestOrdinal::kOneDigit == '1', "incorrect TestOrdinal one digit");
static_assert(TestOrdinal::kMidDigit == '2', "incorrect TestOrdinal min digit");
static_assert(TestOrdinal::kMaxDigit == '3', "incorrect TestOrdinal max digit");
static_assert(TestOrdinal::kMidDigitValue == 2,
              "incorrect TestOrdinal mid digit value");
static_assert(TestOrdinal::kMaxDigitValue == 3,
              "incorrect TestOrdinal max digit value");
static_assert(TestOrdinal::kRadix == 4, "incorrect TestOrdinal radix");

static_assert(LongOrdinal::kZeroDigit == '0',
              "incorrect LongOrdinal zero digit");
static_assert(LongOrdinal::kOneDigit == '1', "incorrect LongOrdinal one digit");
static_assert(LongOrdinal::kMidDigit == '5', "incorrect LongOrdinal mid digit");
static_assert(LongOrdinal::kMaxDigit == '9', "incorrect LongOrdinal max digit");
static_assert(LongOrdinal::kMidDigitValue == 5,
              "incorrect LongOrdinal mid digit value");
static_assert(LongOrdinal::kMaxDigitValue == 9,
              "incorrect LongOrdinal max digit value");
static_assert(LongOrdinal::kRadix == 10, "incorrect LongOrdinal radix");

static_assert(static_cast<char>(LargeOrdinal::kZeroDigit) == '\x00',
              "incorrect LargeOrdinal zero digit");
static_assert(static_cast<char>(LargeOrdinal::kOneDigit) == '\x01',
              "incorrect LargeOrdinal one digit");
static_assert(static_cast<char>(LargeOrdinal::kMidDigit) == '\x80',
              "incorrect LargeOrdinal mid digit");
static_assert(static_cast<char>(LargeOrdinal::kMaxDigit) == '\xff',
              "incorrect LargeOrdinal max digit");
static_assert(LargeOrdinal::kMidDigitValue == 128,
              "incorrect LargeOrdinal mid digit value");
static_assert(LargeOrdinal::kMaxDigitValue == 255,
              "incorrect LargeOrdinal max digit value");
static_assert(LargeOrdinal::kRadix == 256, "incorrect LargeOrdinal radix");

// Create Ordinals that satisfy all but one criterion for validity.
// IsValid() should return false for all of them.
TEST(Ordinal, Invalid) {
  // Length criterion.
  EXPECT_FALSE(TestOrdinal(std::string()).IsValid());
  EXPECT_FALSE(LongOrdinal("0001").IsValid());

  const char kBeforeZero[] = {'0' - 1, '\0'};
  const char kAfterNine[] = {'9' + 1, '\0'};

  // Character criterion.
  EXPECT_FALSE(TestOrdinal(kBeforeZero).IsValid());
  EXPECT_FALSE(TestOrdinal("4").IsValid());
  EXPECT_FALSE(LongOrdinal(std::string("0000") + kBeforeZero).IsValid());
  EXPECT_FALSE(LongOrdinal(std::string("0000") + kAfterNine).IsValid());

  // Zero criterion.
  EXPECT_FALSE(TestOrdinal("0").IsValid());
  EXPECT_FALSE(TestOrdinal("00000").IsValid());

  // Trailing zero criterion.
  EXPECT_FALSE(TestOrdinal("10").IsValid());
  EXPECT_FALSE(TestOrdinal("111110").IsValid());
}

// Create Ordinals that satisfy all criteria for validity.
// IsValid() should return true for all of them.
TEST(Ordinal, Valid) {
  // Length criterion.
  EXPECT_TRUE(TestOrdinal("1").IsValid());
  EXPECT_TRUE(LongOrdinal("10000").IsValid());
}

// Create Ordinals from CreateInitialOrdinal.  They should be valid
// and close to the middle of the range.
TEST(Ordinal, CreateInitialOrdinal) {
  const TestOrdinal& ordinal1 = TestOrdinal::CreateInitialOrdinal();
  const LongOrdinal& ordinal2 = LongOrdinal::CreateInitialOrdinal();
  ASSERT_TRUE(ordinal1.IsValid());
  ASSERT_TRUE(ordinal2.IsValid());
  EXPECT_TRUE(ordinal1.Equals(TestOrdinal("2")));
  EXPECT_TRUE(ordinal2.Equals(LongOrdinal("50000")));
}

// Create an invalid and a valid Ordinal.  EqualsOrBothInvalid should
// return true if called reflexively and false otherwise.
TEST(Ordinal, EqualsOrBothInvalid) {
  const TestOrdinal& valid_ordinal = TestOrdinal::CreateInitialOrdinal();
  const TestOrdinal invalid_ordinal;

  EXPECT_TRUE(valid_ordinal.EqualsOrBothInvalid(valid_ordinal));
  EXPECT_TRUE(invalid_ordinal.EqualsOrBothInvalid(invalid_ordinal));
  EXPECT_FALSE(invalid_ordinal.EqualsOrBothInvalid(valid_ordinal));
  EXPECT_FALSE(valid_ordinal.EqualsOrBothInvalid(invalid_ordinal));
}

// Create three Ordinals in order.  LessThan should return values
// consistent with that order.
TEST(Ordinal, LessThan) {
  const TestOrdinal small_ordinal("1");
  const TestOrdinal middle_ordinal("2");
  const TestOrdinal big_ordinal("3");

  EXPECT_FALSE(small_ordinal.LessThan(small_ordinal));
  EXPECT_TRUE(small_ordinal.LessThan(middle_ordinal));
  EXPECT_TRUE(small_ordinal.LessThan(big_ordinal));

  EXPECT_FALSE(middle_ordinal.LessThan(small_ordinal));
  EXPECT_FALSE(middle_ordinal.LessThan(middle_ordinal));
  EXPECT_TRUE(middle_ordinal.LessThan(big_ordinal));

  EXPECT_FALSE(big_ordinal.LessThan(small_ordinal));
  EXPECT_FALSE(big_ordinal.LessThan(middle_ordinal));
  EXPECT_FALSE(big_ordinal.LessThan(big_ordinal));
}

// Create two single-digit ordinals with byte values 0 and 255.  The
// former should compare as less than the latter, even though the
// native char type may be signed.
TEST(Ordinal, LessThanLarge) {
  const LargeOrdinal small_ordinal("\x01");
  const LargeOrdinal big_ordinal("\xff");

  EXPECT_TRUE(small_ordinal.LessThan(big_ordinal));
}

// Create three Ordinals in order.  GreaterThan should return values
// consistent with that order.
TEST(Ordinal, GreaterThan) {
  const LongOrdinal small_ordinal("10000");
  const LongOrdinal middle_ordinal("55555");
  const LongOrdinal big_ordinal("99999");

  EXPECT_FALSE(small_ordinal.GreaterThan(small_ordinal));
  EXPECT_FALSE(small_ordinal.GreaterThan(middle_ordinal));
  EXPECT_FALSE(small_ordinal.GreaterThan(big_ordinal));

  EXPECT_TRUE(middle_ordinal.GreaterThan(small_ordinal));
  EXPECT_FALSE(middle_ordinal.GreaterThan(middle_ordinal));
  EXPECT_FALSE(middle_ordinal.GreaterThan(big_ordinal));

  EXPECT_TRUE(big_ordinal.GreaterThan(small_ordinal));
  EXPECT_TRUE(big_ordinal.GreaterThan(middle_ordinal));
  EXPECT_FALSE(big_ordinal.GreaterThan(big_ordinal));
}

// Create two valid Ordinals.  Equals should return true only when
// called reflexively.
TEST(Ordinal, Equals) {
  const TestOrdinal ordinal1("1");
  const TestOrdinal ordinal2("2");

  EXPECT_TRUE(ordinal1.Equals(ordinal1));
  EXPECT_FALSE(ordinal1.Equals(ordinal2));

  EXPECT_FALSE(ordinal2.Equals(ordinal1));
  EXPECT_TRUE(ordinal2.Equals(ordinal2));
}

// Create some valid ordinals from some byte strings.
// ToInternalValue() should return the original byte string.
TEST(OrdinalTest, ToInternalValue) {
  EXPECT_EQ("2", TestOrdinal("2").ToInternalValue());
  EXPECT_EQ("12345", LongOrdinal("12345").ToInternalValue());
  EXPECT_EQ("\1\2\3\4\5", LargeOrdinal("\1\2\3\4\5").ToInternalValue());
}

bool IsNonEmptyPrintableString(const std::string& str) {
  if (str.empty())
    return false;
  for (char c : str) {
    if (!isprint(c))
      return false;
  }
  return true;
}

// Create some invalid/valid ordinals.  ToDebugString() should always
// return a non-empty printable string.
TEST(OrdinalTest, ToDebugString) {
  EXPECT_TRUE(IsNonEmptyPrintableString(TestOrdinal().ToDebugString()));
  EXPECT_TRUE(
      IsNonEmptyPrintableString(TestOrdinal("invalid string").ToDebugString()));
  EXPECT_TRUE(IsNonEmptyPrintableString(TestOrdinal("2").ToDebugString()));
  EXPECT_TRUE(IsNonEmptyPrintableString(LongOrdinal("12345").ToDebugString()));
  EXPECT_TRUE(
      IsNonEmptyPrintableString(LargeOrdinal("\1\2\3\4\5").ToDebugString()));
}

// Create three Ordinals in order.  LessThanFn should return values
// consistent with that order.
TEST(Ordinal, LessThanFn) {
  const TestOrdinal small_ordinal("1");
  const TestOrdinal middle_ordinal("2");
  const TestOrdinal big_ordinal("3");

  const TestOrdinal::LessThanFn less_than;

  EXPECT_FALSE(less_than(small_ordinal, small_ordinal));
  EXPECT_TRUE(less_than(small_ordinal, middle_ordinal));
  EXPECT_TRUE(less_than(small_ordinal, big_ordinal));

  EXPECT_FALSE(less_than(middle_ordinal, small_ordinal));
  EXPECT_FALSE(less_than(middle_ordinal, middle_ordinal));
  EXPECT_TRUE(less_than(middle_ordinal, big_ordinal));

  EXPECT_FALSE(less_than(big_ordinal, small_ordinal));
  EXPECT_FALSE(less_than(big_ordinal, middle_ordinal));
  EXPECT_FALSE(less_than(big_ordinal, big_ordinal));
}

template <typename Traits>
std::string GetBetween(const std::string& ordinal_string1,
                       const std::string& ordinal_string2) {
  const Ordinal<Traits> ordinal1(ordinal_string1);
  const Ordinal<Traits> ordinal2(ordinal_string2);
  const Ordinal<Traits> between1 = ordinal1.CreateBetween(ordinal2);
  const Ordinal<Traits> between2 = ordinal2.CreateBetween(ordinal1);
  EXPECT_TRUE(between1.Equals(between2));
  return between1.ToInternalValue();
}

// Create some Ordinals from single-digit strings.  Given two strings
// from this set, CreateBetween should return an Ordinal roughly between
// them that are also single-digit when possible.
TEST(Ordinal, CreateBetweenSingleDigit) {
  EXPECT_EQ("2", GetBetween<TestOrdinal>("1", "3"));
  EXPECT_EQ("12", GetBetween<TestOrdinal>("1", "2"));
  EXPECT_EQ("22", GetBetween<TestOrdinal>("2", "3"));
}

// Create some Ordinals from strings of various lengths.  Given two
// strings from this set, CreateBetween should return an Ordinal roughly
// between them that have as few digits as possible.
TEST(Ordinal, CreateBetweenDifferentLengths) {
  EXPECT_EQ("102", GetBetween<TestOrdinal>("1", "11"));
  EXPECT_EQ("2", GetBetween<TestOrdinal>("1", "31"));
  EXPECT_EQ("132", GetBetween<TestOrdinal>("13", "2"));
  EXPECT_EQ("2", GetBetween<TestOrdinal>("10001", "3"));
  EXPECT_EQ("20000", GetBetween<LongOrdinal>("10001", "30000"));
  EXPECT_EQ("2", GetBetween<TestOrdinal>("10002", "3"));
  EXPECT_EQ("20001", GetBetween<LongOrdinal>("10002", "30000"));
  EXPECT_EQ("2", GetBetween<TestOrdinal>("1", "30002"));
  EXPECT_EQ("20001", GetBetween<LongOrdinal>("10000", "30002"));
}

// Create some Ordinals specifically designed to trigger overflow
// cases.  Given two strings from this set, CreateBetween should
// return an Ordinal roughly between them that have as few digits as
// possible.
TEST(Ordinal, CreateBetweenOverflow) {
  EXPECT_EQ("03", GetBetween<TestOrdinal>("01", "11"));
  EXPECT_EQ("13", GetBetween<TestOrdinal>("11", "21"));
  EXPECT_EQ("113", GetBetween<TestOrdinal>("111", "121"));
  EXPECT_EQ("2", GetBetween<TestOrdinal>("001", "333"));
  EXPECT_EQ("31", GetBetween<TestOrdinal>("222", "333"));
  EXPECT_EQ("3", GetBetween<TestOrdinal>("201", "333"));
  EXPECT_EQ("2", GetBetween<TestOrdinal>("003", "333"));
  EXPECT_EQ("2", GetBetween<TestOrdinal>("2223", "1113"));
}

// Create some Ordinals specifically designed to trigger digit
// overflow cases.  Given two strings from this set, CreateBetween
// should return an Ordinal roughly between them that have as few digits
// as possible.
TEST(Ordinal, CreateBetweenOverflowLarge) {
  EXPECT_EQ("\x80", GetBetween<LargeOrdinal>("\x01\xff", "\xff\xff"));
  EXPECT_EQ("\xff\xfe\x80", GetBetween<LargeOrdinal>("\xff\xfe", "\xff\xff"));
}

// Create some Ordinals.  CreateBefore should return an Ordinal
// roughly halfway towards 0.
TEST(Ordinal, CreateBefore) {
  EXPECT_EQ("02", TestOrdinal("1").CreateBefore().ToInternalValue());
  EXPECT_EQ("03", TestOrdinal("11").CreateBefore().ToInternalValue());
  EXPECT_EQ("03", TestOrdinal("12").CreateBefore().ToInternalValue());
  EXPECT_EQ("1", TestOrdinal("13").CreateBefore().ToInternalValue());
}

// Create some Ordinals.  CreateAfter should return an Ordinal
// roughly halfway towards 0.
TEST(Ordinal, CreateAfter) {
  EXPECT_EQ("31", TestOrdinal("3").CreateAfter().ToInternalValue());
  EXPECT_EQ("322", TestOrdinal("32").CreateAfter().ToInternalValue());
  EXPECT_EQ("33322", TestOrdinal("3332").CreateAfter().ToInternalValue());
  EXPECT_EQ("3", TestOrdinal("22").CreateAfter().ToInternalValue());
  EXPECT_EQ("3", TestOrdinal("23").CreateAfter().ToInternalValue());
}

// Create two valid Ordinals.  EqualsFn should return true only when
// called reflexively.
TEST(Ordinal, EqualsFn) {
  const TestOrdinal ordinal1("1");
  const TestOrdinal ordinal2("2");

  const TestOrdinal::EqualsFn equals;

  EXPECT_TRUE(equals(ordinal1, ordinal1));
  EXPECT_FALSE(equals(ordinal1, ordinal2));

  EXPECT_FALSE(equals(ordinal2, ordinal1));
  EXPECT_TRUE(equals(ordinal2, ordinal2));
}

// Create some Ordinals and shuffle them.  Sorting them using
// LessThanFn should produce the correct order.
TEST(Ordinal, Sort) {
  const LongOrdinal ordinal1("12345");
  const LongOrdinal ordinal2("54321");
  const LongOrdinal ordinal3("87654");
  const LongOrdinal ordinal4("98765");

  std::vector<LongOrdinal> sorted_ordinals;
  sorted_ordinals.push_back(ordinal1);
  sorted_ordinals.push_back(ordinal2);
  sorted_ordinals.push_back(ordinal3);
  sorted_ordinals.push_back(ordinal4);

  std::vector<LongOrdinal> ordinals = sorted_ordinals;
  base::RandomShuffle(ordinals.begin(), ordinals.end());
  std::sort(ordinals.begin(), ordinals.end(), LongOrdinal::LessThanFn());
  EXPECT_TRUE(std::equal(ordinals.begin(), ordinals.end(),
                         sorted_ordinals.begin(), LongOrdinal::EqualsFn()));
}

}  // namespace

}  // namespace syncer
