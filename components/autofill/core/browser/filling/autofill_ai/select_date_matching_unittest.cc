// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/autofill_ai/select_date_matching.h"

#include <ostream>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Optional;

Matcher<SelectOption> IsOption(const SelectOption& option) {
  return AllOf(Field("value", &SelectOption::value, option.value),
               Field("text", &SelectOption::text, option.text));
}

Matcher<DatePartRange> IsDatePartRange(base::span<const SelectOption> options,
                                       uint32_t first_value) {
  std::vector<Matcher<SelectOption>> matchers;
  matchers.reserve(options.size());
  for (const SelectOption& option : options) {
    matchers.push_back(IsOption(option));
  }
  return AllOf(
      Field("options", &DatePartRange::options, ElementsAreArray(matchers)),
      Field("first_value", &DatePartRange::first_value, first_value));
}

Matcher<DatePartRange> IsEmptyDatePartRange() {
  return AllOf(Field("options", &DatePartRange::options, IsEmpty()),
               Field("first_value", &DatePartRange::first_value,
                     std::numeric_limits<uint32_t>::max()));
}

// Creates a range containing [min, min+1, ..., max] (both inclusive).
std::vector<std::string> Range(int min, int max) {
  std::vector<std::string> v;
  if (min <= max) {
    v.reserve(max - min + 1);
    while (min <= max) {
      v.push_back(base::NumberToString(min++));
    }
  }
  return v;
}

// Repeats `s` `num` times.
std::vector<std::string> Repeat(std::string s, int num) {
  return std::vector<std::string>(num, s);
}

// Helper for assembling std::vector<SelectOption>.
struct OptionsBuilder {
  static OptionsBuilder Values(std::vector<std::string> v) {
    return {.values = std::move(v)};
  }

  static OptionsBuilder Texts(std::vector<std::string> v) {
    return {.texts = std::move(v)};
  }

  std::vector<SelectOption> ToSelectOptions() const {
    std::vector<SelectOption> options;
    for (size_t i = 0; i < std::max(values.size(), texts.size()); ++i) {
      options.push_back(
          {.value = i < values.size() ? base::UTF8ToUTF16(values[i]) : u"",
           .text = i < texts.size() ? base::UTF8ToUTF16(texts[i]) : u""});
    }
    return options;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator std::vector<SelectOption>() const { return ToSelectOptions(); }

  // Concatenates the option ranges of `lhs` and `rhs`.
  [[nodiscard]] friend OptionsBuilder operator+(OptionsBuilder lhs,
                                                OptionsBuilder rhs) {
    lhs.FillUp();
    rhs.FillUp();
    lhs.values.insert(lhs.values.end(),
                      std::make_move_iterator(rhs.values.begin()),
                      std::make_move_iterator(rhs.values.end()));
    lhs.texts.insert(lhs.texts.end(),
                     std::make_move_iterator(rhs.texts.begin()),
                     std::make_move_iterator(rhs.texts.end()));
    return lhs;
  }

  // Concatenates the pairwise values and texts of `lhs` and `rhs`.
  [[nodiscard]] friend OptionsBuilder operator|(OptionsBuilder lhs,
                                                OptionsBuilder rhs) {
    size_t n = std::max(std::max(lhs.texts.size(), rhs.texts.size()),
                        std::max(lhs.values.size(), rhs.values.size()));
    lhs.values.resize(n);
    rhs.values.resize(n);
    lhs.texts.resize(n);
    rhs.texts.resize(n);
    for (size_t i = 0; i < lhs.texts.size(); ++i) {
      lhs.values[i] += rhs.values[i];
      lhs.texts[i] += rhs.texts[i];
    }
    return lhs;
  }

  [[nodiscard]] OptionsBuilder WithPadding(size_t width,
                                           char padding = '0') && {
    auto pad = [width, padding](std::string& s) {
      if (0 < s.size() && s.size() < width &&
          std::ranges::all_of(s, &base::IsAsciiDigit<char>)) {
        s = std::string(width - s.size(), padding) + s;
      }
    };
    for (std::string& value : values) {
      pad(value);
    }
    for (std::string& text : texts) {
      pad(text);
    }
    return *this;
  }

  // Fills the shorter of `values` or `texts` with empty strings.
  void FillUp() {
    values.resize(std::max(values.size(), texts.size()));
    texts.resize(std::max(values.size(), texts.size()));
  }

  std::vector<std::string> values;
  std::vector<std::string> texts;
};

// Convenience function to avoid explicit conversions to span.
base::span<const SelectOption> subspan(base::span<const SelectOption> span,
                                       size_t offset) {
  return span.subspan(offset);
}

base::span<const SelectOption> subspan(base::span<const SelectOption> span,
                                       size_t offset,
                                       size_t length) {
  return span.subspan(offset, length);
}

class SelectDateMatchingTest : public testing::Test {
 public:
  using OB = OptionsBuilder;
};

// Year tests.

TEST_F(SelectDateMatchingTest, YearValueNumbersYYYY) {
  std::vector<SelectOption> options = OB::Values(Range(2000, 2099));
  EXPECT_THAT(GetYearRange(options), IsDatePartRange(options, 2000));
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, YearValueNumbersShortYYYY) {
  std::vector<SelectOption> options = OB::Values(Range(2025, 2030));
  EXPECT_THAT(GetYearRange(options), IsDatePartRange(options, 2025));
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, YearTextNumbersYYYY) {
  std::vector<SelectOption> options =
      OB::Texts(Repeat("Year ", 100)) | OB::Texts(Range(2000, 2099));
  EXPECT_THAT(GetYearRange(options), IsDatePartRange(options, 2000));
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, YearValueNumbersYY) {
  std::vector<SelectOption> options = OB::Values(Range(0, 99));
  EXPECT_THAT(GetYearRange(options), IsDatePartRange(options, 2000));
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, YearValueNumbersWithPaddingYY) {
  std::vector<SelectOption> options = OB::Values(Range(0, 99)).WithPadding(0);
  EXPECT_THAT(GetYearRange(options), IsDatePartRange(options, 2000));
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, YearValueNumbersShortYY) {
  std::vector<SelectOption> options = OB::Values(Range(25, 30));
  EXPECT_THAT(GetYearRange(options), IsDatePartRange(options, 2025));
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, YearTextNumbersYY) {
  std::vector<SelectOption> options =
      OB::Texts(Repeat("Year ", 100)) | OB::Texts(Range(0, 99)).WithPadding(2);
  EXPECT_THAT(GetYearRange(options), IsDatePartRange(options, 2000));
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, YearTextAndValueNumbersWithPrefix) {
  std::vector<SelectOption> options = OB::Texts(Repeat("Year ", 100)) |
                                      OB::Texts(Range(2000, 2099)) |
                                      OB::Values(Repeat("year-", 100)) |
                                      OB::Values(Range(0, 99)).WithPadding(2);
  EXPECT_THAT(GetYearRange(options), IsDatePartRange(options, 2000));
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest,
       YearTextAndValueNumbersWithPrefixAndSuffixAndHeaderAndFooter) {
  std::vector<SelectOption> options =
      OB::Texts({"Pick your favourite year"}) +
      ((OB::Texts(Repeat("Jahr ", 100)) | OB::Texts(Range(2000, 2099)) |
        OB::Texts(Repeat(" n. Chr.", 100))) |
       (OB::Values(Repeat("year-", 100)) |
        OB::Values(Range(0, 99)).WithPadding(2) |
        OB::Texts(Repeat("-ad", 100)))) +
      OB::Texts({"Pick one above!"});
  EXPECT_THAT(GetYearRange(options),
              IsDatePartRange(subspan(options, 1, 100), 2000));
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, YearValueTooFewNumbers) {
  std::vector<SelectOption> options = OB::Texts(Range(2000, 2002));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
}

// Month tests.

TEST_F(SelectDateMatchingTest, MonthValueNumbers) {
  std::vector<SelectOption> options = OB::Values(Range(1, 12));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsDatePartRange(options, 1));
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthValueNumbersWithPadding) {
  std::vector<SelectOption> options = OB::Values(Range(1, 12)).WithPadding(2);
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsDatePartRange(options, 1));
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthValueNumbersZeroOffset) {
  std::vector<SelectOption> options = OB::Values(Range(0, 11));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsDatePartRange(options, 1));
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthValueTooFewNumbers) {
  std::vector<SelectOption> options = OB::Texts(Range(1, 11));
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthValueTooManyNumbers) {
  std::vector<SelectOption> options = OB::Texts(Range(1, 13));
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthValueTooManyNumbersZeroOffset) {
  std::vector<SelectOption> options = OB::Texts(Range(0, 12));
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthValueShiftedNumbers) {
  std::vector<SelectOption> options = OB::Texts(Range(3, 15));
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthTextNumbers) {
  std::vector<SelectOption> options = OB::Texts(Range(1, 12));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsDatePartRange(options, 1));
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthTextNumbersWithPrefix) {
  std::vector<SelectOption> options =
      OB::Texts(Repeat("Month ", 12)) | OB::Texts(Range(1, 12));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsDatePartRange(options, 1));
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthTextNumbersWithHeader) {
  std::vector<SelectOption> options =
      OB::Texts({"Pick your month"}) + OB::Texts(Range(1, 12));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsDatePartRange(subspan(options, 1), 1));
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthTextNumbersWithFooter) {
  std::vector<SelectOption> options =
      OB::Texts(Range(1, 12)) + OB::Texts({"No month picked"});
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options),
              IsDatePartRange(subspan(options, 0, 12), 1));
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthTextNumbersWithHeaderAndFooter) {
  std::vector<SelectOption> options = OB::Texts({"Pick your month"}) +
                                      OB::Texts(Range(1, 12)) +
                                      OB::Texts({"No month picked"});
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options),
              IsDatePartRange(subspan(options, 1, 12), 1));
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthWinsAgainstYearYY) {
  std::vector<SelectOption> options =
      OB::Texts(Range(1, 12)) | OB::Values(Range(12, 24));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options),
              IsDatePartRange(subspan(options, 0, 12), 1));
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthWinsAgainstYearYYYY) {
  std::vector<SelectOption> options =
      OB::Texts(Range(1, 12)) | OB::Values(Range(2001, 2012));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsDatePartRange(options, 1));
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthTextString) {
  std::vector<SelectOption> options =
      OB::Texts({"January", "February", "March", "April", "May", "June", "July",
                 "August", "September", "October", "November", "December"});
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsDatePartRange(options, 1));
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, MonthTextStringAbbreviations) {
  std::vector<SelectOption> options =
      OB::Texts({"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
                 "Oct", "Nov", "Dec"});
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsDatePartRange(options, 1));
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

// Day tests.

TEST_F(SelectDateMatchingTest, DayValueNumbers28) {
  std::vector<SelectOption> options = OB::Values(Range(1, 28));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsDatePartRange(options, 1));
}

TEST_F(SelectDateMatchingTest, DayValueNumbers29) {
  std::vector<SelectOption> options = OB::Values(Range(1, 29));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsDatePartRange(options, 1));
}

TEST_F(SelectDateMatchingTest, DayValueNumbers30) {
  std::vector<SelectOption> options = OB::Values(Range(1, 30));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsDatePartRange(options, 1));
}

TEST_F(SelectDateMatchingTest, DayValueNumbers31) {
  std::vector<SelectOption> options = OB::Values(Range(1, 31));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsDatePartRange(options, 1));
}

TEST_F(SelectDateMatchingTest, DayValueNumbersZeroOffset) {
  std::vector<SelectOption> options = OB::Values(Range(0, 30));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsDatePartRange(options, 1));
}

TEST_F(SelectDateMatchingTest, DayValueShiftedNumbers) {
  std::vector<SelectOption> options = OB::Texts(Range(10, 41));
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, DayValueTooFewNumbers) {
  std::vector<SelectOption> options =
      OB::Values({"Foo"}) + OB::Values(Range(1, 27)) + OB::Values({"Bar"});
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, DayValueTooManyNumbers) {
  std::vector<SelectOption> options = OB::Values(Range(1, 32));
  EXPECT_THAT(GetDayRange(options), IsEmptyDatePartRange());
}

TEST_F(SelectDateMatchingTest, DayTextNumbers) {
  std::vector<SelectOption> options = OB::Texts(Range(1, 31));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsDatePartRange(options, 1));
}

TEST_F(SelectDateMatchingTest, DayTextNumbersWithPrefix) {
  std::vector<SelectOption> options =
      OB::Texts(Repeat("Day ", 31)) | OB::Texts(Range(1, 31));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsDatePartRange(options, 1));
}

TEST_F(SelectDateMatchingTest, DayTextNumbersWithHeader) {
  std::vector<SelectOption> options =
      OB::Texts({"Pick your day"}) + OB::Texts(Range(1, 31));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsDatePartRange(subspan(options, 1), 1));
}

TEST_F(SelectDateMatchingTest, DayTextNumbersWithFooter) {
  std::vector<SelectOption> options =
      OB::Texts(Range(1, 31)) + OB::Texts({"No day picked"});
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options),
              IsDatePartRange(subspan(options, 0, 31), 1));
}

TEST_F(SelectDateMatchingTest, DayTextNumbersWithHeaderAndFooter) {
  std::vector<SelectOption> options = OB::Texts({"Pick your day of February"}) +
                                      OB::Texts(Range(1, 28)) +
                                      OB::Texts({"No day picked"});
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options),
              IsDatePartRange(subspan(options, 1, 28), 1));
}

TEST_F(SelectDateMatchingTest, DayValueMultipleSequences) {
  std::vector<SelectOption> options = OB::Values(Range(1, 3)) +
                                      OB::Values(Range(1, 28)) +
                                      OB::Values(Range(1, 2));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options),
              IsDatePartRange(subspan(options, 3, 28), 1));
}

TEST_F(SelectDateMatchingTest, DayWinsAgainstYearYY) {
  std::vector<SelectOption> options =
      OB::Texts(Range(1, 31)) | OB::Values(Range(11, 41));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsDatePartRange(options, 1));
}

TEST_F(SelectDateMatchingTest, DayWinsAgainstYearYYYY) {
  std::vector<SelectOption> options =
      OB::Texts(Range(1, 31)) | OB::Values(Range(2001, 2031));
  EXPECT_THAT(GetYearRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetMonthRange(options), IsEmptyDatePartRange());
  EXPECT_THAT(GetDayRange(options), IsDatePartRange(options, 1));
}

TEST_F(SelectDateMatchingTest, GetByValue) {
  std::vector<SelectOption> years_yyyy = OB::Texts(Range(2001, 2031));
  EXPECT_THAT(GetYearRange(years_yyyy).get_by_value(2025),
              Optional(IsOption({.text = u"2025"})));

  std::vector<SelectOption> years_yy = OB::Values(Range(1, 99)).WithPadding(2);
  EXPECT_THAT(GetYearRange(years_yy).get_by_value(2025),
              Optional(IsOption({.value = u"25"})));

  std::vector<SelectOption> months =
      OB::Texts({"Pick your month"}) + OB::Texts(Range(1, 12)).WithPadding(2);
  EXPECT_THAT(GetMonthRange(months).get_by_value(3),
              Optional(IsOption({.text = u"03"})));

  std::vector<SelectOption> days = OB::Values(Range(1, 31));
  EXPECT_THAT(GetDayRange(days).get_by_value(28),
              Optional(IsOption({.value = u"28"})));
}

}  // namespace
}  // namespace autofill
