// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/url_param_classifications_loader.h"

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/url_param_filter/url_param_filter_classification.pb.h"
#include "chrome/browser/url_param_filter/url_param_filter_test_helper.h"
#include "chrome/common/chrome_features.h"
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
  ClassificationsLoader* classifications_loader_;
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
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{"source1.xyz", {"plzblock1"}}, {"source2.xyz", {"plzblock2"}}}, {});
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  FilterClassification expected_source_1 =
      MakeFilterClassification("source1.xyz", kSourceSiteRole, {"plzblock1"});
  FilterClassification expected_source_2 =
      MakeFilterClassification("source2.xyz", kSourceSiteRole, {"plzblock2"});
  EXPECT_THAT(loader()->GetSourceClassifications(),
              UnorderedElementsAre(
                  Pair("source1.xyz", EqualsProto(expected_source_1)),
                  Pair("source2.xyz", EqualsProto(expected_source_2))));
  EXPECT_THAT(loader()->GetDestinationClassifications(), IsEmpty());
}

TEST_F(UrlParamClassificationsLoaderTest,
       ReadClassifications_OnlyDestinations) {
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {}, {{"destination1.xyz", {"plzblock1"}},
           {"destination2.xyz", {"plzblock2"}}});

  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  FilterClassification expected_destination_1 = MakeFilterClassification(
      "destination1.xyz", kDestinationSiteRole, {"plzblock1"});
  FilterClassification expected_destination_2 = MakeFilterClassification(
      "destination2.xyz", kDestinationSiteRole, {"plzblock2"});
  EXPECT_THAT(loader()->GetSourceClassifications(), IsEmpty());
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(
          Pair("destination1.xyz", EqualsProto(expected_destination_1)),
          Pair("destination2.xyz", EqualsProto(expected_destination_2))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       ReadClassifications_SourcesAndDestinations) {
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{"source1.xyz", {"plzblock1"}}}, {{"destination2.xyz", {"plzblock2"}}});

  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  FilterClassification expected_source =
      MakeFilterClassification("source1.xyz", kSourceSiteRole, {"plzblock1"});
  FilterClassification expected_destination = MakeFilterClassification(
      "destination2.xyz", kDestinationSiteRole, {"plzblock2"});
  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair("source1.xyz", EqualsProto(expected_source))));
  EXPECT_THAT(loader()->GetDestinationClassifications(),
              UnorderedElementsAre(
                  Pair("destination2.xyz", EqualsProto(expected_destination))));
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
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            {}, {{kDestinationSite, {"plzblock3", "plzblock4"}}})}});

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

  FilterClassification expected = MakeFilterClassification(
      kSourceSite, kSourceSiteRole, {"plzblock1", "plzblock2"});
  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair(Eq(kSourceSite), EqualsProto(expected))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetSourceClassifications_FeatureOnly) {
  // Provide classifications using the feature flag.
  SetFeatureParams({{"classifications",
                     CreateBase64EncodedFilterParamClassificationForTesting(
                         {{kSourceSite, {"plzblock1", "plzblock2"}}}, {})}});

  FilterClassification expected = MakeFilterClassification(
      kSourceSite, kSourceSiteRole, {"plzblock1", "plzblock2"});
  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair(Eq(kSourceSite), EqualsProto(expected))));
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
  SetFeatureParams({{"classifications",
                     CreateBase64EncodedFilterParamClassificationForTesting(
                         {{kSourceSite, {"plzblockA", "plzblockB"}}}, {})}});

  FilterClassification expected = MakeFilterClassification(
      kSourceSite, kSourceSiteRole, {"plzblockA", "plzblockB"});
  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair(Eq(kSourceSite), EqualsProto(expected))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetSourceClassifications_FeatureThenComponent) {
  // Create proto with both Source + Destination Classifications
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}},
      {{kDestinationSite, {"plzblock3", "plzblock4"}}});

  // Provide classifications using the feature flag.
  SetFeatureParams({{"classifications",
                     CreateBase64EncodedFilterParamClassificationForTesting(
                         {{kSourceSite, {"plzblockA", "plzblockB"}}}, {})}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  FilterClassification expected = MakeFilterClassification(
      kSourceSite, kSourceSiteRole, {"plzblockA", "plzblockB"});
  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair(Eq(kSourceSite), EqualsProto(expected))));
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
  FilterClassification expected = MakeFilterClassification(
      kSourceSite, kSourceSiteRole, {"plzblock1", "plzblock2"});
  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair(Eq(kSourceSite), EqualsProto(expected))));
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
  FilterClassification expected = MakeFilterClassification(
      kSourceSite, kSourceSiteRole, {"plzblock1", "plzblock2"});
  EXPECT_THAT(
      loader()->GetSourceClassifications(),
      UnorderedElementsAre(Pair(Eq(kSourceSite), EqualsProto(expected))));
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
  SetFeatureParams({{"classifications",
                     CreateBase64EncodedFilterParamClassificationForTesting(
                         {{kSourceSite, {"plzblock1", "plzblock2"}}}, {})}});

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

  FilterClassification expected = MakeFilterClassification(
      kDestinationSite, kDestinationSiteRole, {"plzblock3", "plzblock4"});
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(Eq(kDestinationSite), EqualsProto(expected))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetDestinationClassifications_FeatureOnly) {
  // Provide classifications using the feature flag.
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            {}, {{kDestinationSite, {"plzblock3", "plzblock4"}}})}});

  FilterClassification expected = MakeFilterClassification(
      kDestinationSite, kDestinationSiteRole, {"plzblock3", "plzblock4"});
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(Eq(kDestinationSite), EqualsProto(expected))));
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
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            {}, {{kDestinationSite, {"plzblockA", "plzblockB"}}})}});

  FilterClassification expected = MakeFilterClassification(
      kDestinationSite, kDestinationSiteRole, {"plzblockA", "plzblockB"});
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(Eq(kDestinationSite), EqualsProto(expected))));
}

TEST_F(UrlParamClassificationsLoaderTest,
       GetDestinationClassifications_FeatureThenComponent) {
  // Create proto with both Source + Destination Classifications
  FilterClassifications classifications = MakeClassificationsProtoFromMap(
      {{kSourceSite, {"plzblock1", "plzblock2"}}},
      {{kDestinationSite, {"plzblock3", "plzblock4"}}});

  // Provide classifications using the feature flag.
  SetFeatureParams(
      {{"classifications",
        CreateBase64EncodedFilterParamClassificationForTesting(
            {}, {{kDestinationSite, {"plzblockA", "plzblockB"}}})}});

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  FilterClassification expected = MakeFilterClassification(
      kDestinationSite, kDestinationSiteRole, {"plzblockA", "plzblockB"});
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(Eq(kDestinationSite), EqualsProto(expected))));
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
  FilterClassification expected = MakeFilterClassification(
      kDestinationSite, kDestinationSiteRole, {"plzblock3", "plzblock4"});
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(Eq(kDestinationSite), EqualsProto(expected))));
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
  FilterClassification expected = MakeFilterClassification(
      kDestinationSite, kDestinationSiteRole, {"plzblock3", "plzblock4"});
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(Eq(kDestinationSite), EqualsProto(expected))));
}

}  // namespace
}  // namespace url_param_filter
