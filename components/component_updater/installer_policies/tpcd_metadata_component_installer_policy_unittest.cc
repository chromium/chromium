// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/tpcd_metadata_component_installer_policy.h"

#include <optional>
#include <string_view>
#include <tuple>

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
#include "components/tpcd/metadata/browser/parser.h"
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
      public ::testing::WithParamInterface<
          std::tuple</*IsTpcdMetadataGrantsEnabled:*/ bool,
                     /*IsTpcdMetadataStagingEnabled:*/ bool>> {
 public:
  TpcdMetadataComponentInstallerPolicyTest() {
    CHECK(install_dir_.CreateUniqueTempDir());
    CHECK(install_dir_.IsValid());
    path_ = install_dir().Append(kComponentFileName);
    CHECK(!path_.empty());
  }

  ~TpcdMetadataComponentInstallerPolicyTest() override = default;

  bool IsTpcdMetadataGrantsEnabled() { return std::get<0>(GetParam()); }
  bool IsTpcdMetadataStagingEnabled() { return std::get<1>(GetParam()); }

 protected:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsTpcdMetadataGrantsEnabled()) {
      enabled_features.push_back(net::features::kTpcdMetadataGrants);
    } else {
      disabled_features.push_back(net::features::kTpcdMetadataGrants);
    }

    if (IsTpcdMetadataStagingEnabled()) {
      enabled_features.push_back(net::features::kTpcdMetadataStageControl);
    } else {
      disabled_features.push_back(net::features::kTpcdMetadataStageControl);
    }

    scoped_list_.InitWithFeatures(enabled_features, disabled_features);
  }

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

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    TpcdMetadataComponentInstallerPolicyTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()));

TEST_P(TpcdMetadataComponentInstallerPolicyTest,
       VerifyInstallation_InvalidInstallDir) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(policy()->VerifyInstallation(
      base::Value::Dict(), install_dir().Append(FILE_PATH_LITERAL("x"))));

  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      tpcd::metadata::InstallationResult::kMissingMetadataFile, 1);
}

TEST_P(TpcdMetadataComponentInstallerPolicyTest,
       VerifyInstallation_RejectsMissingFile) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(
      policy()->VerifyInstallation(base::Value::Dict(), install_dir()));

  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      tpcd::metadata::InstallationResult::kMissingMetadataFile, 1);
}

TEST_P(TpcdMetadataComponentInstallerPolicyTest,
       VerifyInstallation_RejectsNotProtoFile) {
  ExecFakeComponentInstallation("clearly not a proto");

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(
      policy()->VerifyInstallation(base::Value::Dict(), install_dir()));
  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      tpcd::metadata::InstallationResult::kParsingToProtoFailed, 1);
}

TEST_P(TpcdMetadataComponentInstallerPolicyTest,
       ComponentReady_ErroneousPrimarySpec) {
  if (!IsTpcdMetadataGrantsEnabled() || !IsTpcdMetadataStagingEnabled()) {
    GTEST_SKIP() << "Reason: Test parameters instance N/A";
  }

  const std::string primary_pattern_spec = "[*]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  tpcd::metadata::Metadata metadata;
  auto* me = metadata.add_metadata_entries();
  me->set_primary_pattern_spec(primary_pattern_spec);
  me->set_secondary_pattern_spec(secondary_pattern_spec);
  me->set_source(tpcd::metadata::Parser::kSourceTest);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  ExecFakeComponentInstallation(metadata.SerializeAsString());

  base::HistogramTester histogram_tester;
  ASSERT_FALSE(
      policy()->VerifyInstallation(base::Value::Dict(), install_dir()));
  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      tpcd::metadata::InstallationResult::kErroneousSpec, 1);
}

TEST_P(TpcdMetadataComponentInstallerPolicyTest,
       ComponentReady_ErroneousSecondarySpec) {
  if (!IsTpcdMetadataGrantsEnabled() || !IsTpcdMetadataStagingEnabled()) {
    GTEST_SKIP() << "Reason: Test parameters instance N/A";
  }

  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*]foo.com";

  tpcd::metadata::Metadata metadata;
  auto* me = metadata.add_metadata_entries();
  me->set_primary_pattern_spec(primary_pattern_spec);
  me->set_secondary_pattern_spec(secondary_pattern_spec);
  me->set_source(tpcd::metadata::Parser::kSourceTest);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  ExecFakeComponentInstallation(metadata.SerializeAsString());

  base::HistogramTester histogram_tester;
  ASSERT_FALSE(
      policy()->VerifyInstallation(base::Value::Dict(), install_dir()));
  histogram_tester.ExpectBucketCount(
      kTpcdMetadataInstallationResult,
      tpcd::metadata::InstallationResult::kErroneousSpec, 1);
}

TEST_P(TpcdMetadataComponentInstallerPolicyTest, ComponentReady_ErroneousDtrp) {
  if (!IsTpcdMetadataGrantsEnabled() || !IsTpcdMetadataStagingEnabled()) {
    GTEST_SKIP() << "Reason: Test parameters instance N/A";
  }

  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  tpcd::metadata::Metadata metadata;
  auto* me = tpcd::metadata::helpers::AddEntryToMetadata(
      metadata, primary_pattern_spec, secondary_pattern_spec);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  // Set a valid DTRP override only.
  {
    me->set_dtrp_override(tpcd::metadata::Parser::kMaxDtrp);

    ExecFakeComponentInstallation(metadata.SerializeAsString());

    base::HistogramTester histogram_tester;
    EXPECT_FALSE(
        policy()->VerifyInstallation(base::Value::Dict(), install_dir()));
    histogram_tester.ExpectBucketCount(
        kTpcdMetadataInstallationResult,
        tpcd::metadata::InstallationResult::kErroneousDtrp, 1);
  }

  // Set an erroneous DTRP.
  {
    me->set_dtrp(tpcd::metadata::Parser::kMaxDtrp + 1);

    ExecFakeComponentInstallation(metadata.SerializeAsString());

    base::HistogramTester histogram_tester;
    EXPECT_FALSE(
        policy()->VerifyInstallation(base::Value::Dict(), install_dir()));
    histogram_tester.ExpectBucketCount(
        kTpcdMetadataInstallationResult,
        tpcd::metadata::InstallationResult::kErroneousDtrp, 1);
  }

  // Set an erroneous DTRP override with a valid DTRP.
  {
    me->set_dtrp(tpcd::metadata::Parser::kMaxDtrp);
    me->set_dtrp_override(tpcd::metadata::Parser::kMaxDtrp + 1);

    ExecFakeComponentInstallation(metadata.SerializeAsString());

    base::HistogramTester histogram_tester;
    EXPECT_FALSE(
        policy()->VerifyInstallation(base::Value::Dict(), install_dir()));
    histogram_tester.ExpectBucketCount(
        kTpcdMetadataInstallationResult,
        tpcd::metadata::InstallationResult::kErroneousDtrp, 1);
  }
}

TEST_P(TpcdMetadataComponentInstallerPolicyTest, ComponentReady_FiresCallback) {
  if (!IsTpcdMetadataGrantsEnabled() || !IsTpcdMetadataStagingEnabled()) {
    GTEST_SKIP() << "Reason: Test parameters instance N/A";
  }

  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  tpcd::metadata::Metadata metadata;
  tpcd::metadata::helpers::AddEntryToMetadata(
      metadata, primary_pattern_spec, secondary_pattern_spec,
      tpcd::metadata::Parser::kSourceCriticalSector);
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
      tpcd::metadata::InstallationResult::kSuccessful, 1);

  policy->ComponentReady(base::Version(), install_dir(), base::Value::Dict());

  run_loop.Run();
}

TEST_P(TpcdMetadataComponentInstallerPolicyTest,
       FeatureDisabled_ComponentReady_DoesNotFireCallback) {
  if (IsTpcdMetadataGrantsEnabled()) {
    GTEST_SKIP() << "Reason: Test parameters instance N/A";
  }

  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  tpcd::metadata::Metadata metadata;
  tpcd::metadata::helpers::AddEntryToMetadata(
      metadata, primary_pattern_spec, secondary_pattern_spec,
      tpcd::metadata::Parser::kSourceCriticalSector);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  ExecFakeComponentInstallation(metadata.SerializeAsString());

  base::RunLoop run_loop;

  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy =
      std::make_unique<TpcdMetadataComponentInstallerPolicy>(
          base::BindLambdaForTesting(
              [&](const std::string& raw_metadata) { NOTREACHED(); }));

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(policy->VerifyInstallation(base::Value::Dict(), install_dir()));
  histogram_tester.ExpectUniqueSample(
      kTpcdMetadataInstallationResult,
      tpcd::metadata::InstallationResult::kSuccessful, 1);

  policy->ComponentReady(base::Version(), install_dir(), base::Value::Dict());

  run_loop.RunUntilIdle();
}

}  // namespace component_updater
