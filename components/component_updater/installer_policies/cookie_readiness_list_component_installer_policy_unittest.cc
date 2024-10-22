// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/cookie_readiness_list_component_installer_policy.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
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

}  // namespace component_updater
