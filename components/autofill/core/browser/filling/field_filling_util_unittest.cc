// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/field_filling_util.h"

#include <optional>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

constexpr std::u16string kDots = u"\u2022\u2060\u2006\u2060";

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

// TODO(crbug.com/394011769): Remove once `kAutofillAiWalletPrivatePasses` is
// launched.
TEST(GetObfuscatedValue, ObfuscateValueLegacy) {
  std::u16string expected = base::StrCat({kDots, kDots});
  EXPECT_EQ(GetObfuscatedValue(u"12"), expected);
}

// TODO(crbug.com/394011769): Remove once `kAutofillAiWalletPrivatePasses` is
// launched.
TEST(GetObfuscatedValue, ObfuscateValueWithPartialLegacy) {
  // Test partial obfuscation (keep last 2 characters).
  EXPECT_EQ(GetObfuscatedValue(u"12345", 2),
            base::StrCat({kDots, kDots, kDots, u"45"}));

  // Test obfuscation of 0 characters (should return fully obfuscated).
  EXPECT_EQ(GetObfuscatedValue(u"12345", 0),
            base::StrCat({kDots, kDots, kDots, kDots, kDots}));

  // Test visible suffix are more characters than length (should not obfuscate
  // all).
  EXPECT_EQ(GetObfuscatedValue(u"12", 5), u"12");

  // Test keeping all characters.
  EXPECT_EQ(GetObfuscatedValue(u"12345", 5), u"12345");
}

TEST(GetObfuscatedValue, ObfuscateValue) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillAiWalletPrivatePasses);

  // 4 dots + up to 4 chars (visible_suffix_length = 4)
  EXPECT_EQ(GetObfuscatedValue(u"123456789", 4),
            base::StrCat({kDots, kDots, kDots, kDots, u"6789"}));

  // Shorter than 4 chars.
  EXPECT_EQ(GetObfuscatedValue(u"12", 4),
            base::StrCat({kDots, kDots, kDots, kDots, u"12"}));

  // Empty string.
  EXPECT_EQ(GetObfuscatedValue(u"", 4),
            base::StrCat({kDots, kDots, kDots, kDots}));
}

TEST(GetObfuscatedValue, ObfuscateAll) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillAiWalletPrivatePasses);

  // Matches length of UI string (4 dots + 4 visible = 8 dots).
  // visible_suffix_length = 0 means obfuscate all.
  EXPECT_EQ(
      GetObfuscatedValue(u"123456789", 0),
      base::StrCat({kDots, kDots, kDots, kDots, kDots, kDots, kDots, kDots}));

  // Short strings (4 dots + 2 visible = 6 dots).
  EXPECT_EQ(GetObfuscatedValue(u"12", 0),
            base::StrCat({kDots, kDots, kDots, kDots, kDots, kDots}));

  // Empty string.
  EXPECT_EQ(GetObfuscatedValue(u"", 0),
            base::StrCat({kDots, kDots, kDots, kDots}));
}

}  // namespace

}  // namespace autofill
