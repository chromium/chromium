// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
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
    WriteConfigToFile(temp_dir().Append(kOnDeviceModelExecutionConfigFile),
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

  EXPECT_EQ(interpreter()->GetAdapter(proto::MODEL_EXECUTION_FEATURE_COMPOSE),
            nullptr);
}

TEST_F(OnDeviceModelExecutionConfigInterpeterTest,
       ConfigFileNotInProvidedPath) {
  interpreter()->UpdateConfigWithFileDir(temp_dir());
  RunUntilIdle();

  EXPECT_EQ(interpreter()->GetAdapter(proto::MODEL_EXECUTION_FEATURE_COMPOSE),
            nullptr);
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

    EXPECT_NE(interpreter()->GetAdapter(proto::MODEL_EXECUTION_FEATURE_COMPOSE),
              nullptr);
    EXPECT_EQ(interpreter()->GetAdapter(
                  proto::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION),
              nullptr);
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

    EXPECT_EQ(interpreter()->GetAdapter(proto::MODEL_EXECUTION_FEATURE_COMPOSE),
              nullptr);
    EXPECT_NE(interpreter()->GetAdapter(
                  proto::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION),
              nullptr);
  }

  {
    // Bad config clears state.
    interpreter()->UpdateConfigWithFileDir(base::FilePath());
    RunUntilIdle();

    EXPECT_EQ(interpreter()->GetAdapter(proto::MODEL_EXECUTION_FEATURE_COMPOSE),
              nullptr);
    EXPECT_EQ(interpreter()->GetAdapter(
                  proto::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION),
              nullptr);
  }
}

}  // namespace

}  // namespace optimization_guide
