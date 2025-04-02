// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_SELECT_DATE_MATCHING_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_SELECT_DATE_MATCHING_H_

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "base/memory/stack_allocated.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// `DatePartRange` and `Get{Year,Month,Day}Range()` scan <option>s for elements
// that look like parts of days.
//
// The functions are mutually exclusive: for given `options`, at most one of
// `Get{Year,Month,Day}Range(options)` returns a non-empty range.
//
// The matching is heuristical. The main ingredient is looking for sequences of
// options whose values increase by 1 in every step. For example, if we find
// such a sequence that has length 31 and starts at 1 or 0, this is considered a
// month. For further details,  see the implementations.

// Represents a sequence of SelectOptions that represent years, months, or
// days.
struct DatePartRange {
  STACK_ALLOCATED();

 public:
  // Returns the SelectOption that represents a normalized `value`.
  //
  // For example, if `options.size() == 50` and `value_offset == 2025`, this
  // range represents the years 2025, 2026, ..., 2075, and `get_by_value(2026)`
  // returns the `options[1]`.
  base::optional_ref<const SelectOption> get_by_value(uint32_t value) const {
    if (value < first_value || value >= first_value + options.size()) {
      return std::nullopt;
    }
    return options[value - first_value];
  }

  // A subspan of a field's options that has represents a sequence of years,
  // months, or days..
  base::span<const SelectOption> options;

  // A numerical representation of `options.front()`.
  // For day ranges, it is 1.
  // For month ranges, it is 1.
  // For year ranges, it is in YYYY format. That is, if `options.front()` is
  // `SelectOption{.text = u"2025"}` or `SelectOption{.text = u"25"}`, then
  // in both cases it is 2025.
  uint32_t first_value = std::numeric_limits<uint32_t>::max();
};

// Returns a subspan of `options` that represents years.
// The span is either empty or has at least 3 elements and its `first_value` is
// a four-digit representation of a year.
DatePartRange GetYearRange(
    base::span<const SelectOption> options LIFETIME_BOUND);

// Returns a subspan of `options` that represents days.
// The span is either empty or has 28 to 31 elements and its `first_value` is 1.
DatePartRange GetDayRange(
    base::span<const SelectOption> options LIFETIME_BOUND);

// Returns a subspan of `options` that represents months.
// The span is either empty or 12 elements and its `first_value` is 1.
DatePartRange GetMonthRange(
    base::span<const SelectOption> options LIFETIME_BOUND);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_SELECT_DATE_MATCHING_H_
