// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/local_auth_factors/local_auth_factors_complexity.h"

#include <algorithm>
#include <string_view>

#include "base/strings/utf_string_conversion_utils.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/icu/source/common/unicode/uchar.h"

namespace policy::local_auth_factors {

namespace {

using Complexity = ash::LocalAuthFactorsComplexity;

enum class CharClass { kDigit, kLower, kUpper, kSymbol, kOther };

CharClass GetCharClass(base_icu::UChar32 c) {
  if (u_isdigit(c)) {
    return CharClass::kDigit;
  }
  if (u_islower(c)) {
    return CharClass::kLower;
  }
  if (u_isupper(c)) {
    return CharClass::kUpper;
  }
  if (u_isgraph(c) && !u_isalnum(c)) {
    return CharClass::kSymbol;
  }
  return CharClass::kOther;
}

// Returns true if the password contains 5+:
// - Repeating characters (e.g., "aaaaa", "@@@@@"),
// - Sequential letters or numbers (e.g., "abcde", "ABCDE", "98765").
bool ContainsTrivialSequence(std::string_view password) {
  constexpr int kMinSeq = 5;
  int inc = 1, dec = 1, same = 1;

  base_icu::UChar32 prev = -1;
  base_icu::UChar32 cur;
  for (size_t i = 0; base::ReadUnicodeCharacter(password, &i, &cur);
       prev = cur, ++i) {
    if (prev == -1) {
      continue;
    }

    const bool same_class = GetCharClass(cur) == GetCharClass(prev);
    const bool is_alphanum = GetCharClass(cur) != CharClass::kSymbol;

    // Increment valid same-class streaks. Symbols can only repeat.
    inc = (same_class && is_alphanum && cur == prev + 1) ? inc + 1 : 1;
    dec = (same_class && is_alphanum && cur == prev - 1) ? dec + 1 : 1;
    same = (same_class && cur == prev) ? same + 1 : 1;

    if (inc >= kMinSeq || dec >= kMinSeq || same >= kMinSeq) {
      return true;
    }
  }

  return false;
}

// Returns true for inputs like "6789", or "6543", or "0000" (but not for inputs
// like "8901" - wrap around isn't considered).
bool ContainsOrderedOrRepeatingSequence(std::string_view pin) {
  bool is_increasing = true;
  bool is_decreasing = true;
  bool is_same = true;

  for (size_t i = 1; i < pin.length(); i++) {
    const char prev = pin[i - 1];
    const char cur = pin[i];

    is_increasing = is_increasing && (cur == prev + 1);
    is_decreasing = is_decreasing && (cur == prev - 1);
    is_same = is_same && (cur == prev);
  }

  return is_increasing || is_decreasing || is_same;
}

}  // namespace

bool CheckPasswordComplexity(std::string_view password, Complexity complexity) {
  bool has_digit = false;
  bool has_lower = false;
  bool has_upper = false;
  bool has_symbol = false;

  for (size_t i = 0; i < password.length(); i++) {
    base_icu::UChar32 c;
    if (!base::ReadUnicodeCharacter(password, &i, &c)) {
      continue;
    }
    CharClass cls = GetCharClass(c);
    if (cls == CharClass::kDigit) {
      has_digit = true;
    } else if (cls == CharClass::kLower) {
      has_lower = true;
    } else if (cls == CharClass::kUpper) {
      has_upper = true;
    } else if (cls == CharClass::kSymbol) {
      has_symbol = true;
    }
  }

  size_t different_classes = has_digit + has_lower + has_upper + has_symbol;
  size_t length = base::CountUnicodeCharacters(password).value_or(0);

  switch (complexity) {
    case Complexity::kNone:
      return length >= 1;

    case Complexity::kLow:
      // The password must contain alphabetic (or symbol) characters (ie. must
      // not be a "PIN").
      return length >= 6 && (has_lower || has_upper || has_symbol);

    case Complexity::kMedium:
      // The password must contain at least two different sets of characters and
      // must not contain trivial sequential or repeating characters.
      return length >= 8 && different_classes >= 2 &&
             !ContainsTrivialSequence(password);

    case Complexity::kHigh:
      // The password must contain all four different sets of characters and
      // must not contain trivial sequential or repeating characters.
      return length >= 12 && different_classes == 4 &&
             !ContainsTrivialSequence(password);
  }
}

bool CheckPinComplexity(std::string_view pin, Complexity complexity) {
  // Check that the pin contains only digits.
  if (!std::ranges::all_of(pin, absl::ascii_isdigit)) {
    return false;
  }

  switch (complexity) {
    case Complexity::kNone:
      return pin.length() >= 1;

    case Complexity::kLow:
      return pin.length() >= 4;

    case Complexity::kMedium:
      return pin.length() >= 6 && !ContainsOrderedOrRepeatingSequence(pin);

    case Complexity::kHigh:
      return pin.length() >= 8 && !ContainsOrderedOrRepeatingSequence(pin);
  }
}

}  // namespace policy::local_auth_factors
