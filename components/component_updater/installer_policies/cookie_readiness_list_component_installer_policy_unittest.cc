// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/cookie_readiness_list_component_installer_policy.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {
constexpr base::FilePath::CharType kCookieReadinessListJsonFileName[] =
    FILE_PATH_LITERAL("cookie-readiness-list.json");
}  // namespace

class CookieReadinessListComponentInstallerPolicyTest : public ::testing::Test {
 public:
  CookieReadinessListComponentInstallerPolicyTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
    CHECK(component_install_dir_.IsValid());
  }

 protected:
  base::ScopedTempDir component_install_dir_;

  base::test::TaskEnvironment env_;
};

TEST_F(CookieReadinessListComponentInstallerPolicyTest,
       VerifyInstallation_ValidDir) {
  CookieReadinessListComponentInstallerPolicy policy;

  EXPECT_FALSE(policy.VerifyInstallation(base::Value::Dict(),
                                         component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(
      component_install_dir_.GetPath().Append(kCookieReadinessListJsonFileName),
      ""));
  EXPECT_TRUE(policy.VerifyInstallation(base::Value::Dict(),
                                        component_install_dir_.GetPath()));
}

TEST_F(CookieReadinessListComponentInstallerPolicyTest,
       VerifyInstallation_InvalidDir) {
  CookieReadinessListComponentInstallerPolicy policy;

  EXPECT_FALSE(policy.VerifyInstallation(base::Value::Dict(),
                                         component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(component_install_dir_.GetPath().Append(
                                  base::FilePath(FILE_PATH_LITERAL("invalid"))),
                              ""));
  EXPECT_FALSE(policy.VerifyInstallation(base::Value::Dict(),
                                         component_install_dir_.GetPath()));
}

TEST_F(CookieReadinessListComponentInstallerPolicyTest,
       ComponentReady_NonexistentFile) {
  base::test::TestFuture<const std::optional<std::string>> future;
  CookieReadinessListComponentInstallerPolicy policy(
      future.GetRepeatingCallback());

  policy.ComponentReadyForTesting(base::Version("0.0.1"),
                                  base::FilePath(FILE_PATH_LITERAL("invalid")),
                                  base::Value::Dict());

  EXPECT_EQ(future.Take(), std::nullopt);
}

TEST_F(CookieReadinessListComponentInstallerPolicyTest,
       ComponentReady_ValidFile) {
  const std::string expectation = "json";
  ASSERT_TRUE(base::WriteFile(
      CookieReadinessListComponentInstallerPolicy::GetInstalledPathForTesting(
          component_install_dir_.GetPath()),
      expectation));

  base::test::TestFuture<const std::optional<std::string>> future;
  CookieReadinessListComponentInstallerPolicy policy(
      future.GetRepeatingCallback());

  policy.ComponentReadyForTesting(base::Version("0.0.1"),
                                  component_install_dir_.GetPath(),
                                  base::Value::Dict());

  EXPECT_EQ(future.Take().value(), expectation);
}

TEST_F(CookieReadinessListComponentInstallerPolicyTest,
       ComponentReady_ComponentUpdate) {
  base::ScopedTempDir dir_v1;
  ASSERT_TRUE(
      dir_v1.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));

  const std::string expectation_v1 = "json";
  ASSERT_TRUE(base::WriteFile(
      CookieReadinessListComponentInstallerPolicy::GetInstalledPathForTesting(
          dir_v1.GetPath()),
      expectation_v1));

  base::test::TestFuture<const std::optional<std::string>> future;
  CookieReadinessListComponentInstallerPolicy policy(
      future.GetRepeatingCallback());

  policy.ComponentReadyForTesting(base::Version("0.0.1"), dir_v1.GetPath(),
                                  base::Value::Dict());

  EXPECT_EQ(future.Take().value(), expectation_v1);

  // Install newer component, which should be read by the policy.
  base::ScopedTempDir dir_v2;
  ASSERT_TRUE(
      dir_v2.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));

  const std::string expectation_v2 = "new json";
  ASSERT_TRUE(base::WriteFile(
      CookieReadinessListComponentInstallerPolicy::GetInstalledPathForTesting(
          dir_v2.GetPath()),
      expectation_v2));

  policy.ComponentReadyForTesting(base::Version("0.0.2"), dir_v2.GetPath(),
                                  base::Value::Dict());

  EXPECT_EQ(future.Take(), expectation_v2);
}

}  // namespace component_updater
