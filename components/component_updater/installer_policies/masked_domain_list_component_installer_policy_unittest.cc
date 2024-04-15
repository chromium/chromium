// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/masked_domain_list_component_installer_policy.h"

#include <optional>
#include <string>
#include <tuple>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {
using ::testing::_;

// Not a valid MDL but enough to check the protobuf end to end.
std::string FakeMDL(const std::string& owner_name) {
  masked_domain_list::MaskedDomainList mdl;
  mdl.add_resource_owners()->set_owner_name(owner_name);
  std::string proto_bytes;
  CHECK(mdl.SerializeToString(&proto_bytes));
  return proto_bytes;
}

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
  const std::string expectation = "owner-1";
  std::string proto_bytes = FakeMDL(expectation);
  base::test::RepeatingTestFuture<base::Version,
                                  std::optional<mojo_base::ProtoWrapper>>
      future;
  auto policy = MaskedDomainListComponentInstallerPolicy(future.GetCallback());

  ASSERT_TRUE(base::WriteFile(
      MaskedDomainListComponentInstallerPolicy::GetInstalledPath(
          component_install_dir_.GetPath()),
      proto_bytes));

  policy.ComponentReady(version, component_install_dir_.GetPath(),
                        base::Value::Dict());

  std::tuple<base::Version, std::optional<mojo_base::ProtoWrapper>> got =
      future.Take();
  EXPECT_TRUE(std::get<0>(got).IsValid());
  EXPECT_EQ(std::get<0>(got), version);
  EXPECT_TRUE(std::get<1>(got).has_value());
  auto mdl = std::get<1>(got)->As<masked_domain_list::MaskedDomainList>();
  EXPECT_EQ(mdl->resource_owners()[0].owner_name(), expectation);
}

TEST_F(MaskedDomainListComponentInstallerPolicyTest, LoadsNewListWhenUpdated) {
  scoped_feature_list_.InitAndEnableFeature(
      network::features::kMaskedDomainList);

  base::test::RepeatingTestFuture<base::Version,
                                  std::optional<mojo_base::ProtoWrapper>>
      future;
  auto policy = MaskedDomainListComponentInstallerPolicy(future.GetCallback());

  const base::Version version1 = base::Version("0.0.1");
  const std::string list_v1 = "MDL v1";
  std::string proto_bytes_v1 = FakeMDL(list_v1);

  base::ScopedTempDir dir_v1;
  ASSERT_TRUE(
      dir_v1.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(
      MaskedDomainListComponentInstallerPolicy::GetInstalledPath(
          dir_v1.GetPath()),
      proto_bytes_v1));
  policy.ComponentReady(version1, dir_v1.GetPath(), base::Value::Dict());

  std::tuple<base::Version, std::optional<mojo_base::ProtoWrapper>> got =
      future.Take();
  EXPECT_TRUE(std::get<0>(got).IsValid());
  EXPECT_EQ(std::get<0>(got), version1);
  EXPECT_TRUE(std::get<1>(got).has_value());
  auto mdl_v1 = std::get<1>(got)->As<masked_domain_list::MaskedDomainList>();
  EXPECT_EQ(mdl_v1->resource_owners()[0].owner_name(), list_v1);

  // Install newer version of the component, which should be picked up
  // when calling ComponentReady again.
  const base::Version version2 = base::Version("0.0.2");
  const std::string list_v2 = "MDL v2";
  std::string proto_bytes_v2 = FakeMDL(list_v2);
  base::ScopedTempDir dir_v2;
  ASSERT_TRUE(
      dir_v2.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(
      MaskedDomainListComponentInstallerPolicy::GetInstalledPath(
          dir_v2.GetPath()),
      proto_bytes_v2));
  policy.ComponentReady(version2, dir_v2.GetPath(), base::Value::Dict());

  std::tuple<base::Version, std::optional<mojo_base::ProtoWrapper>> got2 =
      future.Take();
  EXPECT_TRUE(std::get<0>(got2).IsValid());
  EXPECT_EQ(std::get<0>(got2), version2);
  EXPECT_TRUE(std::get<1>(got2).has_value());
  auto mdl_v2 = std::get<1>(got2)->As<masked_domain_list::MaskedDomainList>();
  EXPECT_EQ(mdl_v2->resource_owners()[0].owner_name(), list_v2);

  env_.RunUntilIdle();
}

}  // namespace component_updater
