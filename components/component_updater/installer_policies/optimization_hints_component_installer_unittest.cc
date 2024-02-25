// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/optimization_hints_component_installer.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_hints_component_update_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

static const char kTestHintsVersion[] = "1.2.3";

class OptimizationHintsMockComponentUpdateService
    : public component_updater::MockComponentUpdateService {
 public:
  OptimizationHintsMockComponentUpdateService() = default;

  OptimizationHintsMockComponentUpdateService(
      const OptimizationHintsMockComponentUpdateService&) = delete;
  OptimizationHintsMockComponentUpdateService& operator=(
      const OptimizationHintsMockComponentUpdateService&) = delete;

  ~OptimizationHintsMockComponentUpdateService() override = default;
};

}  // namespace

namespace component_updater {

class OptimizationHintsComponentInstallerTest : public PlatformTest {
 public:
  OptimizationHintsComponentInstallerTest() = default;

  OptimizationHintsComponentInstallerTest(
      const OptimizationHintsComponentInstallerTest&) = delete;
  OptimizationHintsComponentInstallerTest& operator=(
      const OptimizationHintsComponentInstallerTest&) = delete;

  ~OptimizationHintsComponentInstallerTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());

    policy_ = std::make_unique<OptimizationHintsComponentInstallerPolicy>();
  }

  base::FilePath component_install_dir() {
    return component_install_dir_.GetPath();
  }

  void CreateTestOptimizationHints(const std::string& hints_content) {
    base::FilePath hints_path = component_install_dir().Append(
        optimization_guide::kUnindexedHintsFileName);
    ASSERT_TRUE(base::WriteFile(hints_path, hints_content));
  }

  void LoadOptimizationHints() {
    base::Value::Dict manifest;
    ASSERT_TRUE(policy_->VerifyInstallation(manifest, component_install_dir()));
    const base::Version expected_version(kTestHintsVersion);
    policy_->ComponentReady(expected_version, component_install_dir(),
                            std::move(manifest));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir component_install_dir_;

  std::unique_ptr<OptimizationHintsComponentInstallerPolicy> policy_;
};

TEST_F(OptimizationHintsComponentInstallerTest,
       ComponentRegistrationWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      optimization_guide::features::kOptimizationHints);
  std::unique_ptr<OptimizationHintsMockComponentUpdateService> cus(
      new OptimizationHintsMockComponentUpdateService());
  EXPECT_CALL(*cus, RegisterComponent(testing::_)).Times(0);
  RegisterOptimizationHintsComponent(cus.get());
  RunUntilIdle();
}

TEST_F(OptimizationHintsComponentInstallerTest,
       ComponentRegistrationWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHints);
  std::unique_ptr<OptimizationHintsMockComponentUpdateService> cus(
      new OptimizationHintsMockComponentUpdateService());
  EXPECT_CALL(*cus, RegisterComponent(testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  RegisterOptimizationHintsComponent(cus.get());
  RunUntilIdle();
}

TEST_F(OptimizationHintsComponentInstallerTest, LoadFileWithData) {
  const std::string expected_hints = "some hints";
  ASSERT_NO_FATAL_FAILURE(CreateTestOptimizationHints(expected_hints));
  ASSERT_NO_FATAL_FAILURE(LoadOptimizationHints());

  std::optional<optimization_guide::HintsComponentInfo> component_info =
      optimization_guide::OptimizationHintsComponentUpdateListener::
          GetInstance()
              ->hints_component_info();
  EXPECT_TRUE(component_info.has_value());
  EXPECT_EQ(base::Version(kTestHintsVersion), component_info->version);
  std::string actual_hints;
  ASSERT_TRUE(base::ReadFileToString(component_info->path, &actual_hints));
  EXPECT_EQ(expected_hints, actual_hints);
}

}  // namespace component_updater
