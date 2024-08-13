// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/plus_address_blocklist_component_installer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/plus_addresses/blocked_facets.pb.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_blocklist_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace component_updater {
namespace {
constexpr char kPlusAddressBlocklistVersion[] = "1";
const base::FilePath::CharType kPlusAddressBlocklistBinaryPbFileName[] =
    FILE_PATH_LITERAL("compact_blocked_facets.pb");
}  // namespace

class PlusAddressBlocklistInstallerPolicyTest : public PlatformTest {
 public:
  PlusAddressBlocklistInstallerPolicyTest() = default;
  PlusAddressBlocklistInstallerPolicyTest(
      const PlusAddressBlocklistInstallerPolicyTest&) = delete;
  PlusAddressBlocklistInstallerPolicyTest& operator=(
      const PlusAddressBlocklistInstallerPolicyTest&) = delete;
  ~PlusAddressBlocklistInstallerPolicyTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
  }

  base::FilePath component_install_dir() const {
    return component_install_dir_.GetPath();
  }

  bool CreateTestPlusAddressBlocklist(const std::string& blocklist_content) {
    base::FilePath blocklist_path =
        component_install_dir().Append(kPlusAddressBlocklistBinaryPbFileName);
    return base::WriteFile(blocklist_path, blocklist_content);
  }

  void LoadPlusAddressBlocklist() {
    base::Value::Dict manifest;
    ASSERT_TRUE(policy_.VerifyInstallation(manifest, component_install_dir()));
    const base::Version expected_version(kPlusAddressBlocklistVersion);
    policy_.ComponentReady(expected_version, component_install_dir(),
                           std::move(manifest));
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir component_install_dir_;
  MockComponentUpdateService cus_;
  PlusAddressBlocklistInstallerPolicy policy_;
  base::test::ScopedFeatureList scoped_list{
      plus_addresses::features::kPlusAddressBlocklistEnabled};
};

TEST_F(PlusAddressBlocklistInstallerPolicyTest,
       ComponentRegistrationWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      plus_addresses::features::kPlusAddressBlocklistEnabled);
  EXPECT_CALL(cus_, RegisterComponent(testing::_)).Times(0);
  RegisterPlusAddressBlocklistComponent(&cus_);
  RunUntilIdle();
}

TEST_F(PlusAddressBlocklistInstallerPolicyTest,
       ComponentRegistrationWhenFeatureEnabled) {
  EXPECT_CALL(cus_, RegisterComponent(testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  RegisterPlusAddressBlocklistComponent(&cus_);
  RunUntilIdle();
}

TEST_F(PlusAddressBlocklistInstallerPolicyTest, LoadFileWithData) {
  plus_addresses::CompactPlusAddressBlockedFacets blocked_facets;
  blocked_facets.set_exclusion_pattern("foo");
  blocked_facets.set_exception_pattern("bar");
  ASSERT_TRUE(
      CreateTestPlusAddressBlocklist(blocked_facets.SerializeAsString()));
  ASSERT_NO_FATAL_FAILURE(LoadPlusAddressBlocklist());

  const plus_addresses::PlusAddressBlocklistData& blocklist_data =
      plus_addresses::PlusAddressBlocklistData::GetInstance();
  ASSERT_TRUE(blocklist_data.GetExclusionPattern());
  ASSERT_TRUE(blocklist_data.GetExceptionPattern());
  EXPECT_EQ(blocklist_data.GetExceptionPattern()->pattern(), "bar");
  EXPECT_EQ(blocklist_data.GetExclusionPattern()->pattern(), "foo");
}

}  // namespace component_updater
