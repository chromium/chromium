// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

class OnDeviceModelExecutionConfigInterpeterTest : public testing::Test {
 public:
  OnDeviceModelExecutionConfigInterpeterTest() = default;
  ~OnDeviceModelExecutionConfigInterpeterTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    interpreter_ = std::make_unique<OnDeviceModelExecutionConfigInterpreter>();
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

  OnDeviceModelExecutionConfigInterpreter* interpreter() {
    return interpreter_.get();
  }

  void WriteConfigToFile(const base::FilePath& file_path,
                         const proto::OnDeviceModelExecutionConfig& config) {
    std::string serialized_config;
    ASSERT_TRUE(config.SerializeToString(&serialized_config));
    ASSERT_TRUE(base::WriteFile(file_path, serialized_config));
  }

  void UpdateInterpreterWithConfig(
      const proto::OnDeviceModelExecutionConfig& config) {
    WriteConfigToFile(temp_dir().Append(FILE_PATH_LITERAL(
                          "on_device_model_execution_config.pb")),
                      config);
    interpreter()->UpdateConfigWithFileDir(temp_dir());
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;

  std::unique_ptr<OnDeviceModelExecutionConfigInterpreter> interpreter_;
};

TEST_F(OnDeviceModelExecutionConfigInterpeterTest, EmptyFilePath) {
  interpreter()->UpdateConfigWithFileDir(base::FilePath());
  RunUntilIdle();

  EXPECT_FALSE(interpreter()->HasConfigForFeature(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE));
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConfigFileNotInProvidedPath) {
  interpreter()->UpdateConfigWithFileDir(temp_dir());
  RunUntilIdle();

  EXPECT_FALSE(interpreter()->HasConfigForFeature(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE));
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest, ValidConfig) {
  {
    base::FilePath config_file_path(temp_dir().Append(
        FILE_PATH_LITERAL("on_device_model_execution_config.pb")));
    proto::OnDeviceModelExecutionConfig config;
    config.add_feature_configs()->set_feature(
        proto::MODEL_EXECUTION_FEATURE_COMPOSE);
    WriteConfigToFile(config_file_path, config);

    interpreter()->UpdateConfigWithFileDir(temp_dir());
    RunUntilIdle();

    EXPECT_TRUE(interpreter()->HasConfigForFeature(
        proto::MODEL_EXECUTION_FEATURE_COMPOSE));
    EXPECT_FALSE(interpreter()->HasConfigForFeature(
        proto::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION));
  }

  {
    // Create a new dir with a new file and config.
    base::ScopedTempDir new_temp_dir;
    ASSERT_TRUE(new_temp_dir.CreateUniqueTempDir());
    base::FilePath new_config_file_path(new_temp_dir.GetPath().Append(
        FILE_PATH_LITERAL("on_device_model_execution_config.pb")));
    proto::OnDeviceModelExecutionConfig new_config;
    new_config.add_feature_configs()->set_feature(
        proto::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION);
    WriteConfigToFile(new_config_file_path, new_config);

    interpreter()->UpdateConfigWithFileDir(new_temp_dir.GetPath());
    RunUntilIdle();

    EXPECT_FALSE(interpreter()->HasConfigForFeature(
        proto::MODEL_EXECUTION_FEATURE_COMPOSE));
    EXPECT_TRUE(interpreter()->HasConfigForFeature(
        proto::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION));
  }

  {
    // Bad config clears state.
    interpreter()->UpdateConfigWithFileDir(base::FilePath());
    RunUntilIdle();

    EXPECT_FALSE(interpreter()->HasConfigForFeature(
        proto::MODEL_EXECUTION_FEATURE_COMPOSE));
    EXPECT_FALSE(interpreter()->HasConfigForFeature(
        proto::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION));
  }
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructInputStringNoOnDeviceConfig) {
  base::test::TestMessage test;
  test.set_test("some test");
  auto result = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test,
      /*want_input_context=*/false);

  EXPECT_FALSE(result);
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructInputStringNoOnDeviceConfigForFeature) {
  proto::OnDeviceModelExecutionConfig config;
  config.add_feature_configs()->set_feature(
      proto::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION);
  UpdateInterpreterWithConfig(config);

  base::test::TestMessage test;
  test.set_test("some test");
  auto result = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test,
      /*want_input_context=*/false);

  EXPECT_FALSE(result);
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructInputStringFeatureConfigExistsButNoInputConfig) {
  proto::OnDeviceModelExecutionConfig config;
  config.add_feature_configs()->set_feature(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  UpdateInterpreterWithConfig(config);

  base::test::TestMessage test;
  test.set_test("some test");
  auto result = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test,
      /*want_input_context=*/false);

  EXPECT_FALSE(result);
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructInputStringFeatureConfigExistsMismatchRequest) {
  proto::OnDeviceModelExecutionConfig config;
  auto* fc = config.add_feature_configs();
  fc->set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  auto* input_config = fc->mutable_input_config();
  input_config->set_request_base_name("wrong name");
  UpdateInterpreterWithConfig(config);

  base::test::TestMessage test;
  test.set_test("some test");
  auto result = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test,
      /*want_input_context=*/false);

  EXPECT_FALSE(result);
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructInputStringSimpleRawStringInputContext) {
  proto::OnDeviceModelExecutionConfig config;
  auto* fc = config.add_feature_configs();
  fc->set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  auto* input_config = fc->mutable_input_config();
  input_config->set_request_base_name("base.test.TestMessage");
  auto* substitution = input_config->add_input_context_substitutions();
  substitution->set_string_template("hello this is a %s");
  substitution->add_substitutions()->add_candidates()->set_raw_string("test");
  auto* execute_substitution = input_config->add_execute_substitutions();
  execute_substitution->set_string_template("this should be ignored");
  UpdateInterpreterWithConfig(config);

  base::test::TestMessage test;
  test.set_test("some test");
  auto result = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test,
      /*want_input_context=*/true);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->input_string, "hello this is a test");
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructInputStringFeatureConfigExistsSimpleRawString) {
  proto::OnDeviceModelExecutionConfig config;
  auto* fc = config.add_feature_configs();
  fc->set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  auto* input_config = fc->mutable_input_config();
  input_config->set_request_base_name("base.test.TestMessage");
  auto* substitution = input_config->add_execute_substitutions();
  substitution->set_string_template("hello this is a %s");
  substitution->add_substitutions()->add_candidates()->set_raw_string("test");
  UpdateInterpreterWithConfig(config);

  base::test::TestMessage test;
  test.set_test("some test");
  auto result = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test,
      /*want_input_context=*/false);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->input_string, "hello this is a test");
  EXPECT_FALSE(result->should_ignore_input_context);
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructInputStringFeatureWithProtoField) {
  proto::OnDeviceModelExecutionConfig config;
  auto* fc = config.add_feature_configs();
  fc->set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  auto* input_config = fc->mutable_input_config();
  input_config->set_request_base_name(
      "optimization_guide.proto.ComposeRequest");
  auto* input_substitutions = input_config->add_input_context_substitutions();
  input_substitutions->set_string_template("this is ignored");
  auto* substitution = input_config->add_execute_substitutions();
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
  UpdateInterpreterWithConfig(config);

  proto::ComposeRequest request;
  request.mutable_page_metadata()->set_page_title("nested");
  request.mutable_generate_params()->set_user_input("inner type");
  auto result = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, request,
      /*want_input_context=*/false);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->input_string, "hello this is a test: nested inner type");
  EXPECT_TRUE(result->should_ignore_input_context);
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructInputStringFeatureWithBadProtoField) {
  proto::OnDeviceModelExecutionConfig config;
  auto* fc = config.add_feature_configs();
  fc->set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  auto* input_config = fc->mutable_input_config();
  input_config->set_request_base_name(
      "optimization_guide.proto.ComposeRequest");
  auto* substitution = input_config->add_execute_substitutions();
  substitution->set_string_template("hello this is a test: %s");
  auto* proto_field = substitution->add_substitutions()
                          ->add_candidates()
                          ->mutable_proto_field();
  proto_field->add_proto_descriptors()->set_tag_number(10000);
  UpdateInterpreterWithConfig(config);

  proto::ComposeRequest request;
  request.mutable_page_metadata()->set_page_title("nested");
  auto result = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, request,
      /*want_input_context=*/false);

  EXPECT_FALSE(result);
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructInputStringWithConditions) {
  proto::OnDeviceModelExecutionConfig config;
  auto* fc = config.add_feature_configs();
  fc->set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  auto* input_config = fc->mutable_input_config();
  input_config->set_request_base_name(
      "optimization_guide.proto.ComposeRequest");
  auto* execute_substitution = input_config->add_execute_substitutions();
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

  auto* execute_substitution2 = input_config->add_execute_substitutions();
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
  UpdateInterpreterWithConfig(config);

  proto::ComposeRequest request;
  request.mutable_rewrite_params()->set_previous_response("this is my input");
  request.mutable_rewrite_params()->set_length(proto::COMPOSE_LONGER);
  request.mutable_page_metadata()->set_page_title("title");
  request.mutable_page_metadata()->set_page_url("url");
  auto result = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, request,
      /*want_input_context=*/false);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->input_string,
            "hello this is a test: this is my input title");
  EXPECT_FALSE(result->should_ignore_input_context);
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructOutputMetadataNoConfiguration) {
  auto maybe_metadata = interpreter()->ConstructOutputMetadata(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, "output");

  EXPECT_FALSE(maybe_metadata.has_value());
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructOutputMetadataNoOnDeviceConfigForFeature) {
  proto::OnDeviceModelExecutionConfig config;
  config.add_feature_configs()->set_feature(
      proto::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION);
  UpdateInterpreterWithConfig(config);

  auto maybe_metadata = interpreter()->ConstructOutputMetadata(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, "output");

  EXPECT_FALSE(maybe_metadata.has_value());
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructOutputMetadataOnDeviceConfigHasNoOutputConfig) {
  proto::OnDeviceModelExecutionConfig config;
  auto* fc = config.add_feature_configs();
  fc->set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);

  UpdateInterpreterWithConfig(config);

  auto maybe_metadata = interpreter()->ConstructOutputMetadata(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, "output");

  EXPECT_FALSE(maybe_metadata.has_value());
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructOutputMetadataBadProto) {
  proto::OnDeviceModelExecutionConfig config;
  auto* fc = config.add_feature_configs();
  fc->set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  auto* oc = fc->mutable_output_config();
  oc->set_proto_type("garbage type");
  oc->mutable_proto_field()->add_proto_descriptors()->set_tag_number(1);
  UpdateInterpreterWithConfig(config);

  auto maybe_metadata = interpreter()->ConstructOutputMetadata(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, "output");

  EXPECT_FALSE(maybe_metadata.has_value());
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructOutputMetadataDescriptorSpecifiedNotStringValue) {
  proto::OnDeviceModelExecutionConfig config;
  auto* fc = config.add_feature_configs();
  fc->set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  auto* oc = fc->mutable_output_config();
  oc->set_proto_type("optimization_guide.proto.ComposeRequest");
  oc->mutable_proto_field()->add_proto_descriptors()->set_tag_number(7);
  UpdateInterpreterWithConfig(config);

  auto maybe_metadata = interpreter()->ConstructOutputMetadata(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, "output");

  EXPECT_FALSE(maybe_metadata.has_value());
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructOutputMetadataDescriptorValid) {
  proto::OnDeviceModelExecutionConfig config;
  auto* fc = config.add_feature_configs();
  fc->set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  auto* oc = fc->mutable_output_config();
  oc->set_proto_type("optimization_guide.proto.ComposeResponse");
  oc->mutable_proto_field()->add_proto_descriptors()->set_tag_number(1);
  UpdateInterpreterWithConfig(config);

  auto maybe_metadata = interpreter()->ConstructOutputMetadata(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, "output");

  ASSERT_TRUE(maybe_metadata.has_value());
  EXPECT_EQ(
      "output",
      ParsedAnyMetadata<proto::ComposeResponse>(*maybe_metadata)->output());
}

}  // namespace

}  // namespace optimization_guide
