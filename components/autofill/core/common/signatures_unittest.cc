// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

TEST(SignaturesTest, StripDigits) {
  FormData actual_form;
  actual_form.url = GURL("http://foo.com");
  actual_form.name = u"form_name_12345";

  FormFieldData field1;
  field1.form_control_type = "text";
  field1.name = u"field_name_12345";
  actual_form.fields.push_back(field1);

  FormFieldData field2;
  field2.form_control_type = "text";
  field2.name = u"field_name_1234";
  actual_form.fields.push_back(field2);

  // Sequences of 5 digits or longer should be stripped.
  FormData expected_form(actual_form);
  expected_form.name = u"form_name_";
  expected_form.fields[0].name = u"field_name_";

  EXPECT_EQ(CalculateFormSignature(expected_form).value(),
            CalculateFormSignature(actual_form).value());
  EXPECT_EQ(
      StrToHash64Bit("http://foo.com&form_name_&field_name_&field_name_1234"),
      CalculateFormSignature(actual_form).value());
}

}  // namespace autofill
