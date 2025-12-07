// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/password_manager/password_manager_autofill_helper.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(PasswordManagerAutofillHelperTest, IsOtpFilledField) {
  FormFieldData field_data;
  AutofillField field(field_data);

  // Not autofilled.
  EXPECT_FALSE(PasswordManagerAutofillHelper::IsOtpFilledField(field));

  // Autofilled, but not with OTP.
  field.set_is_autofilled(true);
  field.set_filling_product(FillingProduct::kAddress);
  EXPECT_FALSE(PasswordManagerAutofillHelper::IsOtpFilledField(field));

  // Autofilled with OTP.
  field.set_filling_product(FillingProduct::kOneTimePassword);
  EXPECT_TRUE(PasswordManagerAutofillHelper::IsOtpFilledField(field));

  // Previously autofilled with OTP, but corrected by the user.
  field.set_is_autofilled(false);
  EXPECT_FALSE(PasswordManagerAutofillHelper::IsOtpFilledField(field));
}

}  // namespace autofill
