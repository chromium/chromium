// Copyright 2022 The Chromium Authors
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
#include "testing/gtest/include/gtest/gtest.h"

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
  EXPECT_THAT(loader()->GetClassifications(), IsEmpty());
}

TEST_F(UrlParamClassificationsLoaderTest,
       ReadClassifications_NonserializedProto) {
  loader()->ReadClassifications("clearly not proto");
  EXPECT_THAT(loader()->GetClassifications(), IsEmpty());
}

TEST_F(UrlParamClassificationsLoaderTest, ReadClassifications_EmptyList) {
  FilterClassifications classifications =
      MakeClassificationsProtoFromMap({}, {});
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());
  EXPECT_THAT(loader()->GetClassifications(), IsEmpty());
}

TEST_F(UrlParamClassificationsLoaderTest,
       ReadClassifications_SiteMatchTypeNotSet_DefaultsToETLDPlusOne) {
  base::HistogramTester histogram_tester;
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1"}}}, {{kDestinationSite, {"plzblock2"}}});

  // Clear out site_match_type set to EXACT_ETLD_PLUS_ONE by helper function.
  for (auto& fc : *classifications.mutable_classifications()) {
    fc.clear_site_match_type();
  }
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceKey(kSourceSite),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(DestinationKey(kDestinationSite),
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
       ReadClassifications_MatchTypeKeyCollision_NonExperimentalTagApplied) {
  base::HistogramTester histogram_tester;
  FilterClassifications classifications;

  // Create a candidate with a normal EXACT_ETLD_PLUS_ONE match type.
  // This candidate would be marked NON_EXPERIMENTAL if added to the map, since
  // it has the "default" tag.
  AddClassification(classifications.add_classifications(), kSourceSite,
                    FilterClassification_SiteRole_SOURCE,
                    FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
                    {"plzblock"}, {FilterClassification::USE_CASE_UNKNOWN},
                    {"default", "not_default"});

  // Create a candidate with an unknown site match type.  This candidate's key
  // in ClassificationMap will collide with the first candidate.
  // This candidate would be marked as EXPERIMENTAL if added to the map, since
  // it has only one tag which is not "default".
  AddClassification(classifications.add_classifications(), kSourceSite,
                    FilterClassification_SiteRole_SOURCE,
                    FilterClassification_SiteMatchType_MATCH_TYPE_UNKNOWN,
                    {"plzblock"}, {FilterClassification::USE_CASE_UNKNOWN},
                    {"not_default"});

  const std::string experiment_identifier = "not_default";
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams params;
  params["experiment_identifier"] = experiment_identifier;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIncognitoParamFilterEnabled, params);
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  // The first entry remains in the map, despite the collision.
  // We can tell the first entry is the one that won because it's tagged
  // NON_EXPERIMENTAL.
  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceKey(kSourceSite),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));

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

TEST_F(
    UrlParamClassificationsLoaderTest,
    ReadClassifications_DuplicateKeysExperimentalThenNonExperimental_NonExperimentalTagApplied) {
  base::HistogramTester histogram_tester;
  FilterClassifications classifications;

  // Two classifications keyed in exactly the same way.
  // The first classification is EXPERIMENTAL but the second is
  // NON_EXPERIMENTAL.
  AddClassification(classifications.add_classifications(), kSourceSite,
                    FilterClassification_SiteRole_SOURCE,
                    FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
                    {"plzblock"}, {FilterClassification::USE_CASE_UNKNOWN},
                    {"not_default"});
  AddClassification(classifications.add_classifications(), kSourceSite,
                    FilterClassification_SiteRole_SOURCE,
                    FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
                    {"plzblock"}, {FilterClassification::USE_CASE_UNKNOWN},
                    {"default", "not_default"});

  const std::string experiment_identifier = "not_default";
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams params;
  params["experiment_identifier"] = experiment_identifier;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIncognitoParamFilterEnabled, params);
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  // The parameter is marked as NON_EXPERIMENTAL.
  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceKey(kSourceSite),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));

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

TEST_F(
    UrlParamClassificationsLoaderTest,
    ReadClassifications_DuplicateKeysNonExperimentalThenExperimental_NonExperimentalTagApplied) {
  base::HistogramTester histogram_tester;
  FilterClassifications classifications;

  // Two classifications keyed in exactly the same way.
  // The first classification is NON_EXPERIMENTAL but the second is
  // EXPERIMENTAL.
  AddClassification(classifications.add_classifications(), kSourceSite,
                    FilterClassification_SiteRole_SOURCE,
                    FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
                    {"plzblock"}, {FilterClassification::USE_CASE_UNKNOWN},
                    {"not_default", "default"});
  AddClassification(classifications.add_classifications(), kSourceSite,
                    FilterClassification_SiteRole_SOURCE,
                    FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
                    {"plzblock"}, {FilterClassification::USE_CASE_UNKNOWN},
                    {"not_default"});

  const std::string experiment_identifier = "not_default";
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams params;
  params["experiment_identifier"] = experiment_identifier;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIncognitoParamFilterEnabled, params);
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  // The parameter is marked as NON_EXPERIMENTAL.
  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceKey(kSourceSite),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));

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
       ReadClassifications_SiteMatchTypeSetToUnknown_DefaultsToETLDPlusOne) {
  base::HistogramTester histogram_tester;
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1"}}}, {{kDestinationSite, {"plzblock2"}}});

  // Clear out site_match_type set to EXACT_ETLD_PLUS_ONE by helper function.
  for (auto& fc : *classifications.mutable_classifications()) {
    fc.set_site_match_type(
        FilterClassification_SiteMatchType_MATCH_TYPE_UNKNOWN);
  }
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceKey(kSourceSite),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(DestinationKey(kDestinationSite),
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

TEST_F(UrlParamClassificationsLoaderTest, ReadClassifications_OnlySources) {
  base::HistogramTester histogram_tester;
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{"source1.xyz", {"plzblock1"}}, {"source2.xyz", {"plzblock2"}}}, {});
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceKey("source1.xyz"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(SourceKey("source2.xyz"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));

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
       ReadClassifications_OnlySourceWildcards) {
  base::HistogramTester histogram_tester;
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{SourceWildcardKey("wildcard1"), {"plzblock1"}},
       {SourceWildcardKey("wildcard2"), {"plzblock2"}}});
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceWildcardKey("wildcard1"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(SourceWildcardKey("wildcard2"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));

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

  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(DestinationKey("destination1.xyz"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(DestinationKey("destination2.xyz"),
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
       ReadClassifications_SourcesAndDestinationsAndWildcards) {
  base::HistogramTester histogram_tester;
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{SourceKey("source1.xyz"), {"plzblock1"}},
       {DestinationKey("destination2.xyz"), {"plzblock2"}},
       {SourceWildcardKey("wildcard"), {"plzblock3"}}});

  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceKey("source1.xyz"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(DestinationKey("destination2.xyz"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(SourceWildcardKey("wildcard"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock3",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));

  histogram_tester.ExpectTotalCount(kApplicableClassificationsSourceMetric, 1);
  ASSERT_EQ(
      histogram_tester.GetTotalSum(kApplicableClassificationsSourceMetric), 2);
  histogram_tester.ExpectTotalCount(kApplicableClassificationsDestinationMetric,
                                    1);
  ASSERT_EQ(
      histogram_tester.GetTotalSum(kApplicableClassificationsDestinationMetric),
      1);
  histogram_tester.ExpectTotalCount(kApplicableClassificationsInvalidMetric, 0);
}

TEST_F(UrlParamClassificationsLoaderTest,
       ReadClassifications_NormalizeToLowercase) {
  FilterClassifications classifications = MakeClassificationsProtoFromMap({
      {SourceKey("source1.xyz"), {"UPPERCASE"}},
      {DestinationKey("destination2.xyz"), {"mixedCase123"}},
  });

  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceKey("source1.xyz"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "uppercase",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(DestinationKey("destination2.xyz"),
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
      MakeFilterClassification(
          kDestinationSite, FilterClassification_SiteRole_DESTINATION,
          FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE, {"plzblock5"},
          {}, experiment_identifier);
  FilterClassification source_experiment_classification =
      MakeFilterClassification(
          kSourceSite, FilterClassification_SiteRole_SOURCE,
          FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE, {"plzblock7"},
          {}, experiment_identifier);
  // These do not match our experiment identifier, so they should not appear in
  // the result.
  FilterClassification inapplicable_destination_experiment_classification =
      MakeFilterClassification(
          kDestinationSite, FilterClassification_SiteRole_DESTINATION,
          FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE, {"plzblock6"},
          {}, "not_our_experiment");
  FilterClassification inapplicable_source_experiment_classification =
      MakeFilterClassification(
          kSourceSite, FilterClassification_SiteRole_SOURCE,
          FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE, {"plzblock8"},
          {}, "not_our_experiment");
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

  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(Eq(DestinationKey(kDestinationSite)),
               UnorderedElementsAre(
                   Pair(FilterClassification::USE_CASE_UNKNOWN,
                        UnorderedElementsAre(Pair(
                            "plzblock5",
                            ClassificationExperimentStatus::EXPERIMENTAL))))),
          Pair(Eq(SourceKey(kSourceSite)),
               UnorderedElementsAre(
                   Pair(FilterClassification::USE_CASE_UNKNOWN,
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

TEST_F(UrlParamClassificationsLoaderTest, GetClassifications_ComponentInvalid) {
  // Provide classifications from the Component.
  SetComponentFileContents("clearly not proto");
  loader()->ReadClassifications(test_file_contents());

  // Invalid classifications list result in an empty ClassificationMap.
  EXPECT_THAT(loader()->GetClassifications(), IsEmpty());
}

TEST_F(UrlParamClassificationsLoaderTest, GetClassifications_ComponentOnly) {
  // Create proto with both Source + Destination Classifications
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}},
      {{kDestinationSite, {"plzblock3", "plzblock4"}}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(
              Eq(SourceKey(kSourceSite)),
              UnorderedElementsAre(Pair(
                  FilterClassification::USE_CASE_UNKNOWN,
                  UnorderedElementsAre(
                      Pair("plzblock1",
                           ClassificationExperimentStatus::NON_EXPERIMENTAL),
                      Pair(
                          "plzblock2",
                          ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(Eq(DestinationKey(kDestinationSite)),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(
                       Pair("plzblock3",
                            ClassificationExperimentStatus::NON_EXPERIMENTAL),
                       Pair("plzblock4", ClassificationExperimentStatus::
                                             NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest, GetClassifications_FeatureOnly) {
  // Provide classifications using the feature flag.
  std::map<std::string, std::vector<std::string>> dest_params = {
      {kDestinationSite, {"plzblock3", "plzblock4"}}};
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            {{kSourceSite, {"plzblock1", "plzblock2"}}}, dest_params)}});

  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(
              Eq(SourceKey(kSourceSite)),
              UnorderedElementsAre(Pair(
                  FilterClassification::USE_CASE_UNKNOWN,
                  UnorderedElementsAre(
                      Pair("plzblock1",
                           ClassificationExperimentStatus::NON_EXPERIMENTAL),
                      Pair(
                          "plzblock2",
                          ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(Eq(DestinationKey(kDestinationSite)),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(
                       Pair("plzblock3",
                            ClassificationExperimentStatus::NON_EXPERIMENTAL),
                       Pair("plzblock4", ClassificationExperimentStatus::
                                             NON_EXPERIMENTAL)))))));
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
      loader()->GetClassifications(),
      UnorderedElementsAre(Pair(
          Eq(SourceKey(kSourceSite)),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("uppercase",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("mixedcase123",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetClassifications_ComponentThenFeature) {
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
      loader()->GetClassifications(),
      UnorderedElementsAre(Pair(
          Eq(SourceKey(kSourceSite)),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblocka",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblockb",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetClassifications_FeatureThenComponent) {
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
      loader()->GetClassifications(),
      UnorderedElementsAre(Pair(
          Eq(SourceKey(kSourceSite)),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblocka",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblockb",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetClassifications_ComponentAndFeatureWithoutParams) {
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
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(
              Eq(SourceKey(kSourceSite)),
              UnorderedElementsAre(Pair(
                  FilterClassification::USE_CASE_UNKNOWN,
                  UnorderedElementsAre(
                      Pair("plzblock1",
                           ClassificationExperimentStatus::NON_EXPERIMENTAL),
                      Pair(
                          "plzblock2",
                          ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(Eq(DestinationKey(kDestinationSite)),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(
                       Pair("plzblock3",
                            ClassificationExperimentStatus::NON_EXPERIMENTAL),
                       Pair("plzblock4", ClassificationExperimentStatus::
                                             NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetClassifications_ComponentAndFeatureWithShouldFilterParamOnly) {
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
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(
              Eq(SourceKey(kSourceSite)),
              UnorderedElementsAre(Pair(
                  FilterClassification::USE_CASE_UNKNOWN,
                  UnorderedElementsAre(
                      Pair("plzblock1",
                           ClassificationExperimentStatus::NON_EXPERIMENTAL),
                      Pair(
                          "plzblock2",
                          ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(Eq(DestinationKey(kDestinationSite)),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(
                       Pair("plzblock3",
                            ClassificationExperimentStatus::NON_EXPERIMENTAL),
                       Pair("plzblock4", ClassificationExperimentStatus::
                                             NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetDestinationClassifications_ComponentInvalid) {
  // Provide classifications from the Component.
  SetComponentFileContents("clearly not proto");
  loader()->ReadClassifications(test_file_contents());

  // Invalid classifications list result in an empty ClassificationMap.
  EXPECT_THAT(loader()->GetClassifications(), IsEmpty());
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetClassifications_ComponentOnlyWithUseCases) {
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
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(
              Eq(SourceKey(kSourceSite)),
              UnorderedElementsAre(Pair(
                  FilterClassification::CROSS_SITE_NO_3PC,
                  UnorderedElementsAre(
                      Pair("plzblock1",
                           ClassificationExperimentStatus::NON_EXPERIMENTAL),
                      Pair(
                          "plzblock2",
                          ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(Eq(DestinationKey(kDestinationSite)),
               UnorderedElementsAre(Pair(
                   FilterClassification::CROSS_OTR,
                   UnorderedElementsAre(
                       Pair("plzblock3",
                            ClassificationExperimentStatus::NON_EXPERIMENTAL),
                       Pair("plzblock4", ClassificationExperimentStatus::
                                             NON_EXPERIMENTAL)))))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetClassifications_FeatureOnlyWithUseCases) {
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
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(
              Eq(DestinationKey(kDestinationSite)),
              UnorderedElementsAre(Pair(
                  FilterClassification::CROSS_OTR,
                  UnorderedElementsAre(
                      Pair("plzblock3",
                           ClassificationExperimentStatus::NON_EXPERIMENTAL),
                      Pair(
                          "plzblock4",
                          ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(Eq(SourceKey(kSourceSite)),
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
      loader()->GetClassifications(),
      UnorderedElementsAre(Pair(
          Eq(DestinationKey(kDestinationSite)),
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
      loader()->GetClassifications(),
      UnorderedElementsAre(Pair(
          Eq(DestinationKey(kDestinationSite)),
          UnorderedElementsAre(Pair(
              FilterClassification::USE_CASE_UNKNOWN,
              UnorderedElementsAre(
                  Pair("plzblocka",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL),
                  Pair("plzblockb",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

}  // namespace
}  // namespace url_param_filter
