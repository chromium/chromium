// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/local_auth_factors/local_auth_factors_complexity_checker.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using Complexity = LocalAuthFactorsComplexity;

TEST(LocalAuthFactorsComplexityCheckerTest, Password) {
  EXPECT_TRUE(
      LocalAuthFactorsComplexityChecker::CheckPasswordComplexity("password"));
}

TEST(LocalAuthFactorsComplexityCheckerTest, PinComplexity) {
  const struct TestData {
    std::string test_name;
    std::string_view pin;
    LocalAuthFactorsComplexity complexity;
    bool expected_result;
  } kTestData[] = {
      // Non-digit tests.
      {"NonDigitNone", "a123", Complexity::kNone, false},
      {"NonDigitLow", "12b3", Complexity::kLow, false},
      {"NonDigitMedium", "123c45", Complexity::kMedium, false},
      {"NonDigitHigh", "1234d567", Complexity::kHigh, false},
      {"NonDigitSpace", " 123", Complexity::kNone, false},
      {"NonDigitSymbol", "44%4", Complexity::kNone, false},
      {"NonDigitGood", "123", Complexity::kNone, true},

      // kNone tests (Length >= 1).
      {"NoneEmpty", "", Complexity::kNone, false},
      {"NoneShort", "1", Complexity::kNone, true},
      {"NoneLong", "1234567890", Complexity::kNone, true},

      // kLow tests (Length >= 4).
      {"LowTooShort", "123", Complexity::kLow, false},
      {"LowJustEnough", "1234", Complexity::kLow, true},
      {"LowRepeatingAllowed", "1111", Complexity::kLow, true},
      {"LowIncreasingAllowed", "1234", Complexity::kLow, true},
      {"LowDecreasingAllowed", "4321", Complexity::kLow, true},
      {"LowLong", "12345", Complexity::kLow, true},

      // kMedium tests (Length >= 6, No ordered/repeating).
      {"MediumTooShort", "12345", Complexity::kMedium, false},
      {"MediumJustEnoughGood", "132456", Complexity::kMedium, true},
      {"MediumJustEnoughRepeating", "111111", Complexity::kMedium, false},
      {"MediumJustEnoughIncreasing", "123456", Complexity::kMedium, false},
      {"MediumJustEnoughDecreasing", "654321", Complexity::kMedium, false},
      {"MediumLongGood", "1324567", Complexity::kMedium, true},
      {"MediumLongRepeating", "2222222", Complexity::kMedium, false},
      {"MediumLongIncreasing", "0123456", Complexity::kMedium, false},
      {"MediumLongDecreasing", "9876543", Complexity::kMedium, false},
      {"MediumPartialSequence", "123567", Complexity::kMedium, true},

      // kHigh tests (Length >= 8, No ordered/repeating).
      {"HighTooShort", "1234567", Complexity::kHigh, false},
      {"HighJustEnoughGood", "13245678", Complexity::kHigh, true},
      {"HighJustEnoughRepeating", "11111111", Complexity::kHigh, false},
      {"HighJustEnoughIncreasing", "12345678", Complexity::kHigh, false},
      {"HighJustEnoughDecreasing", "87654321", Complexity::kHigh, false},
      {"HighLongGood", "132456789", Complexity::kHigh, true},
      {"HighLongRepeating", "333333333", Complexity::kHigh, false},
      {"HighLongIncreasing", "012345678", Complexity::kHigh, false},
      {"HighLongDecreasing", "987654321", Complexity::kHigh, false},
      {"HighPartialSequence", "01234578", Complexity::kHigh, true},
  };

  for (const auto& t : kTestData) {
    EXPECT_EQ(t.expected_result,
              LocalAuthFactorsComplexityChecker::CheckPinComplexity(
                  t.pin, t.complexity))
        << "Test case: " << t.test_name;
  }
}

}  // namespace policy
