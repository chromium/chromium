// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/optimization_guide_proto_util.h"

#include "base/test/protobuf_matchers.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::EqualsProto;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Property;
using ::testing::SizeIs;

class AutofillOptimizationGuideProtoUtilTest : public testing::Test {
 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

class ByConversionReason
    : public AutofillOptimizationGuideProtoUtilTest,
      public testing::WithParamInterface<FormDataProtoConversionReason> {};

INSTANTIATE_TEST_SUITE_P(
    AutofillOptimizationGuideProtoUtilTest,
    ByConversionReason,
    testing::Values(FormDataProtoConversionReason::kModelRequest,
                    FormDataProtoConversionReason::kExtensionAPI));

TEST_P(ByConversionReason, ToFormDataProto) {
  FormData form = test::GetFormData(
      {.fields = {{.is_focusable = true,
                   .label = u"label",
                   .name = u"name",
                   .value = u"val",
                   .placeholder = u"placeholder",
                   .aria_label = u"aria-label",
                   .form_control_ax_id = 123},
                  {.is_focusable = false,
                   .label = u"label2",
                   .name = u"name2",
                   .value = u"value",
                   .aria_description = u"aria-description",
                   .form_control_ax_id = 124},
                  {.is_focusable = false,
                   .label = u"select",
                   .form_control_type = FormControlType::kSelectOne,
                   .select_options = {{{.value = u"1", .text = u"text1"},
                                       {.value = u"2", .text = u"text2"}}

                   }}}});
  optimization_guide::proto::FormData form_data_proto =
      ToFormDataProto(form, /*conversion_reason=*/GetParam());
  ASSERT_THAT(form_data_proto.fields(), SizeIs(3));

  EXPECT_EQ(form_data_proto.form_signature(), *CalculateFormSignature(form));

  optimization_guide::proto::FormFieldData field_data1 =
      form_data_proto.fields(0);
  EXPECT_EQ(field_data1.field_signature(),
            *CalculateFieldSignatureForField(form.fields()[0]));
  EXPECT_EQ(field_data1.field_label(), "label");
  EXPECT_EQ(field_data1.field_value(), "");
  EXPECT_EQ(field_data1.field_name(), "name");
  EXPECT_EQ(field_data1.is_focusable(), true);
  EXPECT_EQ(field_data1.placeholder(), "placeholder");
  EXPECT_EQ(field_data1.aria_label(), "aria-label");
  EXPECT_EQ(field_data1.aria_description(), "");
  EXPECT_EQ(field_data1.form_control_ax_node_id(), 123);

  optimization_guide::proto::FormFieldData field_data2 =
      form_data_proto.fields(1);
  EXPECT_EQ(field_data2.field_signature(),
            *CalculateFieldSignatureForField(form.fields()[1]));
  EXPECT_EQ(field_data2.field_label(), "label2");
  EXPECT_EQ(field_data2.field_value(), "");
  EXPECT_EQ(field_data2.field_name(), "name2");
  EXPECT_EQ(field_data2.is_focusable(), false);
  EXPECT_EQ(field_data2.placeholder(), "");
  EXPECT_EQ(field_data2.aria_label(), "");
  EXPECT_EQ(field_data2.aria_description(), "aria-description");
  EXPECT_EQ(field_data2.form_control_ax_node_id(), 124);

  // Check that the options are corectly extracted from the select element.
  optimization_guide::proto::FormFieldData field_data3 =
      form_data_proto.fields(2);
  EXPECT_EQ(field_data3.field_signature(),
            *CalculateFieldSignatureForField(form.fields()[2]));
  EXPECT_EQ(field_data3.field_label(), "select");
  EXPECT_TRUE(field_data3.field_value().empty());
  EXPECT_TRUE(field_data3.field_name().empty());
  auto create_option = [](const std::string& value, const std::string& text) {
    optimization_guide::proto::SelectOption option;
    option.set_value(value);
    option.set_text(text);
    return option;
  };
  EXPECT_THAT(field_data3.select_options(),
              ElementsAre(EqualsProto(create_option("1", "text1")),
                          EqualsProto(create_option("2", "text2"))));
}

// Tests that the "ForExtensionAPI" flavor additionally populates global IDs.
TEST_F(AutofillOptimizationGuideProtoUtilTest, ToFormDataProtoForExtensionAPI) {
  const FormGlobalId form_id = test::MakeFormGlobalId();
  const FieldGlobalId field_id = test::MakeFieldGlobalId();
  FormData form = test::GetFormData({
      .fields = {{.host_frame = field_id.frame_token,
                  .renderer_id = field_id.renderer_id,
                  .name = u"name"}},
      .host_frame = form_id.frame_token,
      .renderer_id = form_id.renderer_id,
  });
  optimization_guide::proto::FormData form_proto =
      ToFormDataProto(form, FormDataProtoConversionReason::kExtensionAPI);

  // Form-level metadata.
  EXPECT_EQ(form_proto.global_id().frame_token(),
            form_id.frame_token->ToString());
  EXPECT_EQ(form_proto.global_id().renderer_id(), *form_id.renderer_id);

  // Field-level metadata.
  ASSERT_THAT(form_proto.fields(), SizeIs(1));
  const optimization_guide::proto::FormFieldData& field_proto =
      form_proto.fields(0);
  EXPECT_EQ(field_proto.global_id().frame_token(),
            field_id.frame_token->ToString());
  EXPECT_EQ(field_proto.global_id().renderer_id(), *field_id.renderer_id);
}

}  // namespace
}  // namespace autofill
