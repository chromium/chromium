// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/local_auth_factors/local_auth_factors_complexity.h"

#include <algorithm>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/icu/source/common/unicode/uchar.h"

namespace policy::local_auth_factors {

namespace {

using Complexity = ash::LocalAuthFactorsComplexity;

// The rules used to validate a password
struct PasswordValidationRules {
  size_t min_length;
  size_t min_classes;
  bool check_non_pin;
  bool check_trivial_sequence;
};

constexpr PasswordValidationRules kDefaultPasswordValidationRules =
    PasswordValidationRules{/* min_length= */ 1, /* min_classes= */ 0,
                            /* check_non_pin= */ false,
                            /* check_trivial_sequence= */ false};

// clang-format off
// Password complexity to password validation rule map, used to lookup password
// validation rules.
constexpr auto kPasswordComplexityValidationMap = base::MakeFixedFlatMap<Complexity, PasswordValidationRules>({
    {Complexity::kNone,   kDefaultPasswordValidationRules},
    {Complexity::kLow,    {/* min_length= */ 6, /* min_classes= */ 0, /* check_non_pin= */ true, /* check_trivial_sequence= */ false}},
    {Complexity::kMedium, {/* min_length= */ 8, /* min_classes= */ 2, /* check_non_pin= */ false, /* check_trivial_sequence= */ true}},
    {Complexity::kHigh,   {/* min_length= */ 12, /* min_classes= */ 4, /* check_non_pin= */ false, /* check_trivial_sequence= */ true}}
});
// clang-format on

PasswordValidationRules GetPasswordComplexityValidationRules(
    Complexity complexity) {
  auto it = kPasswordComplexityValidationMap.find(complexity);
  return it != kPasswordComplexityValidationMap.end()
             ? it->second
             : kDefaultPasswordValidationRules;
}
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

PasswordComplexityResult CheckPasswordComplexity(std::string_view password,
                                                 Complexity complexity) {
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
  const PasswordValidationRules& rules =
      GetPasswordComplexityValidationRules(complexity);

  if (length < rules.min_length) {
    return PasswordComplexityResult::kTooShort;
  }

  if ((rules.check_non_pin && !(has_lower || has_upper || has_symbol)) ||
      (different_classes < rules.min_classes)) {
    return PasswordComplexityResult::kMissesCharacters;
  }

  if (rules.check_trivial_sequence && ContainsTrivialSequence(password)) {
    return PasswordComplexityResult::kContainsTrivialSequence;
  }

  return PasswordComplexityResult::kOk;
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
