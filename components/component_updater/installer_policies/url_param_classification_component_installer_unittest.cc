// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/url_param_classification_component_installer.h"

#include <memory>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/url_param_filter/core/features.h"
#include "components/url_param_filter/core/url_param_classifications_loader.h"
#include "components/url_param_filter/core/url_param_filter_classification.pb.h"
#include "components/url_param_filter/core/url_param_filter_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace component_updater {
namespace {

MATCHER_P(EqualsProto,
          want,
          "Matches an argument against an expected a proto Message.") {
  return arg.SerializeAsString() == want.SerializeAsString();
}

class UrlParamClassificationComponentInstallerTest : public ::testing::Test {
 public:
  UrlParamClassificationComponentInstallerTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
    CHECK(component_install_dir_.IsValid());
  }

  ~UrlParamClassificationComponentInstallerTest() override = default;

  const std::string kClassificationListValidationResultHistogram =
      "Navigation.UrlParamFilter.ClassificationListValidationResult";
  const std::string kSourceSite = "source.xyz";
  const std::string kDestinationSite = "destination.xyz";
  const url_param_filter::FilterClassification_SiteRole kSourceSiteRole =
      url_param_filter::FilterClassification_SiteRole_SOURCE;
  const url_param_filter::FilterClassification_SiteRole kDestinationSiteRole =
      url_param_filter::FilterClassification_SiteRole_DESTINATION;

 protected:
  using ClassificationListValidationResult =
      UrlParamClassificationComponentInstallerPolicy::
          ClassificationListValidationResult;

  void SetComponentFileContents(base::StringPiece content) {
    base::FilePath path =
        component_install_dir_.GetPath().Append(FILE_PATH_LITERAL("list.pb"));
    CHECK(base::WriteFile(path, content));
    CHECK(base::PathExists(path));
  }

  const base::FilePath& path() { return component_install_dir_.GetPath(); }

 private:
  base::ScopedTempDir component_install_dir_;
};

class UrlParamClassificationComponentInstallerFeatureAgnosticTest
    : public UrlParamClassificationComponentInstallerTest,
      public ::testing::WithParamInterface<bool> {
 public:
  UrlParamClassificationComponentInstallerFeatureAgnosticTest() {
    if (GetParam()) {
      scoped_list_.InitAndEnableFeature(
          url_param_filter::features::kIncognitoParamFilterEnabled);
    } else {
      scoped_list_.InitAndDisableFeature(
          url_param_filter::features::kIncognitoParamFilterEnabled);
    }
  }

  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy =
      std::make_unique<UrlParamClassificationComponentInstallerPolicy>(
          base::DoNothing());

 private:
  base::test::ScopedFeatureList scoped_list_;
};

TEST_P(UrlParamClassificationComponentInstallerFeatureAgnosticTest,
       ComponentRegistered) {
  base::test::TaskEnvironment task_env;
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();

  EXPECT_CALL(*service, RegisterComponent(_)).Times(1);
  RegisterUrlParamClassificationComponent(service.get());

  task_env.RunUntilIdle();
}

TEST_P(UrlParamClassificationComponentInstallerFeatureAgnosticTest,
       VerifyInstallation_RejectsMissingFile) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      kClassificationListValidationResultHistogram, 0);

  // Don't write the classifications file
  EXPECT_FALSE(policy->VerifyInstallation(base::Value(), path()));

  histogram_tester.ExpectBucketCount(
      kClassificationListValidationResultHistogram,
      ClassificationListValidationResult::kMissingClassificationsFile, 1);
}

TEST_P(UrlParamClassificationComponentInstallerFeatureAgnosticTest,
       VerifyInstallation_RejectsNotProtoFile) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      kClassificationListValidationResultHistogram, 0);

  SetComponentFileContents("clearly not expected proto");
  EXPECT_FALSE(policy->VerifyInstallation(base::Value(), path()));

  histogram_tester.ExpectBucketCount(
      kClassificationListValidationResultHistogram,
      ClassificationListValidationResult::kParsingToProtoFailed, 1);
}

TEST_P(UrlParamClassificationComponentInstallerFeatureAgnosticTest,
       VerifyInstallation_RejectsMissingSiteNameClassification) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      kClassificationListValidationResultHistogram, 0);

  url_param_filter::FilterClassifications classifications =
      url_param_filter::MakeClassificationsProtoFromMap(
          {{"source1.xyz", {"plzblock1"}}},
          {{"destination2.xyz", {"plzblock2"}}});
  SetComponentFileContents(classifications.SerializeAsString());

  // Accepts valid classifications.
  EXPECT_TRUE(policy->VerifyInstallation(base::Value(), path()));
  histogram_tester.ExpectBucketCount(
      kClassificationListValidationResultHistogram,
      ClassificationListValidationResult::kSuccessful, 1);

  // Add classification with a missing required site name.
  classifications.add_classifications()->set_site_role(kSourceSiteRole);
  SetComponentFileContents(classifications.SerializeAsString());

  // VerifyInstallation fails now that an invalid classification was added.
  EXPECT_FALSE(policy->VerifyInstallation(base::Value(), path()));
  histogram_tester.ExpectBucketCount(
      kClassificationListValidationResultHistogram,
      ClassificationListValidationResult::kClassificationMissingSite, 1);
}

TEST_P(UrlParamClassificationComponentInstallerFeatureAgnosticTest,
       VerifyInstallation_RejectsMissingSiteRoleClassification) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      kClassificationListValidationResultHistogram, 0);

  url_param_filter::FilterClassifications classifications =
      url_param_filter::MakeClassificationsProtoFromMap(
          {{"source1.xyz", {"plzblock1"}}},
          {{"destination2.xyz", {"plzblock2"}}});
  SetComponentFileContents(classifications.SerializeAsString());

  // Accepts valid classifications.
  EXPECT_TRUE(policy->VerifyInstallation(base::Value(), path()));
  histogram_tester.ExpectBucketCount(
      kClassificationListValidationResultHistogram,
      ClassificationListValidationResult::kSuccessful, 1);

  // Add classification with a missing required site role.
  classifications.add_classifications()->set_site(kSourceSite);
  SetComponentFileContents(classifications.SerializeAsString());

  // VerifyInstallation fails now that an invalid classification was added.
  EXPECT_FALSE(policy->VerifyInstallation(base::Value(), path()));
  histogram_tester.ExpectBucketCount(
      kClassificationListValidationResultHistogram,
      ClassificationListValidationResult::kClassificationMissingSiteRole, 1);
}

TEST_P(UrlParamClassificationComponentInstallerFeatureAgnosticTest,
       VerifyInstallation_AcceptsEmptyList) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      kClassificationListValidationResultHistogram, 0);

  url_param_filter::FilterClassifications classifications =
      url_param_filter::MakeClassificationsProtoFromMap({}, {});
  SetComponentFileContents(classifications.SerializeAsString());

  // Accepts valid classifications.
  EXPECT_TRUE(policy->VerifyInstallation(base::Value(), path()));
  histogram_tester.ExpectBucketCount(
      kClassificationListValidationResultHistogram,
      ClassificationListValidationResult::kSuccessful, 1);
}

TEST_P(UrlParamClassificationComponentInstallerFeatureAgnosticTest,
       VerifyInstallation_AcceptsValidList) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      kClassificationListValidationResultHistogram, 0);

  url_param_filter::FilterClassifications classifications =
      url_param_filter::MakeClassificationsProtoFromMap(
          {{"source1.xyz", {"plzblock1"}}},
          {{"destination2.xyz", {"plzblock2"}}});
  SetComponentFileContents(classifications.SerializeAsString());

  // Accepts valid classifications.
  EXPECT_TRUE(policy->VerifyInstallation(base::Value(), path()));
  histogram_tester.ExpectBucketCount(
      kClassificationListValidationResultHistogram,
      ClassificationListValidationResult::kSuccessful, 1);
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    UrlParamClassificationComponentInstallerFeatureAgnosticTest,
    ::testing::Bool());

TEST_F(UrlParamClassificationComponentInstallerTest,
       FeatureEnabled_ComponentReady_FiresCallback) {
  base::test::TaskEnvironment task_env;
  base::test::ScopedFeatureList scoped_list;

  scoped_list.InitAndEnableFeatureWithParameters(
      url_param_filter::features::kIncognitoParamFilterEnabled,
      {{"classifications",
        url_param_filter::
            CreateBase64EncodedFilterParamClassificationForTesting(
                {{"f_source.xyz", {"f_block_1"}}},
                {{"f_destination.xyz", {"f_block_2"}}})}});

  // Provide a valid classification list to the file.
  url_param_filter::FilterClassifications classifications =
      url_param_filter::MakeClassificationsProtoFromMap(
          {{"cu_source.xyz", {"plzblock1"}}},
          {{"cu_destination.xyz", {"plzblock2"}}});
  SetComponentFileContents(classifications.SerializeAsString());

  base::RunLoop run_loop;
  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy =
      std::make_unique<UrlParamClassificationComponentInstallerPolicy>(
          base::BindLambdaForTesting([&](std::string raw_classifications) {
            EXPECT_EQ(raw_classifications, classifications.SerializeAsString());
            run_loop.Quit();
          }));

  ASSERT_TRUE(policy->VerifyInstallation(base::Value(), path()));
  // The callback to set the classifications on the ClassificationsLoader should
  // be called.
  policy->ComponentReady(base::Version(), path(), base::Value());
  run_loop.Run();
}

TEST_F(UrlParamClassificationComponentInstallerTest,
       FeatureDisabled_ComponentReady_DoesntFireCallback) {
  base::test::TaskEnvironment task_env;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      url_param_filter::features::kIncognitoParamFilterEnabled);

  base::RunLoop run_loop;
  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy =
      std::make_unique<UrlParamClassificationComponentInstallerPolicy>(
          base::BindLambdaForTesting([&](std::string raw_classifications) {
            ASSERT_TRUE(false);
            run_loop.Quit();
          }));

  // Provide a valid classification list.
  url_param_filter::FilterClassifications classifications =
      url_param_filter::MakeClassificationsProtoFromMap(
          {{"cu_source.xyz", {"plzblock1"}}},
          {{"cu_destination.xyz", {"plzblock2"}}});
  SetComponentFileContents(classifications.SerializeAsString());
  ASSERT_TRUE(policy->VerifyInstallation(base::Value(), path()));

  // The callback to set the classifications on the ClassificationsLoader should
  // never be called.
  policy->ComponentReady(base::Version(), path(), base::Value());
  run_loop.RunUntilIdle();
}

}  // namespace
}  // namespace component_updater
