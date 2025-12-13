// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/local_auth_factors/local_auth_factors_complexity.h"

#include <algorithm>
#include <string_view>

#include "ash/constants/ash_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace policy::local_auth_factors {

namespace {

using Complexity = ash::LocalAuthFactorsComplexity;

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
  bool has_digit = std::ranges::any_of(password, absl::ascii_isdigit);
  bool has_lower = std::ranges::any_of(password, absl::ascii_islower);
  bool has_upper = std::ranges::any_of(password, absl::ascii_isupper);
  bool has_symbol = std::ranges::any_of(password, absl::ascii_ispunct);
  size_t different_classes = has_digit + has_lower + has_upper + has_symbol;

  switch (complexity) {
    case Complexity::kNone:
      return password.length() >= 1;

    case Complexity::kLow:
      // The password must contain alphabetic (or symbol) characters (ie. must
      // not be a "PIN").
      return password.length() >= 6 && (has_lower || has_upper || has_symbol);

    case Complexity::kMedium:
      // The password must contain at least two different sets of characters.
      return password.length() >= 8 && different_classes >= 2;

    case Complexity::kHigh:
      // The password must contain all four different sets of characters.
      return password.length() >= 12 && different_classes == 4;
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
