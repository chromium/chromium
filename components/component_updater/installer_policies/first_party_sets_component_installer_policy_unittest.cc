// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/first_party_sets_component_installer_policy.h"

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

using ::testing::_;
using ::testing::IsEmpty;

std::string ReadToString(base::File file) {
  std::string contents;
  base::ScopedFILE scoped_file(base::FileToFILE(std::move(file), "r"));
  return base::ReadStreamToString(scoped_file.get(), &contents) ? contents : "";
}

}  // namespace

class FirstPartySetsComponentInstallerTest : public ::testing::Test {
 public:
  FirstPartySetsComponentInstallerTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
  }

  void SetUp() override {
    FirstPartySetsComponentInstallerPolicy::ResetForTesting();
  }

 protected:
  base::test::TaskEnvironment env_;

  base::ScopedTempDir component_install_dir_;
};

TEST_F(FirstPartySetsComponentInstallerTest, NonexistentFile_OnComponentReady) {
  ASSERT_TRUE(base::DeleteFile(
      FirstPartySetsComponentInstallerPolicy::GetInstalledPathForTesting(
          component_install_dir_.GetPath())));

  base::test::TestFuture<base::Version, base::File> future;
  FirstPartySetsComponentInstallerPolicy(future.GetCallback(),
                                         base::TaskPriority::USER_BLOCKING)
      .ComponentReadyForTesting(base::Version(),
                                component_install_dir_.GetPath(),
                                base::Value::Dict());

  std::tuple<base::Version, base::File> got = future.Take();
  EXPECT_FALSE(std::get<0>(got).IsValid());
  EXPECT_FALSE(std::get<1>(got).IsValid());
}

TEST_F(FirstPartySetsComponentInstallerTest,
       NonexistentFile_OnRegistrationComplete) {
  ASSERT_TRUE(base::DeleteFile(
      FirstPartySetsComponentInstallerPolicy::GetInstalledPathForTesting(
          component_install_dir_.GetPath())));

  base::test::TestFuture<base::Version, base::File> future;
  FirstPartySetsComponentInstallerPolicy policy(
      future.GetCallback(), base::TaskPriority::USER_BLOCKING);
  policy.OnRegistrationComplete();

  std::tuple<base::Version, base::File> got = future.Take();
  EXPECT_FALSE(std::get<0>(got).IsValid());
  EXPECT_FALSE(std::get<1>(got).IsValid());

  // Only one call has any effect.
  policy.OnRegistrationComplete();
  env_.RunUntilIdle();
}

TEST_F(FirstPartySetsComponentInstallerTest, LoadsSets_OnComponentReady) {
  const base::Version version = base::Version("0.0.1");
  const std::string expectation = "some first party sets";
  base::test::TestFuture<base::Version, base::File> future;
  auto policy = std::make_unique<FirstPartySetsComponentInstallerPolicy>(
      future.GetCallback(), base::TaskPriority::USER_BLOCKING);

  ASSERT_TRUE(base::WriteFile(
      FirstPartySetsComponentInstallerPolicy::GetInstalledPathForTesting(
          component_install_dir_.GetPath()),
      expectation));

  policy->ComponentReadyForTesting(version, component_install_dir_.GetPath(),
                                   base::Value::Dict());

  std::tuple<base::Version, base::File> got = future.Take();
  EXPECT_TRUE(std::get<0>(got).IsValid());
  EXPECT_EQ(std::get<0>(got), version);
  EXPECT_TRUE(std::get<1>(got).IsValid());
  EXPECT_EQ(ReadToString(std::move(std::get<1>(got))), expectation);
}

// Test that when the first version of the component is installed,
// ComponentReady is a no-op, because OnRegistrationComplete already executed
// the OnceCallback.
TEST_F(FirstPartySetsComponentInstallerTest, IgnoreNewSets_NoInitialComponent) {
  base::test::TestFuture<base::Version, base::File> future;
  FirstPartySetsComponentInstallerPolicy policy(
      future.GetCallback(), base::TaskPriority::USER_BLOCKING);

  policy.OnRegistrationComplete();
  std::tuple<base::Version, base::File> got = future.Take();
  EXPECT_FALSE(std::get<0>(got).IsValid());
  EXPECT_FALSE(std::get<1>(got).IsValid());

  // Install the component, which should be ignored.
  base::ScopedTempDir install_dir;
  ASSERT_TRUE(install_dir.CreateUniqueTempDirUnderPath(
      component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(
      FirstPartySetsComponentInstallerPolicy::GetInstalledPathForTesting(
          install_dir.GetPath()),
      "first party sets content"));
  policy.ComponentReadyForTesting(base::Version("0.0.1"), install_dir.GetPath(),
                                  base::Value::Dict());

  env_.RunUntilIdle();
}

// Test if a component has been installed, ComponentReady will be no-op when
// newer versions are installed.
TEST_F(FirstPartySetsComponentInstallerTest, IgnoreNewSets_OnComponentReady) {
  base::test::TestFuture<base::Version, base::File> future;
  FirstPartySetsComponentInstallerPolicy policy(
      future.GetCallback(), base::TaskPriority::USER_BLOCKING);

  const base::Version version = base::Version("0.0.1");
  const std::string sets_v1 = "first party sets v1";
  base::ScopedTempDir dir_v1;
  ASSERT_TRUE(
      dir_v1.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(
      FirstPartySetsComponentInstallerPolicy::GetInstalledPathForTesting(
          dir_v1.GetPath()),
      sets_v1));
  policy.ComponentReadyForTesting(version, dir_v1.GetPath(),
                                  base::Value::Dict());

  std::tuple<base::Version, base::File> got = future.Take();
  EXPECT_TRUE(std::get<0>(got).IsValid());
  EXPECT_EQ(std::get<0>(got), version);
  EXPECT_TRUE(std::get<1>(got).IsValid());
  EXPECT_EQ(ReadToString(std::move(std::get<1>(got))), sets_v1);

  // Install newer version of the component, which should not be picked up
  // when calling ComponentReady again.
  const std::string sets_v2 = "first party sets v2";
  base::ScopedTempDir dir_v2;
  ASSERT_TRUE(
      dir_v2.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(
      FirstPartySetsComponentInstallerPolicy::GetInstalledPathForTesting(
          dir_v2.GetPath()),
      sets_v2));
  policy.ComponentReadyForTesting(base::Version("0.0.1"), dir_v2.GetPath(),
                                  base::Value::Dict());

  env_.RunUntilIdle();
}

TEST_F(FirstPartySetsComponentInstallerTest, GetInstallerAttributes) {
  FirstPartySetsComponentInstallerPolicy policy(
      base::DoNothing(), base::TaskPriority::USER_BLOCKING);

  EXPECT_THAT(policy.GetInstallerAttributesForTesting(), IsEmpty());
}

}  // namespace component_updater
