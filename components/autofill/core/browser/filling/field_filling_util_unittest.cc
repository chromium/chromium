// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/field_filling_util.h"

#include <optional>
#include <string>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

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

  std::u16string match_value =
      GetSelectControlValue(u"Meenie", field.options(),
                            /*failure_to_fill=*/nullptr)
          .value_or(u"");
  EXPECT_EQ(u"Meenie", match_value);
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

  std::u16string match_value =
      GetSelectControlValue(u"Miney", field.options(),
                            /*failure_to_fill=*/nullptr)
          .value_or(u"");
  EXPECT_EQ(u"2", match_value);
}

TEST(GetObfuscatedValue, ObfuscateValue) {
  EXPECT_EQ(GetObfuscatedValue(u"12"),
            u"\u2022\u2060\u2006\u2060\u2022\u2060\u2006\u2060");
}

}  // namespace autofill
