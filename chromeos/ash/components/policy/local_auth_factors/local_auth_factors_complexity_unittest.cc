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
  const struct TestData {
    std::string test_name;
    std::string_view password;
    Complexity complexity;
    bool expected_result;
  } kTestData[] = {
      // kNone tests (Length >= 1).
      {"NoneEmpty", "", Complexity::kNone, false},
      {"NoneShort", "z", Complexity::kNone, true},
      {"NoneLong", "a9B!qPz~wO", Complexity::kNone, true},
      {"NoneAnyChar", "@", Complexity::kNone, true},

      // kLow tests (Length >= 6, must not be all digits).
      {"LowTooShort", "aB1!", Complexity::kLow, false},
      {"LowJustEnoughAllDigit", "987654", Complexity::kLow, false},
      {"LowJustEnoughWithLower", "pqrstu", Complexity::kLow, true},
      {"LowJustEnoughWithUpper", "ZYXWVU", Complexity::kLow, true},
      {"LowJustEnoughWithSymbol", "$%^&*()", Complexity::kLow, true},
      {"LowJustEnoughMixedAlpha", "KzFwXm", Complexity::kLow, true},
      {"LowJustEnoughMixedAll", "jHkS;2", Complexity::kLow, true},
      {"LowLong", "QuErTyUiOp", Complexity::kLow, true},
      {"LowMixedChars", "P@sswOrd", Complexity::kLow, true},

      // kMedium tests (Length >= 8, >= 2 character classes).
      {"MediumTooShort", "aB1!dEf", Complexity::kMedium, false},
      {"MediumJustEnough1ClassLower", "qwertyui", Complexity::kMedium, false},
      {"MediumJustEnough1ClassUpper", "ASDFGHJK", Complexity::kMedium, false},
      {"MediumJustEnough1ClassDigit", "19283746", Complexity::kMedium, false},
      {"MediumJustEnough1ClassSymbol", "~!@#$%^&", Complexity::kMedium, false},
      {"MediumJustEnough2ClassLU", "PqRstUvW", Complexity::kMedium, true},
      {"MediumJustEnough2ClassLD", "mnbvcxz1", Complexity::kMedium, true},
      {"MediumJustEnough2ClassLS", "lkjhgfd?", Complexity::kMedium, true},
      {"MediumJustEnough2ClassUD", "ZXCVBNM8", Complexity::kMedium, true},
      {"MediumJustEnough2ClassUS", "QAZWSXED:", Complexity::kMedium, true},
      {"MediumJustEnough2ClassDS", "74185296+", Complexity::kMedium, true},
      {"MediumJustEnough3ClassLUD", "xYxYzZa1", Complexity::kMedium, true},
      {"MediumJustEnough3ClassLUS", "aBcDeFg@", Complexity::kMedium, true},
      {"MediumJustEnough4ClassLUNDS", "JkLmnop7$", Complexity::kMedium, true},
      {"MediumLong2Class", "zxcvbnm,./1", Complexity::kMedium, true},

      // kHigh tests (Length >= 12, all 4 character classes).
      {"HighTooShort", "aB1!dEfGhIj", Complexity::kHigh, false},
      {"HighJustEnoughNoDigit", "PqRsTuVwXyZ!", Complexity::kHigh, false},
      {"HighJustEnoughNoLower", "ASDFGHJK123$", Complexity::kHigh, false},
      {"HighJustEnoughNoUpper", "zxcvbnm123?/", Complexity::kHigh, false},
      {"HighJustEnoughNoSymbol", "QwErTyUiOp12", Complexity::kHigh, false},
      {"HighJustEnoughAll4", "aB1!dEfGhIjK", Complexity::kHigh, true},
      {"HighLongAll4", "mYpA55wOrd!sVeRy5eCuRe", Complexity::kHigh, true},
      {"HighVar1", "G00gL3%P@$$wOrd", Complexity::kHigh, true},
      {"HighVar2", "1s~Th1s_L0ng_En0ugH", Complexity::kHigh, true},
      {"HighVar3", "cOmPlExItY-Rul3z!23", Complexity::kHigh, true},

      // --- Repetitions and Sequences ---

      // Identical Characters (Max 4 allowed).
      {"Medium4IdenticalPass", "aaaa1234", Complexity::kMedium, true},
      {"Medium5IdenticalFail", "aaaaa123", Complexity::kMedium, false},
      {"Medium5IdenticalEndFail", "123aaaaa", Complexity::kMedium, false},
      {"High4IdenticalPass", "aaaaB1!dEfGh", Complexity::kHigh, true},
      {"High5IdenticalFail", "aaaaaB1!dEfG", Complexity::kHigh, false},
      {"LowIgnoresIdentical", "aaaaa1", Complexity::kLow, true},

      // Alphabetical / Numerical Sequences (Max 4 allowed).
      {"Medium4SeqPass", "abcd1234", Complexity::kMedium, true},
      {"Medium5SeqIncFail", "abcde123", Complexity::kMedium, false},
      {"Medium5SeqDecFail", "edcba123", Complexity::kMedium, false},
      {"High4SeqPass", "abcdB1!EfGhI", Complexity::kHigh, true},
      {"High5SeqIncFail", "abcdeB1!fGhI", Complexity::kHigh, false},
      {"High5SeqDecFail", "654321abCD!@", Complexity::kHigh, false},
      {"LowIgnoresSequence", "abcde1", Complexity::kLow, true},

      // Cross-class ASCII sequences.
      {"MediumCrossClassIncPass", "XYZ[\\123", Complexity::kMedium, true},
      {"MediumCrossClassDecPass", "{zyxw123", Complexity::kMedium, true},
      {"MediumCrossClassPunctDigitPass", "-./012ab", Complexity::kMedium, true},

      // Mixed delta sequence (Not a single continuous run).
      {"MediumMixedDeltaPass", "abcdc123", Complexity::kMedium, true},

      // Symbol sequence should pass, while symbol repetition should fail.
      {"HighSymbolSequencePass", "aA1#$%&'()*+,-./", Complexity::kHigh, true},
      {"HighSymbolRepetitionFail", "aA1@@@@@@", Complexity::kHigh, false},

      // Off by one errors at symbol <-> alnum boundaries.
      // ASCII 0x5D to 0x65: ]^_`abcde
      {"MediumSymbolLowerBoundaryPass", "]^_`abcd", Complexity::kMedium, true},
      {"MediumSymbolLowerBoundaryPass", "]^_`abcde", Complexity::kMedium,
       false},

      // Unicode characters (counting code points).
      {"UnicodeLowShort", "aa👋", Complexity::kLow, false},
      {"UnicodeLowJustEnough", "aaaaa👋", Complexity::kLow, true},
      {"UnicodeMediumShort", "👋👋👋👋👋👋a", Complexity::kMedium, false},
      {"UnicodeMediumPass", "👋👋👋👋aa11", Complexity::kMedium, true},
      {"UnicodeMediumSequence", "👋👋👋👋👋aa1", Complexity::kMedium, false},
      {"UnicodeNonLatin", "čćšđžaA1!", Complexity::kHigh, false},
      {"UnicodeNonLatinPass", "čćšđžaaAA11!!", Complexity::kHigh, true},
  };

  for (const auto& t : kTestData) {
    EXPECT_EQ(t.expected_result,
              CheckPasswordComplexity(t.password, t.complexity))
        << "Test case: " << t.test_name;
  }
}

TEST(LocalAuthFactorsComplexityCheckerTest, PinComplexity) {
  const struct TestData {
    std::string test_name;
    std::string_view pin;
    Complexity complexity;
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
    EXPECT_EQ(t.expected_result, CheckPinComplexity(t.pin, t.complexity))
        << "Test case: " << t.test_name;
  }
}

}  // namespace policy::local_auth_factors
