// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/integration_tests.h"

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace test {

// The project's position is that component builds are not portable outside of
// the build directory. Therefore, installation of component builds is not
// expected to work and these tests do not run on component builders.
// See crbug.com/1112527.
#if defined(OS_WIN) || !defined(COMPONENT_BUILD)

namespace {

void ExpectActiveVersion(std::string expected) {
  EXPECT_EQ(CreateGlobalPrefs()->GetActiveVersion(), expected);
}

#if defined(OS_MAC)
void ExpectQualified() {
  EXPECT_TRUE(CreateLocalPrefs()->GetQualified());
}
#endif

}  // namespace

class IntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Clean();
    ExpectClean();
    EnterTestMode();
  }

  void TearDown() override {
    ExpectClean();
    Clean();
  }

 private:
  base::test::TaskEnvironment environment_;
};

TEST_F(IntegrationTest, InstallUninstall) {
  Install();
  ExpectInstalled();
  Uninstall();
}

TEST_F(IntegrationTest, InstallAndPromote) {
  Install();
  ExpectInstalled();
// TODO(crbug.com/1109231): resolve implementation inconsistencies for
// different platforms. In this case, Windows promotes during Install, and
// as a post-condition, the version of the updater is active before --wake
// runs.
#if defined(OS_WIN)
  ExpectActiveVersion(UPDATER_VERSION_STRING);
#else
  ExpectActiveVersion("0");
#endif
  RunWake(0);  // Candidate qualifies and promotes to active.
  ExpectActiveVersion(UPDATER_VERSION_STRING);
  ExpectActive();
  Uninstall();
}

#if defined(OS_MAC)
TEST_F(IntegrationTest, RegisterTestApp) {
  RegisterTestApp();
  ExpectInstalled();
  ExpectQualified();
  ExpectActiveVersion(UPDATER_VERSION_STRING);
  ExpectActive();
  Uninstall();
}
#endif

#endif  // defined(OS_WIN) || !defined(COMPONENT_BUILD)

}  // namespace test

}  // namespace updater
