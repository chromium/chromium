// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_param_filter/core/url_param_classifications_loader.h"

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/url_param_filter/core/features.h"
#include "components/url_param_filter/core/url_param_filter_classification.pb.h"
#include "components/url_param_filter/core/url_param_filter_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace url_param_filter {
namespace {
constexpr char kApplicableClassificationsSourceMetric[] =
    "Navigation.UrlParamFilter.ApplicableClassificationCount.Source";
constexpr char kApplicableClassificationsDestinationMetric[] =
    "Navigation.UrlParamFilter.ApplicableClassificationCount.Destination";
constexpr char kApplicableClassificationsInvalidMetric[] =
    "Navigation.UrlParamFilter.ApplicableClassificationCount.Invalid";

class UrlParamClassificationsLoaderTest : public ::testing::Test {
 public:
  UrlParamClassificationsLoaderTest() {
    classifications_loader_ = ClassificationsLoader::GetInstance();
  }

  ~UrlParamClassificationsLoaderTest() override {
    classifications_loader_->ResetListsForTesting();
  }

  const std::string kSourceSite = "source.xyz";
  const std::string kDestinationSite = "destination.xyz";
  const FilterClassification_SiteRole kSourceSiteRole =
      FilterClassification_SiteRole_SOURCE;
  const FilterClassification_SiteRole kDestinationSiteRole =
      FilterClassification_SiteRole_DESTINATION;

 protected:
  void SetFeatureParams(const std::map<std::string, std::string>& params_map) {
    // Note, we can initialize the ScopedFeatureList this way since this
    // unittest is single threaded. If the test is multi-threaded, this would
    // have to be initialized in the tests constructor.

    // With the flag set, the URL should be filtered using this param.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled, params_map);
  }

  void SetComponentFileContents(base::StringPiece content) {
    base::ScopedTempDir temp_dir;
    CHECK(temp_dir.CreateUniqueTempDir());
    CHECK(temp_dir.IsValid());

    base::FilePath path =
        temp_dir.GetPath().Append(FILE_PATH_LITERAL("classifications.pb"));
    CHECK(base::WriteFile(path, content));

    std::string file_str;
    CHECK(base::PathExists(path));
    CHECK(base::ReadFileToString(path, &file_str));
    raw_test_file_ = file_str;
  }

  ClassificationsLoader* loader() { return classifications_loader_; }
  std::string test_file_contents() { return raw_test_file_; }

 private:
  raw_ptr<ClassificationsLoader> classifications_loader_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::string raw_test_file_;
};

TEST_F(UrlParamClassificationsLoaderTest,
       GetClassifications_MissingComponentAndFeature) {
  // Neither Component nor feature provide classifications.
  EXPECT_THAT(loader()->GetSourceClassifications(), IsEmpty());
  EXPECT_THAT(loader()->GetDestinationClassifications(), IsEmpty());
}

TEST_F(UrlParamClassificationsLoaderTest,
       ReadClassifications_NonserializedProto) {
  loader()->ReadClassifications("clearly not proto");
  EXPECT_THAT(loader()->GetSourceClassifications(), IsEmpty());
  EXPECT_THAT(loader()->GetDestinationClassifications(), IsEmpty());
}

TEST_F(UrlParamClassificationsLoaderTest, ReadClassifications_EmptyList) {
  FilterClassifications classifications =
      MakeClassificationsProtoFromMap({}, {});
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());
  EXPECT_THAT(loader()->GetSourceClassifications(), IsEmpty());
  EXPECT_THAT(loader()->GetDestinationClassifications(), IsEmpty());
}

TEST_F(UrlParamClassificationsLoaderTest, ReadClassifications_OnlySources) {
  base::HistogramTester histogram_tester;
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{"source1.xyz", {"plzblock1"}}, {"source2.xyz", {"plzblock2"}}}, {});
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(
          Pair("source1.xyz",
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair("source2.xyz",
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
  EXPECT_THAT(loader()->GetDestinationClassifications(), IsEmpty());

  histogram_tester.ExpectTotalCount(kApplicableClassificationsSourceMetric, 1);
  ASSERT_EQ(
      histogram_tester.GetTotalSum(kApplicableClassificationsSourceMetric), 2);
  histogram_tester.ExpectTotalCount(kApplicableClassificationsDestinationMetric,
                                    1);
  ASSERT_EQ(
      histogram_tester.GetTotalSum(kApplicableClassificationsDestinationMetric),
      0);
  histogram_tester.ExpectTotalCount(kApplicableClassificationsInvalidMetric, 0);
}

TEST_F(UrlParamClassificationsLoaderTest,
       ReadClassifications_OnlyDestinations) {
  base::HistogramTester histogram_tester;
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {}, {{"destination1.xyz", {"plzblock1"}},
           {"destination2.xyz", {"plzblock2"}}});

  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(loader()->GetSourceClassifications(), IsEmpty());
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(
          Pair("destination1.xyz",
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair("destination2.xyz",
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
  histogram_tester.ExpectTotalCount(kApplicableClassificationsSourceMetric, 1);
  ASSERT_EQ(
      histogram_tester.GetTotalSum(kApplicableClassificationsSourceMetric), 0);
  histogram_tester.ExpectTotalCount(kApplicableClassificationsDestinationMetric,
                                    1);
  ASSERT_EQ(
      histogram_tester.GetTotalSum(kApplicableClassificationsDestinationMetric),
      2);
  histogram_tester.ExpectTotalCount(kApplicableClassificationsInvalidMetric, 0);
}

TEST_F(UrlParamClassificationsLoaderTest,
       ReadClassifications_SourcesAndDestinations) {
  base::HistogramTester histogram_tester;
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{"source1.xyz", {"plzblock1"}}}, {{"destination2.xyz", {"plzblock2"}}});

  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(
          Pair("source1.xyz",
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(
          Pair("destination2.xyz",
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));

  histogram_tester.ExpectTotalCount(kApplicableClassificationsSourceMetric, 1);
  ASSERT_EQ(
      histogram_tester.GetTotalSum(kApplicableClassificationsSourceMetric), 1);
  histogram_tester.ExpectTotalCount(kApplicableClassificationsDestinationMetric,
                                    1);
  ASSERT_EQ(
      histogram_tester.GetTotalSum(kApplicableClassificationsDestinationMetric),
      1);
  histogram_tester.ExpectTotalCount(kApplicableClassificationsInvalidMetric, 0);
}

TEST_F(UrlParamClassificationsLoaderTest,
       ReadClassifications_NormalizeToLowercase) {
  FilterClassifications classifications =
      MakeClassificationsProtoFromMap({{"source1.xyz", {"UPPERCASE"}}},
                                      {{"destination2.xyz", {"mixedCase123"}}});

  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(
          Pair("source1.xyz",
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "uppercase",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(
          Pair("destination2.xyz",
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "mixedcase123",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetClassifications_ComponentOnlyWithExperiment) {
  base::HistogramTester histogram_tester;
  const std::string experiment_identifier = "mattwashere";
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams params;
  params["experiment_identifier"] = experiment_identifier;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIncognitoParamFilterEnabled, params);

  // Create proto with both Source + Destination Classifications, with the
  // default experiment tag. Because we apply a non-default tag, these should
  // not be present.
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}},
      {{kDestinationSite, {"plzblock3", "plzblock4"}}});
  FilterClassification destination_experiment_classification =
      MakeFilterClassification(kDestinationSite,
                               FilterClassification_SiteRole_DESTINATION,
                               {"plzblock5"}, {}, experiment_identifier);
  FilterClassification source_experiment_classification =
      MakeFilterClassification(kSourceSite,
                               FilterClassification_SiteRole_SOURCE,
                               {"plzblock7"}, {}, experiment_identifier);
  // These do not match our experiment identifier, so they should not appear in
  // the result.
  FilterClassification inapplicable_destination_experiment_classification =
      MakeFilterClassification(kDestinationSite,
                               FilterClassification_SiteRole_DESTINATION,
                               {"plzblock6"}, {}, "not_our_experiment");
  FilterClassification inapplicable_source_experiment_classification =
      MakeFilterClassification(kSourceSite,
                               FilterClassification_SiteRole_SOURCE,
                               {"plzblock8"}, {}, "not_our_experiment");
  *classifications.add_classifications() =
      std::move(destination_experiment_classification);
  *classifications.add_classifications() =
      std::move(source_experiment_classification);
  *classifications.add_classifications() =
      std::move(inapplicable_destination_experiment_classification);
  *classifications.add_classifications() =
      std::move(inapplicable_source_experiment_classification);

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(loader()->GetDestinationClassifications(),
              UnorderedElementsAre(Pair(
                  Eq(kDestinationSite),
                  UnorderedElementsAre(Pair(
                      FilterClassification::USE_CASE_UNKNOWN,
                      UnorderedElementsAre(Pair(
                          "plzblock5",
                          ClassificationExperimentStatus::EXPERIMENTAL)))))));
  EXPECT_THAT(loader()->GetSourceClassifications(),
              UnorderedElementsAre(Pair(
                  Eq(kSourceSite),
                  UnorderedElementsAre(Pair(
                      FilterClassification::USE_CASE_UNKNOWN,
                      UnorderedElementsAre(Pair(
                          "plzblock7",
                          ClassificationExperimentStatus::EXPERIMENTAL)))))));

  // Although there are 6 total classifications, only one source and one
  // destination classification is applicable due to the experiment override.
  histogram_tester.ExpectTotalCount(kApplicableClassificationsSourceMetric, 1);
  ASSERT_EQ(
      histogram_tester.GetTotalSum(kApplicableClassificationsSourceMetric), 1);
  histogram_tester.ExpectTotalCount(kApplicableClassificationsDestinationMetric,
                                    1);
  ASSERT_EQ(
      histogram_tester.GetTotalSum(kApplicableClassificationsDestinationMetric),
      1);
  histogram_tester.ExpectTotalCount(kApplicableClassificationsInvalidMetric, 0);
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetSourceClassifications_NoSourceClassificationsProvided) {
  // Create proto with only Destination classifications.
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {}, {{kDestinationSite, {"plzblock3", "plzblock4"}}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  // No source classifications were loaded.
  EXPECT_THAT(loader()->GetSourceClassifications(), IsEmpty());

  // Provide classifications from the feature.
  std::map<std::string, std::vector<std::string>> source_params;
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            source_params, {{kDestinationSite, {"plzblock3", "plzblock4"}}})}});

  // No source classifications were loaded.
  EXPECT_THAT(loader()->GetSourceClassifications(), IsEmpty());
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetSourceClassifications_ComponentInvalid) {
  // Provide classifications from the Component.
  SetComponentFileContents("clearly not proto");
  loader()->ReadClassifications(test_file_contents());

  // Invalid classifications list result in an empty ClassificationMap.
  EXPECT_THAT(loader()->GetSourceClassifications(), IsEmpty());
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetSourceClassifications_ComponentOnly) {
  // Create proto with both Source + Destination Classifications
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}},
      {{kDestinationSite, {"plzblock3", "plzblock4"}}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kSourceSite),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetSourceClassifications_FeatureOnly) {
  // Provide classifications using the feature flag.
  std::map<std::string, std::vector<std::string>> dest_params;
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            {{kSourceSite, {"plzblock1", "plzblock2"}}}, dest_params)}});

  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kSourceSite),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetSourceClassifications_FeatureOnly_NormalizeToLowercase) {
  // Provide classifications using the feature flag.
  std::map<std::string, std::vector<std::string>> dest_params;
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            {{kSourceSite, {"UPPERCASE", "mixedCase123"}}}, dest_params)}});

  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kSourceSite),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("uppercase",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("mixedcase123",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetSourceClassifications_ComponentThenFeature) {
  // Create proto with both Source + Destination Classifications
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}},
      {{kDestinationSite, {"plzblock3", "plzblock4"}}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  // Provide classifications using the feature flag.
  std::map<std::string, std::vector<std::string>> dest_params;
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            {{kSourceSite, {"plzblockA", "plzblockB"}}}, dest_params)}});

  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kSourceSite),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblocka",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblockb",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetSourceClassifications_FeatureThenComponent) {
  // Create proto with both Source + Destination Classifications
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}},
      {{kDestinationSite, {"plzblock3", "plzblock4"}}});

  // Provide classifications using the feature flag.
  std::map<std::string, std::vector<std::string>> dest_params;
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            {{kSourceSite, {"plzblockA", "plzblockB"}}}, dest_params)}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kSourceSite),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblocka",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblockb",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetSourceClassifications_ComponentAndFeatureWithoutParams) {
  // Create proto with both Source + Destination Classifications
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}},
      {{kDestinationSite, {"plzblock3", "plzblock4"}}});

  // Don't provide classifications using the feature flag.
  SetFeatureParams({{}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  // Expect that Component classifications are returned since no feature
  // classifications were provided.
  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kSourceSite),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetSourceClassifications_ComponentAndFeatureWithShouldFilterParamOnly) {
  // Create proto with both Source + Destination Classifications
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}},
      {{kDestinationSite, {"plzblock3", "plzblock4"}}});

  // Don't provide classifications using the feature flag.
  SetFeatureParams({{"should_filter", "true"}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  // Expect that Component classifications are returned since no feature
  // classifications were provided.
  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kSourceSite),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetDestinationClassifications_NoDestinationClassificationsProvided) {
  // Create proto with only Source classifications.
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}}, {});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  // No destination classifications were loaded.
  EXPECT_THAT(loader()->GetDestinationClassifications(), IsEmpty());

  // Provide classifications from the feature.
  std::map<std::string, std::vector<std::string>> destination_params;
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            {{kSourceSite, {"plzblock1", "plzblock2"}}}, destination_params)}});

  // No destination classifications were loaded.
  EXPECT_THAT(loader()->GetDestinationClassifications(), IsEmpty());
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetDestinationClassifications_ComponentInvalid) {
  // Provide classifications from the Component.
  SetComponentFileContents("clearly not proto");
  loader()->ReadClassifications(test_file_contents());

  // Invalid classifications list result in an empty ClassificationMap.
  EXPECT_THAT(loader()->GetDestinationClassifications(), IsEmpty());
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetDestinationClassifications_ComponentOnly) {
  // Create proto with both Source + Destination Classifications
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}},
      {{kDestinationSite, {"plzblock3", "plzblock4"}}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kDestinationSite),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblock3",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblock4",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetDestinationClassifications_ComponentOnlyWithUseCases) {
  // Create proto with both Source + Destination Classifications
  FilterClassifications classifications =
      MakeClassificationsProtoFromMapWithUseCases(
          {{kSourceSite,
            {{FilterClassification::CROSS_SITE_NO_3PC,
              {"plzblock1", "plzblock2"}}}}},
          {{kDestinationSite,
            {{FilterClassification::CROSS_OTR, {"plzblock3", "plzblock4"}}}}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kSourceSite),
          UnorderedElementsAre(Pair(
              FilterClassification::CROSS_SITE_NO_3PC,
              UnorderedElementsAre(
                  Pair("plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kDestinationSite),
          UnorderedElementsAre(Pair(
              FilterClassification::CROSS_OTR,
              UnorderedElementsAre(
                  Pair("plzblock3",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblock4",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetDestinationClassifications_FeatureOnly) {
  // Provide classifications using the feature flag.
  std::map<std::string, std::vector<std::string>> source_params;
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            source_params, {{kDestinationSite, {"plzblock3", "plzblock4"}}})}});

  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kDestinationSite),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblock3",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblock4",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetDestinationClassifications_FeatureOnlyWithUseCases) {
  // Provide classifications using the feature flag.
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            {{kSourceSite,
              {{FilterClassification::CROSS_SITE_NO_3PC, {"plzblock"}}}}},
            {{kDestinationSite,
              {{FilterClassification::CROSS_OTR,
                {"plzblock3", "plzblock4"}}}}})}});

  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kDestinationSite),
          UnorderedElementsAre(Pair(
              FilterClassification::CROSS_OTR,
              UnorderedElementsAre(
                  Pair("plzblock3",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblock4",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(
          Pair(Eq(kSourceSite),
               UnorderedElementsAre(Pair(
                   FilterClassification::CROSS_SITE_NO_3PC,
                   UnorderedElementsAre(Pair(
                       "plzblock",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetDestinationClassifications_ComponentThenFeature) {
  // Create proto with both Source + Destination Classifications
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}},
      {{kDestinationSite, {"plzblock3", "plzblock4"}}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  // Provide classifications using the feature flag.
  std::map<std::string, std::vector<std::string>> source_params;
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            source_params, {{kDestinationSite, {"plzblockA", "plzblockB"}}})}});

  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kDestinationSite),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblocka",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblockb",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetDestinationClassifications_FeatureThenComponent) {
  // Create proto with both Source + Destination Classifications
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}},
      {{kDestinationSite, {"plzblock3", "plzblock4"}}});

  // Provide classifications using the feature flag.
  std::map<std::string, std::vector<std::string>> source_params;
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            source_params, {{kDestinationSite, {"plzblockA", "plzblockB"}}})}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kDestinationSite),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblocka",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblockb",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetDestinationClassifications_ComponentAndFeatureWithoutParams) {
  // Create proto with both Source + Destination Classifications
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}},
      {{kDestinationSite, {"plzblock3", "plzblock4"}}});

  // Don't provide classifications using the feature flag.
  SetFeatureParams({{}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  // Expect that Component classifications are returned since no feature
  // classifications were provided.
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kDestinationSite),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblock3",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblock4",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(
    UrlParamClassificationsLoaderTest,
    GetDestinationClassifications_ComponentAndFeatureWithShouldFilterParamOnly) {
  // Create proto with both Source + Destination Classifications
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}},
      {{kDestinationSite, {"plzblock3", "plzblock4"}}});

  // Don't provide classifications using the feature flag.
  SetFeatureParams({{"should_filter", "true"}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  // Expect that Component classifications are returned since no feature
  // classifications were provided.
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(
          Eq(kDestinationSite),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblock3",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblock4",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

}  // namespace
}  // namespace url_param_filter
