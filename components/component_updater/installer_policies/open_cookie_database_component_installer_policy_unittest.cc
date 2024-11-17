// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/open_cookie_database_component_installer_policy.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {
constexpr base::FilePath::CharType kOpenCookieDatabaseCSVFileName[] =
    FILE_PATH_LITERAL("open-cookie-database.csv");
}  // namespace

class OpenCookieDatabaseComponentInstallerPolicyTest : public ::testing::Test {
 public:
  OpenCookieDatabaseComponentInstallerPolicyTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
    CHECK(component_install_dir_.IsValid());
  }

 protected:
  base::ScopedTempDir component_install_dir_;
};

TEST_F(OpenCookieDatabaseComponentInstallerPolicyTest,
       VerifyInstallation_ValidDir) {
  OpenCookieDatabaseComponentInstallerPolicy policy;

  EXPECT_FALSE(policy.VerifyInstallation(base::Value::Dict(),
                                         component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(
      component_install_dir_.GetPath().Append(kOpenCookieDatabaseCSVFileName),
      ""));
  EXPECT_TRUE(policy.VerifyInstallation(base::Value::Dict(),
                                        component_install_dir_.GetPath()));
}

TEST_F(OpenCookieDatabaseComponentInstallerPolicyTest,
       VerifyInstallation_InvalidDir) {
  OpenCookieDatabaseComponentInstallerPolicy policy;

  EXPECT_FALSE(policy.VerifyInstallation(base::Value::Dict(),
                                         component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(component_install_dir_.GetPath().Append(
                                  base::FilePath(FILE_PATH_LITERAL("invalid"))),
                              ""));
  EXPECT_FALSE(policy.VerifyInstallation(base::Value::Dict(),
                                         component_install_dir_.GetPath()));
}

}  // namespace component_updater
