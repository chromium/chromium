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
  // Needed to have unique renderer ids for different fields.
  static int renderer_id = 0;
  FormStructureTestApi test_api{form};
  AutofillField& added_field = test_api.PushField();
  added_field.set_renderer_id(FieldRendererId(++renderer_id));
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
  added_field.set_form_control_type(FormControlType::kSelectOne);
  added_field.set_options(options);
  return added_field;
}

TEST(AutofillOptimizationGuideProtoUtilTest, ToFormDataProto) {
  FormStructure form{FormData()};
  // TODO(crbug.com/373776019): Restructure to remove and extend the
  // AddInputField method.
  AutofillField& field1 = AddInputField(form, u"label", u"name", u"val");
  field1.set_field_is_eligible_for_prediction_improvements(true);
  field1.set_is_visible(true);
  field1.set_is_focusable(true);
  field1.set_placeholder(u"placeholder");
  field1.set_form_control_ax_id(123);
  field1.set_field_is_eligible_for_prediction_improvements(true);

  AutofillField& field2 =
      AddInputField(form, u"label2", u"name2", u"sensitive_value",
                    /*is_sensitive=*/true);
  field2.set_field_is_eligible_for_prediction_improvements(false);
  field2.set_is_visible(true);
  field2.set_is_focusable(false);
  field2.set_placeholder(u"");
  field2.set_form_control_ax_id(124);
  field2.set_value_identified_as_potentially_sensitive(true);

  AutofillField& field3 = AddSelect(
      form, u"select", u"", u"",
      {{.value = u"1", .text = u"text1"}, {.value = u"2", .text = u"text2"}},
      /*is_sensitive=*/false);
  field3.set_field_is_eligible_for_prediction_improvements(false);

  optimization_guide::proto::FormData form_data_proto = ToFormDataProto(form);
  EXPECT_EQ(form_data_proto.fields_size(), 3);

  // The first field should contain everything including the value.
  optimization_guide::proto::FormFieldData field_data1 =
      form_data_proto.fields(0);
  EXPECT_EQ(field_data1.field_label(), "label");
  EXPECT_EQ(field_data1.field_value(), "val");
  EXPECT_EQ(field_data1.field_name(), "name");
  EXPECT_EQ(field_data1.is_visible(), true);
  EXPECT_EQ(field_data1.is_focusable(), true);
  EXPECT_EQ(field_data1.placeholder(), "placeholder");
  EXPECT_EQ(field_data1.form_control_ax_node_id(), 123);
  EXPECT_EQ(field_data1.is_eligible(), true);

  // The second field should contain an empty value because it was marked as
  // sensitive.
  optimization_guide::proto::FormFieldData field_data2 =
      form_data_proto.fields(1);
  EXPECT_EQ(field_data2.field_label(), "label2");
  EXPECT_EQ(field_data2.field_value(), "");
  EXPECT_EQ(field_data2.field_name(), "name2");
  EXPECT_EQ(field_data2.is_visible(), true);
  EXPECT_EQ(field_data2.is_focusable(), false);
  EXPECT_EQ(field_data2.placeholder(), "");
  EXPECT_EQ(field_data2.form_control_ax_node_id(), 124);
  EXPECT_EQ(field_data2.is_eligible(), false);

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
  EXPECT_FALSE(field_data3.is_eligible());
}
}  // namespace
}  // namespace autofill
