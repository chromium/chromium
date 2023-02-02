// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_types.h"

#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class FormTypesTest : public testing::Test {
 private:
  test::AutofillEnvironment autofill_environment_;
};

TEST_F(FormTypesTest, FormHasAllCreditCardFieldsReturnsTrue) {
  FormData form;
  form.fields.resize(3);
  FormStructure form_structure(form);
  FormStructureTestApi(&form_structure)
      .SetFieldTypes({CREDIT_CARD_NUMBER, CREDIT_CARD_EXP_MONTH,
                      CREDIT_CARD_EXP_2_DIGIT_YEAR});

  EXPECT_TRUE(FormHasAllCreditCardFields(form_structure));
}

TEST_F(FormTypesTest, FormHasAllCreditCardFieldsReturnsFalse) {
  FormData incomplete_form;
  incomplete_form.fields.resize(1);
  FormStructure form_structure(incomplete_form);
  FormStructureTestApi(&form_structure).SetFieldTypes({CREDIT_CARD_NUMBER});

  EXPECT_FALSE(FormHasAllCreditCardFields(form_structure));
}

}  // namespace autofill
