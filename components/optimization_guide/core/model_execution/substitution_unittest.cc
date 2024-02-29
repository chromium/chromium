// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/substitution.h"

#include "base/test/test.pb.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

class SubstitutionTest : public testing::Test {
 public:
  SubstitutionTest() = default;
  ~SubstitutionTest() override = default;
};

TEST_F(SubstitutionTest, RawString) {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  auto* substitution = subs.Add();
  substitution->set_string_template("hello this is a %s");
  substitution->add_substitutions()->add_candidates()->set_raw_string("test");

  base::test::TestMessage request;
  request.set_test("some test");
  auto result = CreateSubstitutions(request, subs);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->input_string, "hello this is a test");
  EXPECT_FALSE(result->should_ignore_input_context);
}

TEST_F(SubstitutionTest, ProtoField) {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  auto* substitution = subs.Add();
  substitution->set_string_template("hello this is a test: %s %s");
  substitution->set_should_ignore_input_context(true);
  auto* proto_field2 = substitution->add_substitutions()
                           ->add_candidates()
                           ->mutable_proto_field();
  proto_field2->add_proto_descriptors()->set_tag_number(3);
  proto_field2->add_proto_descriptors()->set_tag_number(2);
  auto* proto_field3 = substitution->add_substitutions()
                           ->add_candidates()
                           ->mutable_proto_field();
  proto_field3->add_proto_descriptors()->set_tag_number(7);
  proto_field3->add_proto_descriptors()->set_tag_number(1);

  proto::ComposeRequest request;
  request.mutable_page_metadata()->set_page_title("nested");
  request.mutable_generate_params()->set_user_input("inner type");
  auto result = CreateSubstitutions(request, subs);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->input_string, "hello this is a test: nested inner type");
  EXPECT_TRUE(result->should_ignore_input_context);
}

TEST_F(SubstitutionTest, BadProtoField) {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  auto* substitution = subs.Add();
  substitution->set_string_template("hello this is a test: %s");
  auto* proto_field = substitution->add_substitutions()
                          ->add_candidates()
                          ->mutable_proto_field();
  proto_field->add_proto_descriptors()->set_tag_number(10000);

  proto::ComposeRequest request;
  request.mutable_page_metadata()->set_page_title("nested");

  auto result = CreateSubstitutions(request, subs);

  EXPECT_FALSE(result);
}

TEST_F(SubstitutionTest, Conditions) {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  auto* execute_substitution = subs.Add();
  execute_substitution->set_string_template("hello this is a test: %s %s");
  auto* substitution1_proto_field = execute_substitution->add_substitutions()
                                        ->add_candidates()
                                        ->mutable_proto_field();
  substitution1_proto_field->add_proto_descriptors()->set_tag_number(8);
  substitution1_proto_field->add_proto_descriptors()->set_tag_number(1);
  auto* substitution2 = execute_substitution->add_substitutions();
  auto* arg1 = substitution2->add_candidates();
  auto* proto_field1 = arg1->mutable_proto_field();
  proto_field1->add_proto_descriptors()->set_tag_number(3);
  proto_field1->add_proto_descriptors()->set_tag_number(1);
  auto* arg1_conditions = arg1->mutable_conditions();
  arg1_conditions->set_condition_evaluation_type(
      proto::CONDITION_EVALUATION_TYPE_OR);
  auto* arg1_c1 = arg1_conditions->add_conditions();
  auto* arg1_c1_proto_field = arg1_c1->mutable_proto_field();
  arg1_c1_proto_field->add_proto_descriptors()->set_tag_number(8);
  arg1_c1_proto_field->add_proto_descriptors()->set_tag_number(2);
  arg1_c1->set_operator_type(proto::OPERATOR_TYPE_EQUAL_TO);
  arg1_c1->mutable_value()->set_int32_value(1);
  auto* arg1_c2 = arg1_conditions->add_conditions();
  arg1_c2->mutable_proto_field()->add_proto_descriptors()->set_tag_number(8);
  arg1_c2->mutable_proto_field()->add_proto_descriptors()->set_tag_number(2);
  arg1_c2->set_operator_type(proto::OPERATOR_TYPE_EQUAL_TO);
  arg1_c1->mutable_value()->set_int32_value(2);
  auto* arg2 = substitution2->add_candidates();
  auto* proto_field2 = arg2->mutable_proto_field();
  proto_field2->add_proto_descriptors()->set_tag_number(3);
  proto_field2->add_proto_descriptors()->set_tag_number(2);
  auto* arg2_conditions = arg2->mutable_conditions();
  arg2_conditions->set_condition_evaluation_type(
      proto::CONDITION_EVALUATION_TYPE_OR);
  auto* arg2_c1 = arg2_conditions->add_conditions();
  auto* arg2_c1_proto_field = arg2_c1->mutable_proto_field();
  arg2_c1_proto_field->add_proto_descriptors()->set_tag_number(8);
  arg2_c1_proto_field->add_proto_descriptors()->set_tag_number(3);
  arg2_c1->set_operator_type(proto::OPERATOR_TYPE_EQUAL_TO);
  arg2_c1->mutable_value()->set_int32_value(1);
  auto* arg2_c2 = arg2_conditions->add_conditions();
  arg2_c2->mutable_proto_field()->add_proto_descriptors()->set_tag_number(8);
  arg2_c2->mutable_proto_field()->add_proto_descriptors()->set_tag_number(3);
  arg2_c2->set_operator_type(proto::OPERATOR_TYPE_EQUAL_TO);
  arg2_c1->mutable_value()->set_int32_value(2);

  auto* execute_substitution2 = subs.Add();
  execute_substitution2->set_string_template("should be ignored: %s");
  execute_substitution2->add_substitutions()->add_candidates()->set_raw_string(
      "also ignored");
  auto* es2_conditions = execute_substitution2->mutable_conditions();
  es2_conditions->set_condition_evaluation_type(
      proto::CONDITION_EVALUATION_TYPE_AND);
  auto* c1 = es2_conditions->add_conditions();
  auto* c1_proto_field = c1->mutable_proto_field();
  c1_proto_field->add_proto_descriptors()->set_tag_number(8);
  c1_proto_field->add_proto_descriptors()->set_tag_number(2);
  c1->set_operator_type(proto::OPERATOR_TYPE_NOT_EQUAL_TO);
  c1->mutable_value()->set_int32_value(0);

  proto::ComposeRequest request;
  request.mutable_rewrite_params()->set_previous_response("this is my input");
  request.mutable_rewrite_params()->set_length(proto::COMPOSE_LONGER);
  request.mutable_page_metadata()->set_page_title("title");
  request.mutable_page_metadata()->set_page_url("url");

  auto result = CreateSubstitutions(request, subs);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->input_string,
            "hello this is a test: this is my input title");
  EXPECT_FALSE(result->should_ignore_input_context);
}

}  // namespace

}  // namespace optimization_guide
