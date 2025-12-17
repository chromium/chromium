// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

using base::test::ErrorIs;

const struct OnDeviceBaseModelSpec kModelSpec = {"test", "0.0.1", {}};
const struct OnDeviceBaseModelSpec kModelSpecNew = {"test", "0.0.2", {}};

class OnDeviceModelMetadataTest : public testing::Test {
 public:
  OnDeviceModelMetadataTest() = default;
  ~OnDeviceModelMetadataTest() override = default;

  void SetUp() override {
    loader_.reset();
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

  void UpdateModel(MaybeOnDeviceModelMetadata metadata) {
    metadata_ = std::move(metadata);
  }

  OnDeviceModelMetadataLoader& loader() { return *loader_; }

  const MaybeOnDeviceModelMetadata& metadata() { return metadata_; }

 private:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;

  MaybeOnDeviceModelMetadata metadata_ =
      base::unexpected(OnDeviceModelStatus::kNotReadyForUnknownReason);
  std::optional<OnDeviceModelMetadataLoader> loader_;
};

TEST_F(OnDeviceModelMetadataTest, EmptyFilePath) {
  loader().Load(base::FilePath(), "test", kModelSpec);
  RunUntilIdle();
  EXPECT_THAT(metadata(), ErrorIs(OnDeviceModelStatus::kInstallNotComplete));
}

TEST_F(OnDeviceModelMetadataTest, ConfigFileNotInProvidedPath) {
  loader().Load(temp_dir(), "test", kModelSpec);
  RunUntilIdle();
  EXPECT_THAT(metadata(), ErrorIs(OnDeviceModelStatus::kInstallNotComplete));
}

TEST_F(OnDeviceModelMetadataTest, ValidConfig) {
  proto::OnDeviceModelExecutionConfig config;
  config.mutable_validation_config()->add_validation_prompts()->set_prompt(
      "test prompt");
  WriteConfigToFile(temp_dir().Append(kOnDeviceModelExecutionConfigFile),
                    config);
  loader().Load(temp_dir(), "test", kModelSpec);
  RunUntilIdle();
  ASSERT_OK_AND_ASSIGN(const OnDeviceModelMetadata& model_metadata, metadata());
  EXPECT_EQ(1, model_metadata.validation_config().validation_prompts().size());
}

TEST_F(OnDeviceModelMetadataTest, ResolvesToLastLoad) {
  proto::OnDeviceModelExecutionConfig config;
  config.mutable_validation_config()->add_validation_prompts()->set_prompt(
      "test prompt");
  WriteConfigToFile(temp_dir().Append(kOnDeviceModelExecutionConfigFile),
                    config);

  // Set up a second version.
  base::ScopedTempDir new_temp_dir;
  ASSERT_TRUE(new_temp_dir.CreateUniqueTempDir());
  base::FilePath new_config_file_path(new_temp_dir.GetPath().Append(
      FILE_PATH_LITERAL("on_device_model_execution_config.pb")));
  proto::OnDeviceModelExecutionConfig new_config;
  new_config.mutable_validation_config()->add_validation_prompts()->set_prompt(
      "prompt1");
  new_config.mutable_validation_config()->add_validation_prompts()->set_prompt(
      "prompt2");
  WriteConfigToFile(new_config_file_path, new_config);

  // Do both loads
  loader().Load(temp_dir(), "version1", kModelSpec);
  loader().Load(new_temp_dir.GetPath(), "version2", kModelSpecNew);
  RunUntilIdle();

  // The final state should match the last Load.
  ASSERT_OK_AND_ASSIGN(const OnDeviceModelMetadata& model_metadata, metadata());
  EXPECT_EQ(model_metadata.version(), "version2");
  EXPECT_EQ(model_metadata.model_spec().model_version,
            kModelSpecNew.model_version);
  EXPECT_EQ(model_metadata.model_spec().model_name, kModelSpecNew.model_name);
  EXPECT_EQ(2, model_metadata.validation_config().validation_prompts().size());
}

TEST_F(OnDeviceModelMetadataTest, InvalidStateChangeResets) {
  proto::OnDeviceModelExecutionConfig config;
  config.mutable_validation_config()->add_validation_prompts()->set_prompt(
      "test prompt");
  WriteConfigToFile(temp_dir().Append(kOnDeviceModelExecutionConfigFile),
                    config);
  loader().Load(temp_dir(), "test", kModelSpec);
  loader().StateChanged(
      base::unexpected(OnDeviceModelStatus::kInstallNotComplete));
  RunUntilIdle();
  // metadata should have reverted to error state again after the first load
  // finished.
  EXPECT_THAT(metadata(), ErrorIs(OnDeviceModelStatus::kInstallNotComplete));
}

}  // namespace

}  // namespace optimization_guide
