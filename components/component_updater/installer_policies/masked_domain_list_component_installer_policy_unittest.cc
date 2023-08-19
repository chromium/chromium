// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/masked_domain_list_component_installer_policy.h"

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {
using ::testing::_;

}  // namespace

class MaskedDomainListComponentInstallerPolicyTest : public ::testing::Test {
 public:
  MaskedDomainListComponentInstallerPolicyTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
  }

 protected:
  base::test::TaskEnvironment env_;
  base::test::ScopedFeatureList scoped_feature_list_;

  base::ScopedTempDir component_install_dir_;
};

TEST_F(MaskedDomainListComponentInstallerPolicyTest,
       LoadsFile_OnComponentReady) {
  scoped_feature_list_.InitAndEnableFeature(
      network::features::kMaskedDomainList);
  const base::Version version = base::Version("0.0.1");
  const std::string expectation = "some list contents";
  base::test::RepeatingTestFuture<base::Version, std::string> future;
  auto policy = MaskedDomainListComponentInstallerPolicy(future.GetCallback());

  ASSERT_TRUE(base::WriteFile(
      MaskedDomainListComponentInstallerPolicy::GetInstalledPath(
          component_install_dir_.GetPath()),
      expectation));

  policy.ComponentReady(version, component_install_dir_.GetPath(),
                        base::Value::Dict());

  std::tuple<base::Version, std::string> got = future.Take();
  EXPECT_TRUE(std::get<0>(got).IsValid());
  EXPECT_EQ(std::get<0>(got), version);
  EXPECT_EQ(std::get<1>(got), expectation);
}

TEST_F(MaskedDomainListComponentInstallerPolicyTest, LoadsNewListWhenUpdated) {
  scoped_feature_list_.InitAndEnableFeature(
      network::features::kMaskedDomainList);

  base::test::RepeatingTestFuture<base::Version, std::string> future;
  auto policy = MaskedDomainListComponentInstallerPolicy(future.GetCallback());

  const base::Version version1 = base::Version("0.0.1");
  const std::string list_v1 = "MDL v1";
  base::ScopedTempDir dir_v1;
  ASSERT_TRUE(
      dir_v1.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(
      MaskedDomainListComponentInstallerPolicy::GetInstalledPath(
          dir_v1.GetPath()),
      list_v1));
  policy.ComponentReady(version1, dir_v1.GetPath(), base::Value::Dict());

  std::tuple<base::Version, std::string> got = future.Take();
  EXPECT_TRUE(std::get<0>(got).IsValid());
  EXPECT_EQ(std::get<0>(got), version1);
  EXPECT_EQ(std::get<1>(got), list_v1);

  // Install newer version of the component, which should be picked up
  // when calling ComponentReady again.
  const base::Version version2 = base::Version("0.0.2");
  const std::string list_v2 = "MDL v2";
  base::ScopedTempDir dir_v2;
  ASSERT_TRUE(
      dir_v2.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(
      MaskedDomainListComponentInstallerPolicy::GetInstalledPath(
          dir_v2.GetPath()),
      list_v2));
  policy.ComponentReady(version2, dir_v2.GetPath(), base::Value::Dict());

  std::tuple<base::Version, std::string> got2 = future.Take();
  EXPECT_TRUE(std::get<0>(got2).IsValid());
  EXPECT_EQ(std::get<0>(got2), version2);
  EXPECT_EQ(std::get<1>(got2), list_v2);

  env_.RunUntilIdle();
}

}  // namespace component_updater
