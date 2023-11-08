// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
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
  auto maybe_string = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test);

  EXPECT_FALSE(maybe_string.has_value());
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructInputStringNoOnDeviceConfigForFeature) {
  proto::OnDeviceModelExecutionConfig config;
  config.add_feature_configs()->set_feature(
      proto::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION);
  UpdateInterpreterWithConfig(config);

  base::test::TestMessage test;
  test.set_test("some test");
  auto maybe_string = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test);

  EXPECT_FALSE(maybe_string.has_value());
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructInputStringFeatureConfigExistsButNoInputConfig) {
  proto::OnDeviceModelExecutionConfig config;
  config.add_feature_configs()->set_feature(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  UpdateInterpreterWithConfig(config);

  base::test::TestMessage test;
  test.set_test("some test");
  auto maybe_string = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test);

  EXPECT_FALSE(maybe_string.has_value());
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
  auto maybe_string = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test);

  EXPECT_FALSE(maybe_string.has_value());
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructInputStringFeatureConfigExistsUnexpectedArgsEvaluated) {
  proto::OnDeviceModelExecutionConfig config;
  auto* fc = config.add_feature_configs();
  fc->set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  auto* input_config = fc->mutable_input_config();
  input_config->set_request_base_name("base.test.TestMessage");
  auto* substitution = input_config->add_execute_substitutions();
  substitution->set_string_template("hello this is a %s");
  substitution->set_expected_num_args(1);
  substitution->add_args()->set_raw_string("test");
  substitution->add_args()->set_raw_string("test2");
  UpdateInterpreterWithConfig(config);

  base::test::TestMessage test;
  test.set_test("some test");
  auto maybe_string = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test);

  EXPECT_FALSE(maybe_string.has_value());
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
  substitution->set_expected_num_args(1);
  substitution->add_args()->set_raw_string("test");
  UpdateInterpreterWithConfig(config);

  base::test::TestMessage test;
  test.set_test("some test");
  auto maybe_string = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test);

  ASSERT_TRUE(maybe_string);
  EXPECT_EQ(*maybe_string, "hello this is a test");
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConstructInputStringFeatureWithProtoField) {
  proto::OnDeviceModelExecutionConfig config;
  auto* fc = config.add_feature_configs();
  fc->set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  auto* input_config = fc->mutable_input_config();
  input_config->set_request_base_name(
      "optimization_guide.proto.ComposeRequest");
  auto* substitution = input_config->add_execute_substitutions();
  substitution->set_string_template("hello this is a test: %s %s");
  substitution->set_expected_num_args(2);
  auto* proto_field = substitution->add_args()->mutable_proto_field();
  proto_field->add_proto_descriptors()->set_tag_number(2);
  auto* proto_field2 = substitution->add_args()->mutable_proto_field();
  proto_field2->add_proto_descriptors()->set_tag_number(3);
  proto_field2->add_proto_descriptors()->set_tag_number(2);
  UpdateInterpreterWithConfig(config);

  proto::ComposeRequest request;
  request.set_user_input("this is my input");
  request.mutable_page_metadata()->set_page_title("nested");
  auto maybe_string = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, request);

  ASSERT_TRUE(maybe_string);
  EXPECT_EQ(*maybe_string, "hello this is a test: this is my input nested");
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
  substitution->set_expected_num_args(1);
  auto* proto_field = substitution->add_args()->mutable_proto_field();
  proto_field->add_proto_descriptors()->set_tag_number(10000);
  UpdateInterpreterWithConfig(config);

  proto::ComposeRequest request;
  request.set_user_input("this is my input");
  request.mutable_page_metadata()->set_page_title("nested");
  auto maybe_string = interpreter()->ConstructInputString(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, request);

  EXPECT_FALSE(maybe_string.has_value());
}

}  // namespace

}  // namespace optimization_guide
