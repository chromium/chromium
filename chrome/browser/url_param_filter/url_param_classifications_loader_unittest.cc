// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/url_param_classifications_loader.h"

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

MATCHER_P(EqualsProto,
          want,
          "Matches an argument against an expected a proto Message.") {
  return arg.SerializeAsString() == want.SerializeAsString();
}

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
  void SetFeatureParams(std::string param) {
    // Note, we can initialize the ScopedFeatureList this way since this
    // unittest is single threaded. If the test is multi-threaded, this would
    // have to be initialized in the tests constructor.

    // With the flag set, the URL should be filtered using this param.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled, {{"classifications", param}});
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

  void AddClassification(FilterClassification* classification,
                         std::string site,
                         FilterClassification_SiteRole role,
                         std::vector<std::string> params) {
    classification->set_site(site);
    classification->set_site_role(role);
    for (const std::string& param : params) {
      FilterParameter* parameters = classification->add_parameters();
      parameters->set_name(param);
    }
  }

  FilterClassifications MakeClassificationsProtoFromMap(
      std::map<std::string, std::vector<std::string>> source_map,
      std::map<std::string, std::vector<std::string>> dest_map) {
    FilterClassifications classifications;
    for (const auto& [site, params] : source_map) {
      AddClassification(classifications.add_classifications(), site,
                        kSourceSiteRole, params);
    }
    for (const auto& [site, params] : dest_map) {
      AddClassification(classifications.add_classifications(), site,
                        kDestinationSiteRole, params);
    }
    return classifications;
  }

  FilterClassification MakeFilterClassification(
      std::string site,
      FilterClassification_SiteRole role,
      std::vector<std::string> params) {
    FilterClassification fc;
    AddClassification(&fc, site, role, params);
    return fc;
  }

  ClassificationsLoader* loader() { return classifications_loader_; }
  std::string test_file_contents() { return raw_test_file_; }

 private:
  ClassificationsLoader* classifications_loader_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::string raw_test_file_;
};

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
  SetFeatureParams(CreateBase64EncodedFilterParamClassificationForTesting(
      {{kSourceSite, {"plzblock1", "plzblock2"}}}, {}));

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
  SetFeatureParams(CreateBase64EncodedFilterParamClassificationForTesting(
      {{kSourceSite, {"plzblockA", "plzblockB"}}}, {}));

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
  SetFeatureParams(CreateBase64EncodedFilterParamClassificationForTesting(
      {{kSourceSite, {"plzblockA", "plzblockB"}}}, {}));

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
  SetFeatureParams(CreateBase64EncodedFilterParamClassificationForTesting(
      {}, {{kDestinationSite, {"plzblock3", "plzblock4"}}}));

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
  SetFeatureParams(CreateBase64EncodedFilterParamClassificationForTesting(
      {}, {{kDestinationSite, {"plzblockA", "plzblockB"}}}));

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
  SetFeatureParams(CreateBase64EncodedFilterParamClassificationForTesting(
      {}, {{kDestinationSite, {"plzblockA", "plzblockB"}}}));

  // Provide classifications from the Component.
  SetComponentFileContents(classifications.SerializeAsString());
  loader()->ReadClassifications(test_file_contents());

  FilterClassification expected = MakeFilterClassification(
      kDestinationSite, kDestinationSiteRole, {"plzblockA", "plzblockB"});
  EXPECT_THAT(
      loader()->GetDestinationClassifications(),
      UnorderedElementsAre(Pair(Eq(kDestinationSite), EqualsProto(expected))));
}

}  // namespace
}  // namespace url_param_filter
