// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_proto_util.h"

#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

TEST(OptimizationGuideProtoUtilTest, ToFormDataProto) {
  autofill::FormFieldData form_field_data;
  form_field_data.set_label(u"label");
  form_field_data.set_value(u"val");
  form_field_data.set_name(u"name");
  autofill::FormFieldData form_field_data_with_select;
  form_field_data_with_select.set_label(u"select");
  form_field_data_with_select.set_options(
      {{.value = u"1", .text = u"text1"}, {.value = u"2", .text = u"text2"}});
  autofill::FormData form_data;
  form_data.set_fields({form_field_data, form_field_data_with_select});

  optimization_guide::proto::FormData form_data_proto =
      ToFormDataProto(form_data);
  EXPECT_EQ(form_data_proto.fields_size(), 2);
  optimization_guide::proto::FormFieldData field_data1 =
      form_data_proto.fields(0);
  EXPECT_EQ(field_data1.field_label(), "label");
  EXPECT_EQ(field_data1.field_value(), "val");
  EXPECT_EQ(field_data1.field_name(), "name");
  optimization_guide::proto::FormFieldData field_data2 =
      form_data_proto.fields(1);
  EXPECT_EQ(field_data2.field_label(), "select");
  EXPECT_TRUE(field_data2.field_value().empty());
  EXPECT_TRUE(field_data2.field_name().empty());
  EXPECT_EQ(2, field_data2.select_options_size());
  optimization_guide::proto::SelectOption select_option1 =
      field_data2.select_options(0);
  EXPECT_EQ("1", select_option1.value());
  EXPECT_EQ("text1", select_option1.text());
  optimization_guide::proto::SelectOption select_option2 =
      field_data2.select_options(1);
  EXPECT_EQ("2", select_option2.value());
  EXPECT_EQ("text2", select_option2.text());
}

}  // namespace

}  // namespace optimization_guide
