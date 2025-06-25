// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/autofill_ai/select_date_matching.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/memory/stack_allocated.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

namespace {

// Represents a sequence of consecutive integers.
// This is not a `base::span` because we need the indices for different
// containers.
struct Sequence {
  template <typename T>
  base::span<T> subspan(base::span<T> s) const {
    return s.subspan(offset, length);
  }

  size_t offset = 0;
  size_t length = 0;
  // Whether the sequence is strictly increasing (or empty).
  bool increasing = true;
};

// Returns a longest sequence of non-negative integers in `nums` where neighbors
// differ by 1 and that is either increasing or decreasing. E.g., returns
// {.offset = 1, .length = 3} for {-1, 1, 2, 3, 5, 1, 2, 3}.
Sequence GetLongestConsecutiveNonNegativeSequence(base::span<const int> nums) {
  Sequence longest;
  Sequence current;

  auto is_consecutive = [nums](size_t index, bool check_three) {
    return std::abs(nums[index - 1] - nums[index]) == 1 &&
           (!check_three || (nums[index] - nums[index - 1] ==
                             nums[index - 1] - nums[index - 2]));
  };

  for (size_t i = 0; i < nums.size(); ++i) {
    if (nums[i] < 0) {
      continue;
    }
    if (i == 0 || nums[i - 1] < 0 ||
        !is_consecutive(i, /*check_three=*/current.length >= 2)) {
      if (longest.length < current.length) {
        longest = current;
      }
      current = {.offset = i, .length = 1};
    } else {
      ++current.length;
    }
  }
  if (longest.length < current.length) {
    longest = current;
  }

  longest.increasing =
      longest.length <= 1 || nums[longest.offset] < nums[longest.offset + 1];
  return longest;
}

// Removes the non-digit prefix and suffix. For example,
// `TrimNonDigits("foo12x3bar")` returns "12x3".
std::u16string_view TrimNonDigits(std::u16string_view s) {
  // There's no need to decode the Unicode characters: If `s.front()` is a
  // second code unit ("low surrogate"), its value does not overlap with ASCII.
  while (!s.empty() && !base::IsAsciiDigit(s.front())) {
    s = s.substr(1);
  }
  while (!s.empty() && !base::IsAsciiDigit(s.back())) {
    s = s.substr(0, s.size() - 1);
  }
  return s;
}

// Returns a vector of the same size as `option` where the `i`th integer is the
// non-negative integer found in `option` or -1 if there is none.
std::vector<int> ExtractNumbers(base::span<const SelectOption> options,
                                std::u16string SelectOption::* selector) {
  return base::ToVector(options, [&selector](const SelectOption& option) {
    int num;
    if (base::StringToInt(TrimNonDigits(option.*selector), &num) && num >= 0) {
      return num;
    }
    return -1;
  });
}

}  // namespace

DatePartRange GetYearRange(base::span<const SelectOption> options) {
  auto year_offset = [](base::span<const int> nums, Sequence seq) -> uint32_t {
    auto is_yyyy = [](int year) { return 1800 <= year && year <= 2200; };
    auto is_yy = [](int year) { return 0 <= year && year <= 99; };
    if (std::ranges::all_of(seq.subspan(nums), is_yyyy) && seq.length > 3) {
      return nums[seq.offset];
    }
    if (std::ranges::all_of(seq.subspan(nums), is_yy) && seq.length > 3 &&
        seq.length != 12 && (seq.length < 28 || seq.length > 31)) {
      return 2000 + nums[seq.offset];
    }
    return 0;
  };

  // We want Get{Year,Month,Day}Range() to be mutually exclusive. Despite the
  // checks for YY years in year_offset(), such ambiguities can happen if, for
  // example, the values are [1,...,12] and the years are [2001, ..., 2012] or
  // [00, ..., 24].
  auto is_month_or_day = [&options] {
    return !GetDayRange(options).options.empty() ||
           !GetMonthRange(options).options.empty();
  };

  {
    std::vector<int> nums = ExtractNumbers(options, &SelectOption::text);
    Sequence seq = GetLongestConsecutiveNonNegativeSequence(nums);
    uint32_t first_value = year_offset(nums, seq);
    if (first_value != 0 && !is_month_or_day()) {
      return {seq.subspan(options), first_value, seq.increasing};
    }
  }
  {
    std::vector<int> nums = ExtractNumbers(options, &SelectOption::value);
    Sequence seq = GetLongestConsecutiveNonNegativeSequence(nums);
    uint32_t first_value = year_offset(nums, seq);
    if (first_value != 0 && !is_month_or_day()) {
      return {seq.subspan(options), first_value, seq.increasing};
    }
  }
  return {};
}

DatePartRange GetMonthRange(base::span<const SelectOption> options) {
  // There are 12 months. We tolerate two additional options (e.g.,
  // "Pick month" and "Unknown").
  if (options.size() < 12 || options.size() > 14) {
    return {};
  }

  bool saw_digits = false;
  {
    // The user-visible text must contain "1" to "12".
    std::vector<int> nums = ExtractNumbers(options, &SelectOption::text);
    Sequence seq = GetLongestConsecutiveNonNegativeSequence(nums);
    if (seq.length == 12 && nums[seq.offset] == 1) {
      return {seq.subspan(options), 1};
    }
    saw_digits |= seq.length > 0;
  }
  {
    // The user-invisible values must be "1" to "12" or "0" to "11".
    std::vector<int> nums = ExtractNumbers(options, &SelectOption::value);
    Sequence seq = GetLongestConsecutiveNonNegativeSequence(nums);
    if (seq.length == 12 && nums[seq.offset] <= 1) {
      return {seq.subspan(options), 1};
    }
    saw_digits |= seq.length > 0;
  }

  // If there are no numbers, perhaps the months are spelled out.
  if (!saw_digits && options.size() == 12) {
    return {options, 1};
  }
  return {};
}

DatePartRange GetDayRange(base::span<const SelectOption> options) {
  // Months have 28 to 31 days. We tolerate two additional options (e.g.,
  // "Pick day" and "Unknown").
  if (options.size() < 28 || options.size() > 33) {
    return {};
  }

  {
    // The user-visible text must start at "1".
    std::vector<int> nums = ExtractNumbers(options, &SelectOption::text);
    Sequence seq = GetLongestConsecutiveNonNegativeSequence(nums);
    if (28 <= seq.length && seq.length <= 31 && nums[seq.offset] == 1) {
      return {seq.subspan(options), 1};
    }
  }
  {
    // The user-invisible values must start at "0" or "1".
    std::vector<int> nums = ExtractNumbers(options, &SelectOption::value);
    Sequence seq = GetLongestConsecutiveNonNegativeSequence(nums);
    if (28 <= seq.length && seq.length <= 31 && nums[seq.offset] <= 1) {
      return {seq.subspan(options), 1};
    }
  }
  return {};
}

}  // namespace autofill
