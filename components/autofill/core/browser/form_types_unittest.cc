// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_types.h"

#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using autofill::FieldType;
using std::string;

struct FormTypesTestCase {
  std::vector<FieldType> field_types;
  std::vector<std::u16string> field_values;
  bool expected_result;
};

autofill::FormFieldData CreateFieldWithValue(std::u16string value) {
  FormFieldData field;
  field.set_value(value);
  return field;
}

class FormTypesTest : public testing::TestWithParam<FormTypesTestCase> {
 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

TEST_P(FormTypesTest, FormHasFillableCreditCardFields) {
  FormTypesTestCase test_case = GetParam();

  std::vector<FormFieldData> fields;
  for (const auto& value : test_case.field_values) {
    fields.push_back(CreateFieldWithValue(value));
  }
  FormData form;
  form.set_fields(std::move(fields));
  FormStructure form_structure(form);
  test_api(form_structure).SetFieldTypes(test_case.field_types);

  EXPECT_THAT(FormHasAllCreditCardFields(form_structure),
              testing::Eq(test_case.expected_result));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FormTypesTest,
    testing::Values(FormTypesTestCase{{CREDIT_CARD_NUMBER,
                                       CREDIT_CARD_EXP_MONTH,
                                       CREDIT_CARD_EXP_2_DIGIT_YEAR},
                                      {u"", u"", u""},
                                      true},
                    FormTypesTestCase{{CREDIT_CARD_NUMBER}, {u""}, false}));

}  // namespace autofill
