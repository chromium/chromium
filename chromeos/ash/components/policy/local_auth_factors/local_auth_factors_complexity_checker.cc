// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/local_auth_factors/local_auth_factors_complexity_checker.h"

#include <algorithm>
#include <string_view>

#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace policy {

namespace {

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

// static
bool LocalAuthFactorsComplexityChecker::CheckPasswordComplexity(
    std::string_view password) {
  return true;
}

// static
bool LocalAuthFactorsComplexityChecker::CheckPinComplexity(
    std::string_view pin,
    LocalAuthFactorsComplexity complexity) {
  // Check that the pin contains only digits.
  if (!std::ranges::all_of(pin, absl::ascii_isdigit)) {
    return false;
  }

  switch (complexity) {
    case LocalAuthFactorsComplexity::kNone:
      return pin.length() >= 1;

    case LocalAuthFactorsComplexity::kLow:
      return pin.length() >= 4;

    case LocalAuthFactorsComplexity::kMedium:
      return pin.length() >= 6 && !ContainsOrderedOrRepeatingSequence(pin);

    case LocalAuthFactorsComplexity::kHigh:
      return pin.length() >= 8 && !ContainsOrderedOrRepeatingSequence(pin);
  }
}

}  // namespace policy
