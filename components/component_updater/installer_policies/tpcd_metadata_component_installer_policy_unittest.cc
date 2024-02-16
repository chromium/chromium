// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/tpcd_metadata_component_installer_policy.h"

#include <string_view>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/tpcd/metadata/parser_test_helper.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {
namespace {
using ::testing::_;

constexpr base::FilePath::CharType kComponentFileName[] =
    FILE_PATH_LITERAL("metadata.pb");

constexpr char kTpcdMetadataInstallationResult[] =
    "Navigation.TpcdMitigations.MetadataInstallationResult";
}  // namespace

class TpcdMetadataComponentInstallerPolicyTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<bool> {
 public:
  TpcdMetadataComponentInstallerPolicyTest() {
    CHECK(install_dir_.CreateUniqueTempDir());
    CHECK(install_dir_.IsValid());
    path_ = install_dir().Append(kComponentFileName);
    CHECK(!path_.empty());
    if (GetParam()) {
      scoped_list_.InitAndEnableFeature(net::features::kTpcdMetadataGrants);
    } else {
      scoped_list_.InitAndDisableFeature(net::features::kTpcdMetadataGrants);
    }
  }

  ~TpcdMetadataComponentInstallerPolicyTest() override = default;

 protected:
  base::test::TaskEnvironment env_;
  const base::FilePath& install_dir() const { return install_dir_.GetPath(); }

  base::FilePath path() const { return path_; }

  void ExecFakeComponentInstallation(std::string_view contents) {
    CHECK(base::WriteFile(path(), contents));
    CHECK(base::PathExists(path()));
  }

  component_updater::ComponentInstallerPolicy* policy() {
    return policy_.get();
  }

 private:
  base::ScopedTempDir install_dir_;
  base::FilePath path_;
  base::test::ScopedFeatureList scoped_list_;
  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy_ =
      std::make_unique<TpcdMetadataComponentInstallerPolicy>(base::DoNothing());
};

TEST_P(TpcdMetadataComponentInstallerPolicyTest,
       VerifyInstallation_InvalidInstallDir) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(policy()->VerifyInstallation(
      base::Value::Dict(), install_dir().Append(FILE_PATH_LITERAL("x"))));

  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      TpcdMetadataInstallationResult::kMissingMetadataFile, 1);
}

TEST_P(TpcdMetadataComponentInstallerPolicyTest,
       VerifyInstallation_RejectsMissingFile) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(
      policy()->VerifyInstallation(base::Value::Dict(), install_dir()));

  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      TpcdMetadataInstallationResult::kMissingMetadataFile, 1);
}

TEST_P(TpcdMetadataComponentInstallerPolicyTest,
       VerifyInstallation_RejectsNotProtoFile) {
  ExecFakeComponentInstallation("clearly not a proto");

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(
      policy()->VerifyInstallation(base::Value::Dict(), install_dir()));
  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      TpcdMetadataInstallationResult::kParsingToProtoFailed, 1);
}

TEST_P(TpcdMetadataComponentInstallerPolicyTest,
       FeatureEnabled_ComponentReady_ErroneousPrimarySpec) {
  if (!GetParam()) {
    GTEST_SKIP() << "Reason: Test parameter instance N/A";
  }

  const std::string primary_pattern_spec = "[*]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  tpcd::metadata::Metadata metadata;
  AddEntryToMetadata(metadata, primary_pattern_spec, secondary_pattern_spec);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  ExecFakeComponentInstallation(metadata.SerializeAsString());

  base::HistogramTester histogram_tester;
  ASSERT_FALSE(
      policy()->VerifyInstallation(base::Value::Dict(), install_dir()));
  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      TpcdMetadataInstallationResult::kErroneousSpec, 1);
}

TEST_P(TpcdMetadataComponentInstallerPolicyTest,
       FeatureEnabled_ComponentReady_ErroneousSecondarySpec) {
  if (!GetParam()) {
    GTEST_SKIP() << "Reason: Test parameter instance N/A";
  }

  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*]foo.com";

  tpcd::metadata::Metadata metadata;
  AddEntryToMetadata(metadata, primary_pattern_spec, secondary_pattern_spec);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  ExecFakeComponentInstallation(metadata.SerializeAsString());

  base::HistogramTester histogram_tester;
  ASSERT_FALSE(
      policy()->VerifyInstallation(base::Value::Dict(), install_dir()));
  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      TpcdMetadataInstallationResult::kErroneousSpec, 1);
}

TEST_P(TpcdMetadataComponentInstallerPolicyTest,
       FeatureEnabled_ComponentReady_FiresCallback) {
  if (!GetParam()) {
    GTEST_SKIP() << "Reason: Test parameter instance N/A";
  }

  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  tpcd::metadata::Metadata metadata;
  AddEntryToMetadata(metadata, primary_pattern_spec, secondary_pattern_spec);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  ExecFakeComponentInstallation(metadata.SerializeAsString());

  base::RunLoop run_loop;

  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy =
      std::make_unique<TpcdMetadataComponentInstallerPolicy>(
          base::BindLambdaForTesting([&](const std::string& raw_metadata) {
            EXPECT_EQ(raw_metadata, metadata.SerializeAsString());
            run_loop.Quit();
          }));

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(policy->VerifyInstallation(base::Value::Dict(), install_dir()));
  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      TpcdMetadataInstallationResult::kSuccessful, 1);

  policy->ComponentReady(base::Version(), install_dir(), base::Value::Dict());

  run_loop.Run();
}

TEST_P(TpcdMetadataComponentInstallerPolicyTest,
       FeatureDisabled_ComponentReady_DoesNotFireCallback) {
  if (GetParam()) {
    GTEST_SKIP() << "Reason: Test parameter instance N/A";
  }

  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  tpcd::metadata::Metadata metadata;
  AddEntryToMetadata(metadata, primary_pattern_spec, secondary_pattern_spec);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  ExecFakeComponentInstallation(metadata.SerializeAsString());

  base::RunLoop run_loop;

  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy =
      std::make_unique<TpcdMetadataComponentInstallerPolicy>(
          base::BindLambdaForTesting(
              [&](const std::string& raw_metadata) { NOTREACHED_NORETURN(); }));

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(policy->VerifyInstallation(base::Value::Dict(), install_dir()));
  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      TpcdMetadataInstallationResult::kSuccessful, 1);

  policy->ComponentReady(base::Version(), install_dir(), base::Value::Dict());

  run_loop.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    TpcdMetadataComponentInstallerPolicyTest,
    ::testing::Bool());

}  // namespace component_updater
