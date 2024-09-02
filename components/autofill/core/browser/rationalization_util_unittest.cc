// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/rationalization_util.h"

#include <memory>
#include <tuple>
#include <vector>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

struct FieldTemplate {
  // Description of the field passed to the rationalization.
  autofill::FieldType type;

  // Expectation of field after rationalization.
  bool only_fill_when_focused;
};

// Returns a tuple of test input and expectations.
// The input is the vector of fields. The expectations indicate whether
// the fields will have the only_fill_when_focused flag set to true.
std::tuple<std::vector<std::unique_ptr<AutofillField>>, std::vector<bool>>
CreateTest(std::vector<FieldTemplate> field_templates) {
  std::vector<std::unique_ptr<AutofillField>> fields;
  for (const auto& f : field_templates) {
    fields.push_back(std::make_unique<AutofillField>());
    fields.back()->SetTypeTo(AutofillType(f.type));
  }

  std::vector<bool> expected_only_fill_when_focused;
  for (const auto& f : field_templates)
    expected_only_fill_when_focused.push_back(f.only_fill_when_focused);

  return std::make_tuple(std::move(fields),
                         std::move(expected_only_fill_when_focused));
}

std::vector<AutofillField*> ToPointers(
    std::vector<std::unique_ptr<AutofillField>>& fields) {
  std::vector<AutofillField*> result;
  for (const auto& f : fields)
    result.push_back(f.get());
  return result;
}

std::vector<bool> GetOnlyFilledWhenFocused(
    const std::vector<std::unique_ptr<AutofillField>>& fields) {
  std::vector<bool> result;
  for (const auto& f : fields)
    result.push_back(f->only_fill_when_focused());
  return result;
}

TEST(AutofillRationalizationUtilTest, PhoneNumber_FirstNumberIsWholeNumber) {
  auto [fields, expected_only_fill_when_focused] =
      CreateTest({{NAME_FULL, false},
                  {ADDRESS_HOME_LINE1, false},
                  {PHONE_HOME_WHOLE_NUMBER, false},
                  {PHONE_HOME_CITY_AND_NUMBER, true}});
  rationalization_util::RationalizePhoneNumberFields(ToPointers(fields));
  EXPECT_THAT(GetOnlyFilledWhenFocused(fields),
              ::testing::Eq(expected_only_fill_when_focused));
}

TEST(AutofillRationalizationUtilTest, PhoneNumber_FirstNumberIsComponentized) {
  auto [fields, expected_only_fill_when_focused] =
      CreateTest({{NAME_FULL, false},
                  {ADDRESS_HOME_LINE1, false},
                  {PHONE_HOME_COUNTRY_CODE, false},
                  {PHONE_HOME_CITY_CODE, false},
                  {PHONE_HOME_NUMBER, false},
                  {PHONE_HOME_COUNTRY_CODE, true},
                  {PHONE_HOME_CITY_CODE, true},
                  {PHONE_HOME_NUMBER, true}});
  rationalization_util::RationalizePhoneNumberFields(ToPointers(fields));
  EXPECT_THAT(GetOnlyFilledWhenFocused(fields),
              ::testing::Eq(expected_only_fill_when_focused));
}

TEST(AutofillRationalizationUtilTest,
     PhoneNumber_BestEffortWhenNoCompleteNumberIsFound) {
  auto [fields, expected_only_fill_when_focused] =
      CreateTest({{NAME_FULL, false},
                  {ADDRESS_HOME_LINE1, false},
                  {PHONE_HOME_COUNTRY_CODE, false},
                  {PHONE_HOME_CITY_CODE, false}});
  // Even though we did not find the PHONE_HOME_NUMBER finishing the phone
  // number, the remaining fields are filled.
  rationalization_util::RationalizePhoneNumberFields(ToPointers(fields));
  EXPECT_THAT(GetOnlyFilledWhenFocused(fields),
              ::testing::Eq(expected_only_fill_when_focused));
}

TEST(AutofillRationalizationUtilTest, PhoneNumber_FillPhonePartsOnceOnly) {
  auto [fields, expected_only_fill_when_focused] =
      CreateTest({{NAME_FULL, false},
                  {ADDRESS_HOME_LINE1, false},
                  {PHONE_HOME_COUNTRY_CODE, false},
                  {PHONE_HOME_CITY_CODE, false},
                  {PHONE_HOME_NUMBER, false},
                  // The following represent a second number and an incomplete
                  // third number that are not filled.
                  {PHONE_HOME_WHOLE_NUMBER, true},
                  {PHONE_HOME_CITY_CODE, true}});
  rationalization_util::RationalizePhoneNumberFields(ToPointers(fields));
  EXPECT_THAT(GetOnlyFilledWhenFocused(fields),
              ::testing::Eq(expected_only_fill_when_focused));
}

TEST(AutofillRationalizationUtilTest, PhoneNumber_SkipHiddenPhoneNumberFields) {
  auto [fields, expected_only_fill_when_focused] =
      CreateTest({{NAME_FULL, false},
                  {ADDRESS_HOME_LINE1, false},
                  // This one is not focusable (e.g. hidden) and does not get
                  // filled for that reason.
                  {PHONE_HOME_CITY_AND_NUMBER, true},
                  {PHONE_HOME_WHOLE_NUMBER, false}});
  // With the `kAutofillUseParameterizedSectioning` `!FormFieldData::is_visible`
  // fields are skipped.
  fields[2]->set_is_visible(false);
  fields[2]->set_is_focusable(false);
  rationalization_util::RationalizePhoneNumberFields(ToPointers(fields));
  EXPECT_THAT(GetOnlyFilledWhenFocused(fields),
              ::testing::Eq(expected_only_fill_when_focused));
}

TEST(AutofillRationalizationUtilTest,
     PhoneNumber_ProcessNumberPrefixAndSuffix) {
  auto [fields, expected_only_fill_when_focused] =
      CreateTest({{NAME_FULL, false},
                  {ADDRESS_HOME_LINE1, false},
                  {PHONE_HOME_CITY_CODE, false},
                  {PHONE_HOME_NUMBER_PREFIX, false},
                  {PHONE_HOME_NUMBER_SUFFIX, false},
                  // This would be a second number.
                  {PHONE_HOME_CITY_CODE, true},
                  {PHONE_HOME_NUMBER_PREFIX, true},
                  {PHONE_HOME_NUMBER_SUFFIX, true}});
  rationalization_util::RationalizePhoneNumberFields(ToPointers(fields));
  EXPECT_THAT(GetOnlyFilledWhenFocused(fields),
              ::testing::Eq(expected_only_fill_when_focused));
}

TEST(AutofillRationalizationUtilTest, PhoneNumber_IncorrectPrefix) {
  auto [fields, expected_only_fill_when_focused] =
      CreateTest({{NAME_FULL, false},
                  {ADDRESS_HOME_LINE1, false},
                  // Let's assume this field was incorrectly classified as a
                  // prefix and there is no suffix but a local phone number.
                  {PHONE_HOME_NUMBER_PREFIX, true},
                  {PHONE_HOME_CITY_CODE, false},
                  {PHONE_HOME_NUMBER, false},
                  // This would be a second number.
                  {PHONE_HOME_CITY_AND_NUMBER, true}});
  rationalization_util::RationalizePhoneNumberFields(ToPointers(fields));
  EXPECT_THAT(GetOnlyFilledWhenFocused(fields),
              ::testing::Eq(expected_only_fill_when_focused));
}

TEST(AutofillRationalizationUtilTest, PhoneNumber_IncorrectSuffix) {
  auto [fields, expected_only_fill_when_focused] =
      CreateTest({{NAME_FULL, false},
                  {ADDRESS_HOME_LINE1, false},
                  // Let's assume this field was incorrectly classified as a
                  // suffix and there is no prefix but a local phone number.
                  {PHONE_HOME_NUMBER_SUFFIX, true},
                  {PHONE_HOME_CITY_CODE, false},
                  {PHONE_HOME_NUMBER, false},
                  // This would be a second number.
                  {PHONE_HOME_CITY_AND_NUMBER, true}});
  rationalization_util::RationalizePhoneNumberFields(ToPointers(fields));
  EXPECT_THAT(GetOnlyFilledWhenFocused(fields),
              ::testing::Eq(expected_only_fill_when_focused));
}

}  // namespace
}  // namespace autofill
