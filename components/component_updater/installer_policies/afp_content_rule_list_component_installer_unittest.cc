// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/afp_content_rule_list_component_installer.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/ios/content_rule_list_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace component_updater {

namespace {

using ::testing::Eq;
using InstallationResult =
    AntiFingerprintingContentRuleListComponentInstallerPolicy::
        InstallationResult;

constexpr char kTestVersion[] = "1.0.0.1";

}  // namespace

class AntiFingerprintingContentRuleListComponentInstallerPolicyTest
    : public PlatformTest {
 public:
  AntiFingerprintingContentRuleListComponentInstallerPolicyTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    // Reset state of global data before each test for proper isolation.
    script_blocking::ContentRuleListData::GetInstance().ResetForTesting();
    ASSERT_TRUE(install_dir_.CreateUniqueTempDir());
    policy_ = std::make_unique<
        AntiFingerprintingContentRuleListComponentInstallerPolicy>(
        on_load_complete_future_.GetRepeatingCallback());
  }

  // Helper method to call the private VerifyInstallation method on the policy.
  bool CallVerifyInstallation(const base::Value::Dict& manifest) {
    return policy_->VerifyInstallation(manifest, install_dir_.GetPath());
  }

  // Helper method to call the private ComponentReady method on the policy.
  void CallComponentReady(const base::Version& version,
                          base::Value::Dict manifest) {
    policy_->ComponentReady(version, install_dir_.GetPath(),
                            std::move(manifest));
  }

  // Helper method to call the private GetInstallerAttributes method on the
  // policy.
  update_client::InstallerAttributes CallGetInstallerAttributes() {
    return policy_->GetInstallerAttributes();
  }

  // Helper method to call the private GetInstalledPath method on the policy.
  base::FilePath CallGetInstalledPath(const base::FilePath& install_dir) {
    return policy_->GetInstalledPath(install_dir);
  }

  // TODO(crbug.com/436881800): For testing only. Remove after the experiment.
  std::string CallTransformJsonForDryRun(std::string json) {
    return AntiFingerprintingContentRuleListComponentInstallerPolicy::
        TransformJsonForDryRun(std::move(json));
  }

 protected:
  void WriteJsonFile(const std::string& content) {
    base::FilePath json_path = CallGetInstalledPath(install_dir_.GetPath());
    ASSERT_TRUE(base::WriteFile(json_path, content));
  }

  base::test::TaskEnvironment task_env_;
  base::ScopedTempDir install_dir_;
  base::test::TestFuture<std::optional<std::string>> on_load_complete_future_;
  std::unique_ptr<AntiFingerprintingContentRuleListComponentInstallerPolicy>
      policy_;
};

// Tests that the component is registered when the master feature is enabled.
TEST_F(AntiFingerprintingContentRuleListComponentInstallerPolicyTest,
       ComponentRegistration_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(fingerprinting_protection_filter::features::
                                        kEnableFingerprintingProtectionFilter);

  base::test::TestFuture<void> future;
  MockComponentUpdateService service;
  EXPECT_CALL(service, RegisterComponent(testing::_))
      .WillOnce([&future](const auto&) {
        future.SetValue();
        return true;
      });

  AntiFingerprintingContentRuleListComponentInstallerPolicy::Register(&service);
  EXPECT_TRUE(future.Wait());
}

// Tests the VerifyInstallation method when the JSON file is present.
TEST_F(AntiFingerprintingContentRuleListComponentInstallerPolicyTest,
       VerifyInstallation_Success) {
  base::HistogramTester histogram_tester;
  WriteJsonFile(R"([{"id": 1}])");
  EXPECT_TRUE(CallVerifyInstallation(base::Value::Dict()));
  histogram_tester.ExpectUniqueSample(
      "FingerprintingProtection.WKContentRuleListComponent.InstallationResult",
      InstallationResult::kSuccess, 1);
}

// Tests the VerifyInstallation method when the JSON file is missing.
TEST_F(AntiFingerprintingContentRuleListComponentInstallerPolicyTest,
       VerifyInstallation_MissingFile) {
  base::HistogramTester histogram_tester;
  // Do not write the file.
  EXPECT_FALSE(CallVerifyInstallation(base::Value::Dict()));
  histogram_tester.ExpectUniqueSample(
      "FingerprintingProtection.WKContentRuleListComponent.InstallationResult",
      InstallationResult::kMissingJsonFile, 1);
}

// Tests that ComponentReady loads the JSON from disk and calls the
// completion callback.
TEST_F(AntiFingerprintingContentRuleListComponentInstallerPolicyTest,
       ComponentReady_CallsOnLoadCompleteCallback) {
  const std::string kTestJson = R"([{"trigger": {"url-filter": ".*" }}])";
  WriteJsonFile(kTestJson);

  CallComponentReady(base::Version(kTestVersion), base::Value::Dict());

  // `ComponentReady` posts a task to read the file. We wait for the future
  // which is tied to the `on_load_complete_` callback.
  std::optional<std::string> result = on_load_complete_future_.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, kTestJson);
}

// Tests that ComponentReady logs a read error if the file exists but cannot
// be read.
TEST_F(AntiFingerprintingContentRuleListComponentInstallerPolicyTest,
       ComponentReady_LogsFileReadError) {
  base::HistogramTester histogram_tester;
  // Create a directory where the file is expected to cause a read error.
  base::FilePath json_path = CallGetInstalledPath(install_dir_.GetPath());
  ASSERT_TRUE(base::CreateDirectory(json_path));

  CallComponentReady(base::Version(kTestVersion), base::Value::Dict());

  // Wait for the async file read to complete.
  std::optional<std::string> result = on_load_complete_future_.Get();
  EXPECT_FALSE(result.has_value());

  // The callback in ComponentReady should have logged the read error.
  histogram_tester.ExpectUniqueSample(
      "FingerprintingProtection.WKContentRuleListComponent.InstallationResult",
      InstallationResult::kFileReadError, 1);
}

// TODO(crbug.com/436881800): Clean up the dry-run feature flag after the
// experiment.
// Tests that the JSON transformation for the dry run mode works correctly.
#if BUILDFLAG(IS_IOS)
TEST_F(AntiFingerprintingContentRuleListComponentInstallerPolicyTest,
       TransformJsonForDryRun) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      fingerprinting_protection_filter::features::
          kEnableFingerprintingProtectionFilteriOSDryRun);

  const std::string kOriginalJson =
      R"([{"action": {"type": "block"}, "trigger": {"url-filter": ".*"}}])";
  const std::string kExpectedJson =
      R"([{"action":{"type":"ignore-previous-rules"},)"
      R"("trigger":{"url-filter":".*"}}])";

  std::string result = CallTransformJsonForDryRun(kOriginalJson);
  EXPECT_EQ(result, kExpectedJson);
}

// Tests that the dry run transformation logs metrics correctly when rules are
// transformed.
TEST_F(AntiFingerprintingContentRuleListComponentInstallerPolicyTest,
       TransformJsonForDryRun_LogsMetrics_SuccessRulesTransformed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      fingerprinting_protection_filter::features::
          kEnableFingerprintingProtectionFilteriOSDryRun);
  base::HistogramTester histogram_tester;

  const std::string kJson =
      R"([{"action": {"type": "block"}}, {"action": {"type": "allow"}}])";
  CallTransformJsonForDryRun(kJson);

  histogram_tester.ExpectUniqueSample(
      "FingerprintingProtection.IOSDryRun.TransformResult",
      AntiFingerprintingContentRuleListComponentInstallerPolicy::
          IOSDryRunTransformResult::kSuccessRulesTransformed,
      1);
  histogram_tester.ExpectUniqueSample(
      "FingerprintingProtection.IOSDryRun.TotalRules", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "FingerprintingProtection.IOSDryRun.SkippedRules", 1, 1);
}

// Tests that the dry run transformation logs metrics correctly when no rules
// are transformed.
TEST_F(AntiFingerprintingContentRuleListComponentInstallerPolicyTest,
       TransformJsonForDryRun_LogsMetrics_SuccessNoRulesToTransform) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      fingerprinting_protection_filter::features::
          kEnableFingerprintingProtectionFilteriOSDryRun);
  base::HistogramTester histogram_tester;

  const std::string kJson =
      R"([{"action": {"type": "allow"}}, {"action": {"type": "allow"}}])";
  CallTransformJsonForDryRun(kJson);

  histogram_tester.ExpectUniqueSample(
      "FingerprintingProtection.IOSDryRun.TransformResult",
      AntiFingerprintingContentRuleListComponentInstallerPolicy::
          IOSDryRunTransformResult::kSuccessNoRulesToTransform,
      1);
  histogram_tester.ExpectUniqueSample(
      "FingerprintingProtection.IOSDryRun.TotalRules", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "FingerprintingProtection.IOSDryRun.SkippedRules", 2, 1);
}

// Tests that the dry run transformation logs a parse failure metric correctly.
TEST_F(AntiFingerprintingContentRuleListComponentInstallerPolicyTest,
       TransformJsonForDryRun_LogsMetrics_JsonParseFailed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      fingerprinting_protection_filter::features::
          kEnableFingerprintingProtectionFilteriOSDryRun);
  base::HistogramTester histogram_tester;

  CallTransformJsonForDryRun("invalid json");

  histogram_tester.ExpectUniqueSample(
      "FingerprintingProtection.IOSDryRun.TransformResult",
      AntiFingerprintingContentRuleListComponentInstallerPolicy::
          IOSDryRunTransformResult::kJsonParseFailed,
      1);
  histogram_tester.ExpectTotalCount(
      "FingerprintingProtection.IOSDryRun.TotalRules", 0);
}
#endif  // BUILDFLAG(IS_IOS)

// Tests that GetInstallerAttributes returns an empty version when features are
// disabled.
TEST_F(AntiFingerprintingContentRuleListComponentInstallerPolicyTest,
       InstallerAttributes_FeaturesDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {fingerprinting_protection_filter::features::
               kEnableFingerprintingProtectionFilterInIncognito,
           fingerprinting_protection_filter::features::
               kEnableFingerprintingProtectionFilter});

  update_client::InstallerAttributes attributes = CallGetInstallerAttributes();
  EXPECT_EQ(
      attributes[AntiFingerprintingContentRuleListComponentInstallerPolicy::
                     kExperimentalVersionAttributeName],
      "");
}

// Tests that GetInstallerAttributes returns the incognito version when the
// incognito feature is enabled.
TEST_F(AntiFingerprintingContentRuleListComponentInstallerPolicyTest,
       InstallerAttributes_IncognitoEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{fingerprinting_protection_filter::features::
            kEnableFingerprintingProtectionFilterInIncognito,
        {{fingerprinting_protection_filter::features::
              kExperimentVersionIncognito.name,
          "incognito_v1"}}}},
      {fingerprinting_protection_filter::features::
           kEnableFingerprintingProtectionFilter});

  update_client::InstallerAttributes attributes = CallGetInstallerAttributes();
  EXPECT_EQ(
      attributes[AntiFingerprintingContentRuleListComponentInstallerPolicy::
                     kExperimentalVersionAttributeName],
      "incognito_v1");
}

// Tests that GetInstallerAttributes returns the standard version when the
// incognito feature is disabled but the standard feature is enabled.
TEST_F(AntiFingerprintingContentRuleListComponentInstallerPolicyTest,
       InstallerAttributes_StandardEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{fingerprinting_protection_filter::features::
            kEnableFingerprintingProtectionFilter,
        {{fingerprinting_protection_filter::features::
              kExperimentVersionNonIncognito.name,
          "standard_v2"}}}},
      {fingerprinting_protection_filter::features::
           kEnableFingerprintingProtectionFilterInIncognito});

  update_client::InstallerAttributes attributes = CallGetInstallerAttributes();
  EXPECT_EQ(
      attributes[AntiFingerprintingContentRuleListComponentInstallerPolicy::
                     kExperimentalVersionAttributeName],
      "standard_v2");
}

}  // namespace component_updater
