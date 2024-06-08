// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

const struct OnDeviceBaseModelSpec kModelSpec = {.model_name = "test",
                                                 .model_version = "0.0.1"};
const struct OnDeviceBaseModelSpec kModelSpecNew = {.model_name = "test",
                                                    .model_version = "0.0.2"};

class OnDeviceModelMetadataTest : public testing::Test {
 public:
  OnDeviceModelMetadataTest() = default;
  ~OnDeviceModelMetadataTest() override = default;

  void SetUp() override {
    loader_.reset();
    metadata_.reset();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    loader_.emplace(base::BindRepeating(&OnDeviceModelMetadataTest::UpdateModel,
                                        base::Unretained(this)),
                    nullptr);
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

  void WriteConfigToFile(const base::FilePath& file_path,
                         const proto::OnDeviceModelExecutionConfig& config) {
    std::string serialized_config;
    ASSERT_TRUE(config.SerializeToString(&serialized_config));
    ASSERT_TRUE(base::WriteFile(file_path, serialized_config));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void UpdateModel(std::unique_ptr<OnDeviceModelMetadata> metadata) {
    metadata_ = std::move(metadata);
  }

  OnDeviceModelMetadataLoader& loader() { return *loader_; }

  const OnDeviceModelMetadata* metadata() { return metadata_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;

  std::unique_ptr<OnDeviceModelMetadata> metadata_;
  std::optional<OnDeviceModelMetadataLoader> loader_;
};

TEST_F(OnDeviceModelMetadataTest, EmptyFilePath) {
  loader().Load(base::FilePath(), "test", kModelSpec);
  RunUntilIdle();

  EXPECT_EQ(metadata()->GetAdapter(proto::MODEL_EXECUTION_FEATURE_COMPOSE),
            nullptr);
}

TEST_F(OnDeviceModelMetadataTest, ConfigFileNotInProvidedPath) {
  loader().Load(temp_dir(), "test", kModelSpec);
  RunUntilIdle();

  EXPECT_EQ(metadata()->GetAdapter(proto::MODEL_EXECUTION_FEATURE_COMPOSE),
            nullptr);
}

TEST_F(OnDeviceModelMetadataTest, ValidConfig) {
  proto::OnDeviceModelExecutionConfig config;
  config.add_feature_configs()->set_feature(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  WriteConfigToFile(temp_dir().Append(kOnDeviceModelExecutionConfigFile),
                    config);
  loader().Load(temp_dir(), "test", kModelSpec);
  RunUntilIdle();

  EXPECT_NE(metadata()->GetAdapter(proto::MODEL_EXECUTION_FEATURE_COMPOSE),
            nullptr);
  EXPECT_EQ(
      metadata()->GetAdapter(proto::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION),
      nullptr);
}

TEST_F(OnDeviceModelMetadataTest, ResolvesToLastLoad) {
  proto::OnDeviceModelExecutionConfig config;
  config.add_feature_configs()->set_feature(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  WriteConfigToFile(temp_dir().Append(kOnDeviceModelExecutionConfigFile),
                    config);

  // Set up a second version.
  base::ScopedTempDir new_temp_dir;
  ASSERT_TRUE(new_temp_dir.CreateUniqueTempDir());
  base::FilePath new_config_file_path(new_temp_dir.GetPath().Append(
      FILE_PATH_LITERAL("on_device_model_execution_config.pb")));
  proto::OnDeviceModelExecutionConfig new_config;
  new_config.add_feature_configs()->set_feature(
      proto::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION);
  WriteConfigToFile(new_config_file_path, new_config);

  // Do both loads
  loader().Load(temp_dir(), "version1", kModelSpec);
  loader().Load(new_temp_dir.GetPath(), "version2", kModelSpecNew);
  RunUntilIdle();

  // The final state should match the last Load.
  EXPECT_EQ(metadata()->version(), "version2");
  EXPECT_EQ(metadata()->model_spec().model_version,
            kModelSpecNew.model_version);
  EXPECT_EQ(metadata()->model_spec().model_name, kModelSpecNew.model_name);
  EXPECT_EQ(metadata()->GetAdapter(proto::MODEL_EXECUTION_FEATURE_COMPOSE),
            nullptr);
  EXPECT_NE(
      metadata()->GetAdapter(proto::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION),
      nullptr);
}

TEST_F(OnDeviceModelMetadataTest, NullStateChangeResets) {
  proto::OnDeviceModelExecutionConfig config;
  config.add_feature_configs()->set_feature(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  WriteConfigToFile(temp_dir().Append(kOnDeviceModelExecutionConfigFile),
                    config);
  loader().Load(temp_dir(), "test", kModelSpec);
  loader().StateChanged(nullptr);
  RunUntilIdle();
  // Should have reverted to null again after the first load finished.
  EXPECT_EQ(nullptr, metadata());
}

}  // namespace

}  // namespace optimization_guide
