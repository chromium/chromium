// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/trust_token_key_commitments_component_installer_policy.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_updater_switches.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {
using ::testing::_;
}  // namespace

class TrustTokenKeyCommitmentsComponentInstallerTest : public ::testing::Test {
 public:
  TrustTokenKeyCommitmentsComponentInstallerTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
  }

 protected:
  base::test::TaskEnvironment env_;

  base::ScopedTempDir component_install_dir_;
};

TEST_F(TrustTokenKeyCommitmentsComponentInstallerTest,
       LoadsCommitmentsFromOverriddenPath) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(network::features::kPrivateStateTokens);

  base::SequenceCheckerImpl checker;

  std::string expectation = "some trust token keys";
  base::RunLoop run_loop;
  auto confirmation_callback = [&](const std::string& got) {
    EXPECT_TRUE(checker.CalledOnValidSequence());
    EXPECT_EQ(got, expectation);
    run_loop.Quit();
  };

  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));

  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      switches::kComponentUpdaterTrustTokensComponentPath, temp_path);

  ASSERT_TRUE(base::WriteFile(temp_path, expectation));

  auto policy =
      std::make_unique<TrustTokenKeyCommitmentsComponentInstallerPolicy>(
          base::BindLambdaForTesting(confirmation_callback));

  // The |component_install_dir_.GetPath()| should be ignored in favor of the
  // separate path we provide through the switch.
  policy->ComponentReady(base::Version(), component_install_dir_.GetPath(),
                         base::Value::Dict());

  run_loop.Run();

  base::DeleteFile(temp_path);
}

TEST_F(TrustTokenKeyCommitmentsComponentInstallerTest, LoadsCommitments) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(network::features::kPrivateStateTokens);

  base::SequenceCheckerImpl checker;

  std::string expectation = "some trust token keys";
  base::RunLoop run_loop;
  auto confirmation_callback = [&](const std::string& got) {
    EXPECT_TRUE(checker.CalledOnValidSequence());
    EXPECT_EQ(got, expectation);
    run_loop.Quit();
  };
  auto policy =
      std::make_unique<TrustTokenKeyCommitmentsComponentInstallerPolicy>(
          base::BindLambdaForTesting(confirmation_callback));

  ASSERT_TRUE(base::WriteFile(
      TrustTokenKeyCommitmentsComponentInstallerPolicy::GetInstalledPath(
          component_install_dir_.GetPath()),
      expectation));

  policy->ComponentReady(base::Version(), component_install_dir_.GetPath(),
                         base::Value::Dict());

  run_loop.Run();
}

}  // namespace component_updater
