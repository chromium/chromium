// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/local_auth_factors/local_auth_factors_complexity.h"

#include <string>
#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace policy::local_auth_factors {

using Complexity = ash::LocalAuthFactorsComplexity;

TEST(LocalAuthFactorsComplexityCheckerTest, PasswordComplexity) {
  using Result = PasswordComplexityResult;
  // clang-format off
  const struct TestData {
    std::string test_name;
    std::string_view password;
    Complexity complexity;
    Result expected_result;
  } kTestData[] = {
      // kNone tests (Length >= 1).
      {"NoneEmpty", "", Complexity::kNone, Result::kTooShort},
      {"NoneShort", "z", Complexity::kNone, Result::kOk},
      {"NoneLong", "a9B!qPz~wO", Complexity::kNone, Result::kOk},
      {"NoneAnyChar", "@", Complexity::kNone, Result::kOk},

      // kLow tests (Length >= 6, must not be all digits).
      {"LowTooShort", "aB1!", Complexity::kLow, Result::kTooShort},
      {"LowJustEnoughAllDigit", "987654", Complexity::kLow, Result::kMissesCharacters},
      {"LowJustEnoughWithLower", "pqrstu", Complexity::kLow, Result::kOk},
      {"LowJustEnoughWithUpper", "ZYXWVU", Complexity::kLow, Result::kOk},
      {"LowJustEnoughWithSymbol", "$%^&*()", Complexity::kLow, Result::kOk},
      {"LowJustEnoughMixedAlpha", "KzFwXm", Complexity::kLow, Result::kOk},
      {"LowJustEnoughMixedAll", "jHkS;2", Complexity::kLow, Result::kOk},
      {"LowLong", "QuErTyUiOp", Complexity::kLow, Result::kOk},
      {"LowMixedChars", "P@sswOrd", Complexity::kLow, Result::kOk},

      // kMedium tests (Length >= 8, >= 2 character classes).
      {"MediumTooShort", "aB1!dEf", Complexity::kMedium, Result::kTooShort},
      {"MediumJustEnough1ClassLower", "qwertyui", Complexity::kMedium, Result::kMissesCharacters},
      {"MediumJustEnough1ClassUpper", "ASDFGHJK", Complexity::kMedium,Result::kMissesCharacters},
      {"MediumJustEnough1ClassDigit", "19283746", Complexity::kMedium, Result::kMissesCharacters},
      {"MediumJustEnough1ClassSymbol", "~!@#$%^&", Complexity::kMedium, Result::kMissesCharacters},
      {"MediumJustEnough2ClassLU", "PqRstUvW", Complexity::kMedium, Result::kOk},
      {"MediumJustEnough2ClassLD", "mnbvcxz1", Complexity::kMedium, Result::kOk},
      {"MediumJustEnough2ClassLS", "lkjhgfd?", Complexity::kMedium, Result::kOk},
      {"MediumJustEnough2ClassUD", "ZXCVBNM8", Complexity::kMedium, Result::kOk},
      {"MediumJustEnough2ClassUS", "QAZWSXED:", Complexity::kMedium, Result::kOk},
      {"MediumJustEnough2ClassDS", "74185296+", Complexity::kMedium, Result::kOk},
      {"MediumJustEnough3ClassLUD", "xYxYzZa1", Complexity::kMedium, Result::kOk},
      {"MediumJustEnough3ClassLUS", "aBcDeFg@", Complexity::kMedium, Result::kOk},
      {"MediumJustEnough4ClassLUNDS", "JkLmnop7$", Complexity::kMedium, Result::kOk},
      {"MediumLong2Class", "zxcvbnm,./1", Complexity::kMedium, Result::kOk},

      // kHigh tests (Length >= 12, all 4 character classes).
      {"HighTooShort", "aB1!dEfGhIj", Complexity::kHigh, Result::kTooShort},
      {"HighJustEnoughNoDigit", "PqRsTuVwXyZ!", Complexity::kHigh, Result::kMissesCharacters},
      {"HighJustEnoughNoLower", "ASDFGHJK123$", Complexity::kHigh, Result::kMissesCharacters},
      {"HighJustEnoughNoUpper", "zxcvbnm123?/", Complexity::kHigh, Result::kMissesCharacters},
      {"HighJustEnoughNoSymbol", "QwErTyUiOp12", Complexity::kHigh, Result::kMissesCharacters},
      {"HighJustEnoughAll4", "aB1!dEfGhIjK", Complexity::kHigh, Result::kOk},
      {"HighLongAll4", "mYpA55wOrd!sVeRy5eCuRe", Complexity::kHigh, Result::kOk},
      {"HighVar1", "G00gL3%P@$$wOrd", Complexity::kHigh, Result::kOk},
      {"HighVar2", "1s~Th1s_L0ng_En0ugH", Complexity::kHigh, Result::kOk},
      {"HighVar3", "cOmPlExItY-Rul3z!23", Complexity::kHigh, Result::kOk},

      // --- Repetitions and Sequences ---

      // Identical Characters (Max 4 allowed).
      {"Medium4IdenticalPass", "aaaa1234", Complexity::kMedium, Result::kOk},
      {"Medium5IdenticalFail", "aaaaa123", Complexity::kMedium, Result::kContainsTrivialSequence},
      {"Medium5IdenticalEndFail", "123aaaaa", Complexity::kMedium, Result::kContainsTrivialSequence},
      {"High4IdenticalPass", "aaaaB1!dEfGh", Complexity::kHigh, Result::kOk},
      {"High5IdenticalFail", "aaaaaB1!dEfG", Complexity::kHigh, Result::kContainsTrivialSequence},
      {"LowIgnoresIdentical", "aaaaa1", Complexity::kLow, Result::kOk},

      // Alphabetical / Numerical Sequences (Max 4 allowed).
      {"Medium4SeqPass", "abcd1234", Complexity::kMedium, Result::kOk},
      {"Medium5SeqIncFail", "abcde123", Complexity::kMedium, Result::kContainsTrivialSequence},
      {"Medium5SeqDecFail", "edcba123", Complexity::kMedium, Result::kContainsTrivialSequence},
      {"High4SeqPass", "abcdB1!EfGhI", Complexity::kHigh, Result::kOk},
      {"High5SeqIncFail", "abcdeB1!fGhI", Complexity::kHigh, Result::kContainsTrivialSequence},
      {"High5SeqDecFail", "654321abCD!@", Complexity::kHigh, Result::kContainsTrivialSequence},
      {"LowIgnoresSequence", "abcde1", Complexity::kLow, Result::kOk},

      // Cross-class ASCII sequences.
      {"MediumCrossClassIncPass", "XYZ[\\123", Complexity::kMedium, Result::kOk},
      {"MediumCrossClassDecPass", "{zyxw123", Complexity::kMedium, Result::kOk},
      {"MediumCrossClassPunctDigitPass", "-./012ab", Complexity::kMedium, Result::kOk},

      // Mixed delta sequence (Not a single continuous run).
      {"MediumMixedDeltaPass", "abcdc123", Complexity::kMedium, Result::kOk},

      // Symbol sequence should pass, while symbol repetition should fail.
      {"HighSymbolSequencePass", "aA1#$%&'()*+,-./", Complexity::kHigh, Result::kOk},
      {"HighSymbolRepetitionFail", "aA1!B2c3@@@@@@", Complexity::kHigh, Result::kContainsTrivialSequence},

      // Off by one errors at symbol <-> alnum boundaries.
      // ASCII 0x5D to 0x65: ]^_`abcde
      {"MediumSymbolLowerBoundaryPass", "]^_`abcd", Complexity::kMedium, Result::kOk},
      {"MediumSymbolLowerBoundaryPass", "]^_`abcde", Complexity::kMedium, Result::kContainsTrivialSequence},

      // Unicode characters (counting code points).
      {"UnicodeLowShort", "aa👋", Complexity::kLow, Result::kTooShort},
      {"UnicodeLowJustEnough", "aaaaa👋", Complexity::kLow, Result::kOk},
      {"UnicodeMediumShort", "👋👋👋👋👋👋a", Complexity::kMedium, Result::kTooShort},
      {"UnicodeMediumPass", "👋👋👋👋aa11", Complexity::kMedium, Result::kOk},
      {"UnicodeMediumSequence", "👋👋👋👋👋aa1", Complexity::kMedium, Result::kContainsTrivialSequence},
      {"UnicodeNonLatin", "čćšđža1!abcd", Complexity::kHigh, Result::kMissesCharacters},
      {"UnicodeNonLatinPass", "čćšđžaaAA11!!", Complexity::kHigh, Result::kOk},

      // Uncased scripts and multi-byte sequences should be correctly
      // classified to meet complexity requirements.
      {"ArabicUncasedLow", "العربية", Complexity::kLow, Result::kOk},
      {"JapaneseUncasedLow", "あいうえおか", Complexity::kLow, Result::kOk},
  };
  // clang-format on

  for (const auto& t : kTestData) {
    EXPECT_EQ(t.expected_result,
              CheckPasswordComplexity(t.password, t.complexity))
        << "Test case: " << t.test_name;
  }
}

TEST(LocalAuthFactorsComplexityCheckerTest, PinComplexity) {
  using Result = PinComplexityResult;
  // clang-format off
  const struct TestData {
    std::string test_name;
    std::string_view pin;
    Complexity complexity;
    Result expected_result;
  } kTestData[] = {
      // Non-digit tests.
      {"NonDigitNone", "a123", Complexity::kNone, Result::kContainsNonDigits},
      {"NonDigitLow", "12b3", Complexity::kLow, Result::kContainsNonDigits},
      {"NonDigitMedium", "123c45", Complexity::kMedium, Result::kContainsNonDigits},
      {"NonDigitHigh", "1234d567", Complexity::kHigh, Result::kContainsNonDigits},
      {"NonDigitSpace", " 123", Complexity::kNone, Result::kContainsNonDigits},
      {"NonDigitSymbol", "44%4", Complexity::kNone, Result::kContainsNonDigits},
      {"NonDigitGood", "123", Complexity::kNone, Result::kOk},

      // kNone tests (Length >= 1).
      {"NoneEmpty", "", Complexity::kNone, Result::kTooShort},
      {"NoneShort", "1", Complexity::kNone, Result::kOk},
      {"NoneLong", "1234567890", Complexity::kNone, Result::kOk},

      // kLow tests (Length >= 4).
      {"LowTooShort", "123", Complexity::kLow, Result::kTooShort},
      {"LowJustEnough", "1234", Complexity::kLow, Result::kOk},
      {"LowRepeatingAllowed", "1111", Complexity::kLow, Result::kOk},
      {"LowIncreasingAllowed", "1234", Complexity::kLow, Result::kOk},
      {"LowDecreasingAllowed", "4321", Complexity::kLow, Result::kOk},
      {"LowLong", "12345", Complexity::kLow, Result::kOk},

      // kMedium tests (Length >= 6, No ordered/repeating).
      {"MediumTooShort", "12345", Complexity::kMedium, Result::kTooShort},
      {"MediumJustEnoughGood", "132456", Complexity::kMedium, Result::kOk},
      {"MediumJustEnoughRepeating", "111111", Complexity::kMedium, Result::kContainsRepeatingDigits},
      {"MediumJustEnoughIncreasing", "123456", Complexity::kMedium, Result::kContainsOrderedSequence},
      {"MediumJustEnoughDecreasing", "654321", Complexity::kMedium, Result::kContainsOrderedSequence},
      {"MediumLongGood", "1324567", Complexity::kMedium, Result::kOk},
      {"MediumLongRepeating", "2222222", Complexity::kMedium, Result::kContainsRepeatingDigits},
      {"MediumLongIncreasing", "0123456", Complexity::kMedium, Result::kContainsOrderedSequence},
      {"MediumLongDecreasing", "9876543", Complexity::kMedium, Result::kContainsOrderedSequence},
      {"MediumPartialSequence", "123567", Complexity::kMedium, Result::kOk},

      // kHigh tests (Length >= 8, No ordered/repeating).
      {"HighTooShort", "1234567", Complexity::kHigh, Result::kTooShort},
      {"HighJustEnoughGood", "13245678", Complexity::kHigh, Result::kOk},
      {"HighJustEnoughRepeating", "11111111", Complexity::kHigh, Result::kContainsRepeatingDigits},
      {"HighJustEnoughIncreasing", "12345678", Complexity::kHigh, Result::kContainsOrderedSequence},
      {"HighJustEnoughDecreasing", "87654321", Complexity::kHigh, Result::kContainsOrderedSequence},
      {"HighLongGood", "132456789", Complexity::kHigh, Result::kOk},
      {"HighLongRepeating", "333333333", Complexity::kHigh, Result::kContainsRepeatingDigits},
      {"HighLongIncreasing", "012345678", Complexity::kHigh, Result::kContainsOrderedSequence},
      {"HighLongDecreasing", "987654321", Complexity::kHigh, Result::kContainsOrderedSequence},
      {"HighPartialSequence", "01234578", Complexity::kHigh, Result::kOk},
  };
  // clang-format on

  for (const auto& t : kTestData) {
    EXPECT_EQ(t.expected_result, CheckPinComplexity(t.pin, t.complexity))
        << "Test case: " << t.test_name;
  }
}

}  // namespace policy::local_auth_factors
