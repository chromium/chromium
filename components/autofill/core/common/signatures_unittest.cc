// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/signatures.h"

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

TEST(SignaturesTest, StripDigits) {
  FormData actual_form;
  actual_form.set_url(GURL("http://foo.com"));
  actual_form.set_name(u"form_name_12345");

  FormFieldData field1;
  field1.set_form_control_type(FormControlType::kInputText);
  field1.set_name(u"field_name_12345");
  test_api(actual_form).Append(field1);

  FormFieldData field2;
  field2.set_form_control_type(FormControlType::kInputText);
  field2.set_name(u"field_name_1234");
  test_api(actual_form).Append(field2);

  // Sequences of 5 digits or longer should be stripped.
  FormData expected_form(actual_form);
  expected_form.set_name(u"form_name_");
  test_api(expected_form).field(0).set_name(u"field_name_");

  EXPECT_EQ(CalculateFormSignature(expected_form).value(),
            CalculateFormSignature(actual_form).value());
  EXPECT_EQ(
      StrToHash64Bit("http://foo.com&form_name_&field_name_&field_name_1234"),
      CalculateFormSignature(actual_form).value());
}

// Tests that <input type={checkbox,radio,date}> do not count towards
// FormSignatures.
TEST(SignaturesTest, IgnoreCheckboxRadioDate) {
  FormData form;
  form.set_url(GURL("http://foo.com"));
  form.set_name(u"form_name");

  {
    FormFieldData field;
    field.set_form_control_type(FormControlType::kInputText);
    field.set_name(u"field1");
    test_api(form).Append(field);
  }

  {
    FormFieldData field;
    field.set_form_control_type(FormControlType::kInputCheckbox);
    field.set_name(u"field2");
    field.set_check_status(FormFieldData::CheckStatus::kCheckableButUnchecked);
    test_api(form).Append(field);
  }

  {
    FormFieldData field;
    field.set_form_control_type(FormControlType::kInputRadio);
    field.set_name(u"field1");
    field.set_check_status(FormFieldData::CheckStatus::kChecked);
    test_api(form).Append(field);
  }

  {
    FormFieldData field;
    field.set_form_control_type(FormControlType::kInputDate);
    field.set_name(u"field2");
    test_api(form).Append(field);
  }

  EXPECT_EQ(StrToHash64Bit("http://foo.com&form_name&field1"),
            CalculateFormSignature(form).value());
}

TEST(SignaturesTest, AlternativeFormSignatureLarge) {
  FormData large_form;
  large_form.set_url(GURL("http://foo.com/login?q=a#ref"));

  FormFieldData field1;
  field1.set_form_control_type(FormControlType::kInputText);
  test_api(large_form).Append(field1);

  FormFieldData field2;
  field2.set_form_control_type(FormControlType::kInputText);
  test_api(large_form).Append(field2);

  FormFieldData field3;
  field3.set_form_control_type(FormControlType::kInputEmail);
  test_api(large_form).Append(field3);

  FormFieldData field4;
  field4.set_form_control_type(FormControlType::kInputTelephone);
  test_api(large_form).Append(field4);

  // Alternative form signature string of a form with more than two fields
  // should only concatenate scheme, host, and field types.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&text&email&tel"),
            CalculateAlternativeFormSignature(large_form).value());
}

TEST(SignaturesTest, AlternativeFormSignatureSmallPath) {
  FormData small_form_path;
  small_form_path.set_url(GURL("http://foo.com/login?q=a#ref"));

  FormFieldData field1;
  field1.set_form_control_type(FormControlType::kInputText);
  test_api(small_form_path).Append(field1);

  FormFieldData field2;
  field2.set_form_control_type(FormControlType::kInputText);
  test_api(small_form_path).Append(field2);

  // Alternative form signature string of a form with 2 fields or less should
  // concatenate scheme, host, field types, and path if it is non-empty.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&text/login"),
            CalculateAlternativeFormSignature(small_form_path).value());
}

TEST(SignaturesTest, AlternativeFormSignatureSmallRef) {
  FormData small_form_ref;
  small_form_ref.set_url(GURL("http://foo.com?q=a#ref"));

  FormFieldData field1;
  field1.set_form_control_type(FormControlType::kInputText);
  test_api(small_form_ref).Append(field1);

  FormFieldData field2;
  field2.set_form_control_type(FormControlType::kInputText);
  test_api(small_form_ref).Append(field2);

  // Alternative form signature string of a form with 2 fields or less and
  // without a path should concatenate scheme, host, field types, and reference
  // if it is non-empty.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&text#ref"),
            CalculateAlternativeFormSignature(small_form_ref).value());
}

TEST(SignaturesTest, AlternativeFormSignatureSmallQuery) {
  FormData small_form_query;
  small_form_query.set_url(GURL("http://foo.com?q=a"));

  FormFieldData field1;
  field1.set_form_control_type(FormControlType::kInputText);
  test_api(small_form_query).Append(field1);

  FormFieldData field2;
  field2.set_form_control_type(FormControlType::kInputText);
  test_api(small_form_query).Append(field2);

  // Alternative form signature string of a form with 2 fields or less and
  // without a path or reference should concatenate scheme, host, field types,
  // and query if it is non-empty.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&text?q=a"),
            CalculateAlternativeFormSignature(small_form_query).value());
}

TEST(SignaturesTest, StructuralFormSignatureSmall) {
  // Test with a small form (<= 2 fields).
  FormData small_form;
  small_form.set_url(GURL("http://foo.com/login?q=a#ref"));

  FormFieldData field1;
  field1.set_form_control_type(FormControlType::kInputText);
  test_api(small_form).Append(field1);

  FormFieldData field2;
  field2.set_form_control_type(FormControlType::kInputEmail);
  test_api(small_form).Append(field2);

  // Structural form signature string of a form with 2 fields or less should
  // only concatenate scheme, host, and field types. It should not include
  // path, ref or query.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&email"),
            CalculateStructuralFormSignature(small_form).value());
}

TEST(SignaturesTest, StructuralFormSignatureLarge) {
  // Test with a large form (> 2 fields).
  FormData large_form;
  large_form.set_url(GURL("http://foo.com/login?q=a#ref"));

  FormFieldData field1;
  field1.set_form_control_type(FormControlType::kInputText);
  test_api(large_form).Append(field1);

  FormFieldData field2;
  field2.set_form_control_type(FormControlType::kInputEmail);
  test_api(large_form).Append(field2);

  FormFieldData field3;
  field3.set_form_control_type(FormControlType::kInputTelephone);
  test_api(large_form).Append(field3);

  // Structural form signature string of a form with more than two fields
  // should only concatenate scheme, host, and field types.
  EXPECT_EQ(StrToHash64Bit("http://foo.com&text&email&tel"),
            CalculateStructuralFormSignature(large_form).value());
}

}  // namespace autofill
