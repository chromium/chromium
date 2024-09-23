// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/string_ordinal.h"

#include <algorithm>
#include <vector>

#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace syncer {

namespace {

// Create StringOrdinals that satisfy all but one criterion for validity.
// IsValid() should return false for all of them.
TEST(StringOrdinalTest, Invalid) {
  // Length criterion.
  EXPECT_FALSE(StringOrdinal(std::string()).IsValid());

  const char kBeforeA[] = {'a' - 1, '\0'};
  const char kAfterZ[] = {'z' + 1, '\0'};

  // Character criterion.
  EXPECT_FALSE(StringOrdinal(kBeforeA).IsValid());
  EXPECT_FALSE(StringOrdinal(kAfterZ).IsValid());

  // Zero criterion.
  EXPECT_FALSE(StringOrdinal("a").IsValid());

  // Trailing zero criterion.
  EXPECT_FALSE(StringOrdinal("ba").IsValid());
}

// Create StringOrdinals that satisfy all criteria for validity.
// IsValid() should return true for all of them.
TEST(StringOrdinalTest, Valid) {
  // Length criterion.
  EXPECT_TRUE(StringOrdinal("b").IsValid());
}

// Create StringOrdinals from CreateInitialOrdinal.  They should be valid
// and close to the middle of the range.
TEST(StringOrdinalTest, CreateInitialOrdinal) {
  const StringOrdinal& ordinal = StringOrdinal::CreateInitialOrdinal();
  ASSERT_TRUE(ordinal.IsValid());
  // "n" is the midpoint letter of the alphabet.
  EXPECT_TRUE(ordinal.Equals(StringOrdinal("n")));
}

// Create an invalid and a valid StringOrdinal.  EqualsOrBothInvalid should
// return true if called reflexively and false otherwise.
TEST(StringOrdinalTest, EqualsOrBothInvalid) {
  const StringOrdinal& valid_ordinal = StringOrdinal::CreateInitialOrdinal();
  const StringOrdinal invalid_ordinal;

  EXPECT_TRUE(valid_ordinal.EqualsOrBothInvalid(valid_ordinal));
  EXPECT_TRUE(invalid_ordinal.EqualsOrBothInvalid(invalid_ordinal));
  EXPECT_FALSE(invalid_ordinal.EqualsOrBothInvalid(valid_ordinal));
  EXPECT_FALSE(valid_ordinal.EqualsOrBothInvalid(invalid_ordinal));
}

// Create three StringOrdinals in order.  LessThan should return values
// consistent with that order.
TEST(StringOrdinalTest, LessThan) {
  const StringOrdinal small_ordinal("b");
  const StringOrdinal middle_ordinal("c");
  const StringOrdinal big_ordinal("d");

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

// Create three StringOrdinals in order.  GreaterThan should return values
// consistent with that order.
TEST(StringOrdinalTest, GreaterThan) {
  const StringOrdinal small_ordinal("b");
  const StringOrdinal middle_ordinal("c");
  const StringOrdinal big_ordinal("d");

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

// Create two valid StringOrdinals.  Equals should return true only when
// called reflexively.
TEST(StringOrdinalTest, Equals) {
  const StringOrdinal ordinal1("b");
  const StringOrdinal ordinal2("c");

  EXPECT_TRUE(ordinal1.Equals(ordinal1));
  EXPECT_FALSE(ordinal1.Equals(ordinal2));

  EXPECT_FALSE(ordinal2.Equals(ordinal1));
  EXPECT_TRUE(ordinal2.Equals(ordinal2));
}

// Create some valid ordinals from some byte strings.
// ToInternalValue() should return the original byte string.
TEST(StringOrdinalTest, ToInternalValue) {
  EXPECT_EQ("c", StringOrdinal("c").ToInternalValue());
  EXPECT_EQ("bcdef", StringOrdinal("bcdef").ToInternalValue());
}

bool IsNonEmptyPrintableString(const std::string& str) {
  if (str.empty()) {
    return false;
  }
  for (char c : str) {
    if (!absl::ascii_isprint(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  return true;
}

// Create some invalid/valid ordinals.  ToDebugString() should always
// return a non-empty printable string.
TEST(StringOrdinalTest, ToDebugString) {
  EXPECT_TRUE(IsNonEmptyPrintableString(StringOrdinal().ToDebugString()));
  EXPECT_TRUE(IsNonEmptyPrintableString(
      StringOrdinal("invalid string").ToDebugString()));
  EXPECT_TRUE(IsNonEmptyPrintableString(StringOrdinal("c").ToDebugString()));
  EXPECT_TRUE(
      IsNonEmptyPrintableString(StringOrdinal("bcdef").ToDebugString()));
}

// Create three StringOrdinals in order.  LessThanFn should return values
// consistent with that order.
TEST(StringOrdinalTest, LessThanFn) {
  const StringOrdinal small_ordinal("b");
  const StringOrdinal middle_ordinal("c");
  const StringOrdinal big_ordinal("d");

  const StringOrdinal::LessThanFn less_than;

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

std::string GetBetween(const std::string& ordinal_string1,
                       const std::string& ordinal_string2) {
  const StringOrdinal ordinal1(ordinal_string1);
  const StringOrdinal ordinal2(ordinal_string2);
  const StringOrdinal between1 = ordinal1.CreateBetween(ordinal2);
  const StringOrdinal between2 = ordinal2.CreateBetween(ordinal1);
  EXPECT_TRUE(between1.Equals(between2));
  return between1.ToInternalValue();
}

// Create some StringOrdinals from single-digit strings.  Given two strings
// from this set, CreateBetween should return a StringOrdinal roughly between
// them that are also single-digit when possible.
TEST(StringOrdinalTest, CreateBetweenSingleDigit) {
  EXPECT_EQ("c", GetBetween("b", "d"));
  EXPECT_EQ("bn", GetBetween("b", "c"));
  EXPECT_EQ("cn", GetBetween("c", "d"));
}

// Create some StringOrdinals from strings of various lengths.  Given two
// strings from this set, CreateBetween should return an StringOrdinal roughly
// between them that have as few digits as possible.
TEST(StringOrdinalTest, CreateBetweenDifferentLengths) {
  EXPECT_EQ("ban", GetBetween("b", "bb"));
  EXPECT_EQ("c", GetBetween("b", "db"));
  EXPECT_EQ("bo", GetBetween("bd", "c"));
  EXPECT_EQ("c", GetBetween("baaab", "d"));
  EXPECT_EQ("c", GetBetween("baaac", "d"));
  EXPECT_EQ("c", GetBetween("b", "daaac"));
}

// Create some StringOrdinals specifically designed to trigger overflow
// cases.  Given two strings from this set, CreateBetween should
// return a StringOrdinal roughly between them that have as few digits as
// possible.
TEST(StringOrdinalTest, CreateBetweenOverflow) {
  EXPECT_EQ("ao", GetBetween("ab", "bb"));
  EXPECT_EQ("bo", GetBetween("bb", "cb"));
  EXPECT_EQ("bbo", GetBetween("bbb", "bcb"));
  EXPECT_EQ("bo", GetBetween("aab", "ddd"));
  EXPECT_EQ("cpp", GetBetween("ccc", "ddd"));
  EXPECT_EQ("co", GetBetween("cab", "ddd"));
  EXPECT_EQ("bo", GetBetween("aad", "ddd"));
  EXPECT_EQ("boo", GetBetween("cccd", "bbbd"));
}

// Create some StringOrdinals.  CreateBefore should return a StringOrdinal
// roughly halfway towards "a".
TEST(StringOrdinalTest, CreateBefore) {
  EXPECT_EQ("an", StringOrdinal("b").CreateBefore().ToInternalValue());
  EXPECT_EQ("ao", StringOrdinal("bb").CreateBefore().ToInternalValue());
  EXPECT_EQ("ao", StringOrdinal("bc").CreateBefore().ToInternalValue());
  EXPECT_EQ("ap", StringOrdinal("bd").CreateBefore().ToInternalValue());
}

// Create some StringOrdinals.  CreateAfter should return a StringOrdinal
// roughly halfway towards "a".
TEST(StringOrdinalTest, CreateAfter) {
  EXPECT_EQ("o", StringOrdinal("d").CreateAfter().ToInternalValue());
  EXPECT_EQ("on", StringOrdinal("dc").CreateAfter().ToInternalValue());
  EXPECT_EQ("ooon", StringOrdinal("dddc").CreateAfter().ToInternalValue());
  EXPECT_EQ("o", StringOrdinal("cc").CreateAfter().ToInternalValue());
  EXPECT_EQ("o", StringOrdinal("cd").CreateAfter().ToInternalValue());
}

// Create two valid StringOrdinals.  EqualsFn should return true only when
// called reflexively.
TEST(StringOrdinalTest, EqualsFn) {
  const StringOrdinal ordinal1("b");
  const StringOrdinal ordinal2("c");

  const StringOrdinal::EqualsFn equals;

  EXPECT_TRUE(equals(ordinal1, ordinal1));
  EXPECT_FALSE(equals(ordinal1, ordinal2));

  EXPECT_FALSE(equals(ordinal2, ordinal1));
  EXPECT_TRUE(equals(ordinal2, ordinal2));
}

// Create some StringOrdinals and shuffle them.  Sorting them using
// LessThanFn should produce the correct order.
TEST(StringOrdinalTest, Sort) {
  const StringOrdinal ordinal1("bcefg");
  const StringOrdinal ordinal2("gfecb");
  const StringOrdinal ordinal3("ihgfe");
  const StringOrdinal ordinal4("jihgf");

  std::vector<StringOrdinal> sorted_ordinals;
  sorted_ordinals.push_back(ordinal1);
  sorted_ordinals.push_back(ordinal2);
  sorted_ordinals.push_back(ordinal3);
  sorted_ordinals.push_back(ordinal4);

  std::vector<StringOrdinal> ordinals = sorted_ordinals;
  base::RandomShuffle(ordinals.begin(), ordinals.end());
  base::ranges::sort(ordinals, StringOrdinal::LessThanFn());
  EXPECT_TRUE(base::ranges::equal(ordinals, sorted_ordinals,
                                  StringOrdinal::EqualsFn()));
}

}  // namespace

}  // namespace syncer
