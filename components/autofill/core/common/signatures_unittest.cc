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
  field1.form_control_type = FormControlType::kInputText;
  field1.name = u"field_name_12345";
  actual_form.fields.push_back(field1);

  FormFieldData field2;
  field2.form_control_type = FormControlType::kInputText;
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

TEST(SignaturesTest, AlternativeFormSignatureLarge) {
  FormData large_form;
  large_form.url = GURL("http://foo.com/login?q=a#ref");

  FormFieldData field1;
  field1.form_control_type = FormControlType::kInputText;
  large_form.fields.push_back(field1);

  FormFieldData field2;
  field2.form_control_type = FormControlType::kInputText;
  large_form.fields.push_back(field2);

  FormFieldData field3;
  field3.form_control_type = FormControlType::kInputEmail;
  large_form.fields.push_back(field3);

  FormFieldData field4;
  field4.form_control_type = FormControlType::kInputTelephone;
  large_form.fields.push_back(field4);

  // Alternative form signature string of a form with more than two fields
  // should only concatenate scheme, host, and field types.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&text&email&tel"),
            CalculateAlternativeFormSignature(large_form).value());
}

TEST(SignaturesTest, AlternativeFormSignatureSmallPath) {
  FormData small_form_path;
  small_form_path.url = GURL("http://foo.com/login?q=a#ref");

  FormFieldData field1;
  field1.form_control_type = FormControlType::kInputText;
  small_form_path.fields.push_back(field1);

  FormFieldData field2;
  field2.form_control_type = FormControlType::kInputText;
  small_form_path.fields.push_back(field2);

  // Alternative form signature string of a form with 2 fields or less should
  // concatenate scheme, host, field types, and path if it is non-empty.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&text/login"),
            CalculateAlternativeFormSignature(small_form_path).value());
}

TEST(SignaturesTest, AlternativeFormSignatureSmallRef) {
  FormData small_form_ref;
  small_form_ref.url = GURL("http://foo.com?q=a#ref");

  FormFieldData field1;
  field1.form_control_type = FormControlType::kInputText;
  small_form_ref.fields.push_back(field1);

  FormFieldData field2;
  field2.form_control_type = FormControlType::kInputText;
  small_form_ref.fields.push_back(field2);

  // Alternative form signature string of a form with 2 fields or less and
  // without a path should concatenate scheme, host, field types, and reference
  // if it is non-empty.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&text#ref"),
            CalculateAlternativeFormSignature(small_form_ref).value());
}

TEST(SignaturesTest, AlternativeFormSignatureSmallQuery) {
  FormData small_form_query;
  small_form_query.url = GURL("http://foo.com?q=a");

  FormFieldData field1;
  field1.form_control_type = FormControlType::kInputText;
  small_form_query.fields.push_back(field1);

  FormFieldData field2;
  field2.form_control_type = FormControlType::kInputText;
  small_form_query.fields.push_back(field2);

  // Alternative form signature string of a form with 2 fields or less and
  // without a path or reference should concatenate scheme, host, field types,
  // and query if it is non-empty.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&text?q=a"),
            CalculateAlternativeFormSignature(small_form_query).value());
}

}  // namespace autofill
