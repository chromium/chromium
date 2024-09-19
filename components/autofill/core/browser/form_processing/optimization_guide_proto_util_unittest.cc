// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/optimization_guide_proto_util.h"

#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

AutofillField& AddInputField(FormStructure& form,
                             const std::u16string& label,
                             const std::u16string& name,
                             const std::u16string value,
                             bool is_sensitive = false) {
  FormStructureTestApi test_api{form};
  AutofillField& added_field = test_api.PushField();
  added_field.set_value(value);
  added_field.set_name(name);
  added_field.set_label(label);
  added_field.set_value_identified_as_potentially_sensitive(is_sensitive);
  return added_field;
}

AutofillField& AddSelect(FormStructure& form,
                         const std::u16string& label,
                         const std::u16string& name,
                         const std::u16string value,
                         const std::vector<SelectOption> options,
                         bool is_sensitive = false) {
  AutofillField& added_field =
      AddInputField(form, label, name, value, is_sensitive);
  added_field.set_form_control_type(autofill::FormControlType::kSelectOne);
  added_field.set_options(options);
  return added_field;
}

TEST(AutofillOptimizationGuideProtoUtilTest, ToFormDataProto) {
  FormStructure form{autofill::FormData()};
  AddInputField(form, u"label", u"name", u"val", /*is_sensitive=*/false);
  AddInputField(form, u"label2", u"name2", u"sensitive_value",
                /*is_sensitive=*/true);
  AddSelect(
      form, u"select", u"", u"",
      {{.value = u"1", .text = u"text1"}, {.value = u"2", .text = u"text2"}},
      /*is_sensitive=*/false);

  optimization_guide::proto::FormData form_data_proto = ToFormDataProto(form);
  EXPECT_EQ(form_data_proto.fields_size(), 3);

  // The first field should contain everything including the value.
  optimization_guide::proto::FormFieldData field_data1 =
      form_data_proto.fields(0);
  EXPECT_EQ(field_data1.field_label(), "label");
  EXPECT_EQ(field_data1.field_value(), "val");
  EXPECT_EQ(field_data1.field_name(), "name");

  // The second field should contain an empty value because it was marked as
  // sensitive.
  optimization_guide::proto::FormFieldData field_data2 =
      form_data_proto.fields(1);
  EXPECT_EQ(field_data2.field_label(), "label2");
  EXPECT_EQ(field_data2.field_value(), "");
  EXPECT_EQ(field_data2.field_name(), "name2");

  // Check that the options are corectly extracted from the select element.
  optimization_guide::proto::FormFieldData field_data3 =
      form_data_proto.fields(2);
  EXPECT_EQ(field_data3.field_label(), "select");
  EXPECT_TRUE(field_data3.field_value().empty());
  EXPECT_TRUE(field_data3.field_name().empty());
  EXPECT_EQ(2, field_data3.select_options_size());
  optimization_guide::proto::SelectOption select_option1 =
      field_data3.select_options(0);
  EXPECT_EQ("1", select_option1.value());
  EXPECT_EQ("text1", select_option1.text());
  optimization_guide::proto::SelectOption select_option2 =
      field_data3.select_options(1);
  EXPECT_EQ("2", select_option2.value());
  EXPECT_EQ("text2", select_option2.text());
}

}  // namespace

}  // namespace autofill
