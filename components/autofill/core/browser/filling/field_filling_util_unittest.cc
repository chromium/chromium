// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/field_filling_util.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

constexpr std::u16string_view kDots = u"\u2022\u2060\u2006\u2060";

class FieldFillingUtilTest : public testing::Test {
 public:
  FieldFillingUtilTest() = default;
  FieldFillingUtilTest(const FieldFillingUtilTest&) = delete;
  FieldFillingUtilTest& operator=(const FieldFillingUtilTest&) = delete;

  AutofillField CreateTestSelectAutofillField(
      const std::vector<const char*>& values,
      FieldType heuristic_type) {
    AutofillField field{test::CreateTestSelectField(values)};
    field.set_heuristic_type(GetActiveHeuristicSource(), heuristic_type);
    return field;
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

TEST_F(FieldFillingUtilTest, GetSelectControlByValue) {
  std::vector<const char*> kOptions = {
      "Eenie",
      "Meenie",
      "Miney",
      "Mo",
  };

  AutofillField field = CreateTestSelectAutofillField(kOptions, NAME_FIRST);

  // Set semantically empty contents for each option, so that only the values
  // can be used for matching.
  std::vector<SelectOption> options = field.options();
  for (size_t i = 0; i < field.options().size(); ++i) {
    options[i].text = base::NumberToString16(i);
  }
  field.set_options(std::move(options));

  std::optional<SelectOption> match_option =
      GetSelectControlOption(u"Meenie", field.options(),
                             /*failure_to_fill=*/nullptr);
  ASSERT_TRUE(match_option.has_value());
  EXPECT_EQ(u"Meenie", match_option->value);
}

TEST_F(FieldFillingUtilTest, GetSelectControlByContents) {
  std::vector<const char*> kOptions = {
      "Eenie",
      "Meenie",
      "Miney",
      "Mo",
  };
  AutofillField field = CreateTestSelectAutofillField(kOptions, NAME_FIRST);

  // Set semantically empty values for each option, so that only the contents
  // can be used for matching.
  std::vector<SelectOption> options = field.options();
  for (size_t i = 0; i < field.options().size(); ++i) {
    options[i].value = base::NumberToString16(i);
  }
  field.set_options(std::move(options));

  std::optional<SelectOption> match_option =
      GetSelectControlOption(u"Miney", field.options(),
                             /*failure_to_fill=*/nullptr);
  ASSERT_TRUE(match_option.has_value());
  EXPECT_EQ(u"2", match_option->value);
}

TEST(GetObfuscatedValue, ObfuscateValue) {
  // 4 dots + up to 4 chars (visible_suffix_length = 4)
  EXPECT_EQ(GetObfuscatedValue(u"123456789", 4),
            base::StrCat({kDots, kDots, kDots, kDots, u"6789"}));

  // Boundary case: exactly 4 characters.
  EXPECT_EQ(GetObfuscatedValue(u"1234", 4),
            base::StrCat({kDots, kDots, kDots, kDots, u"1234"}));

  // Shorter than 4 chars pads with extra dots so total length is always 8.
  EXPECT_EQ(GetObfuscatedValue(u"12", 4),
            base::StrCat({kDots, kDots, kDots, kDots, kDots, kDots, u"12"}));

  // Empty string returns 8 dots.
  EXPECT_EQ(
      GetObfuscatedValue(u"", 4),
      base::StrCat({kDots, kDots, kDots, kDots, kDots, kDots, kDots, kDots}));

  // Partial visible suffix (e.g. 2 chars) pads with 6 dots so total is 8.
  EXPECT_EQ(GetObfuscatedValue(u"12345", 2),
            base::StrCat({kDots, kDots, kDots, kDots, kDots, kDots, u"45"}));

  // Requests for more than 4 visible characters clamp to 4.
  EXPECT_EQ(GetObfuscatedValue(u"123456789", 6),
            base::StrCat({kDots, kDots, kDots, kDots, u"6789"}));
}

TEST(GetObfuscatedValue, ObfuscateAll) {
  // Default argument (visible_suffix_length = 0) returns 8 dots.
  EXPECT_EQ(
      GetObfuscatedValue(u"123456789"),
      base::StrCat({kDots, kDots, kDots, kDots, kDots, kDots, kDots, kDots}));

  // Explicit visible_suffix_length = 0 returns 8 dots to avoid leaking length.
  EXPECT_EQ(
      GetObfuscatedValue(u"123456789", 0),
      base::StrCat({kDots, kDots, kDots, kDots, kDots, kDots, kDots, kDots}));

  // Short strings also return 8 dots.
  EXPECT_EQ(
      GetObfuscatedValue(u"12", 0),
      base::StrCat({kDots, kDots, kDots, kDots, kDots, kDots, kDots, kDots}));

  // Empty string also returns 8 dots.
  EXPECT_EQ(
      GetObfuscatedValue(u"", 0),
      base::StrCat({kDots, kDots, kDots, kDots, kDots, kDots, kDots, kDots}));
}

}  // namespace

}  // namespace autofill
