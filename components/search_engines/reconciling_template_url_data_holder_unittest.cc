// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/reconciling_template_url_data_holder.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url_data.h"
#include "testing/gtest/include/gtest/gtest.h"

class ReconcilingTemplateURLDataHolderTest : public testing::Test {
 public:
  ReconcilingTemplateURLDataHolderTest()
      : holder_(
            &search_engines_test_environment_.pref_service(),
            &search_engines_test_environment_.search_engine_choice_service()) {}

  void SetUp() override {
    // Ensure Top Search Engine definitions consistently reported for the US.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "US");
  }

 protected:
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  ReconcilingTemplateURLDataHolder holder_;
};

TEST_F(ReconcilingTemplateURLDataHolderTest, Set_SafeWithEmptyPointer) {
  holder_.SetAndReconcile({});
  ASSERT_EQ(nullptr, holder_.Get());
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       GetOrComputeKeyword_Get_NonEligiblePlayKeyword) {
  auto supplied_engine = GenerateDummyTemplateURLData("searchengine.com");
  supplied_engine->created_from_play_api = true;
  supplied_engine->SetURL("https://de.yahoo.com");
  holder_.SetSearchEngineBypassingReconciliationForTesting(
      std::move(supplied_engine));

  auto [keyword, is_generated] = holder_.GetOrComputeKeyword();
  ASSERT_EQ(keyword, u"searchengine.com");
  ASSERT_FALSE(is_generated);
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       GetOrComputeKeyword_Get_NonEligibleNotFromPlay) {
  auto supplied_engine = GenerateDummyTemplateURLData("yahoo.com");
  supplied_engine->created_from_play_api = false;
  supplied_engine->SetURL("https://de.yahoo.com");
  holder_.SetSearchEngineBypassingReconciliationForTesting(
      std::move(supplied_engine));

  auto [keyword, is_generated] = holder_.GetOrComputeKeyword();
  ASSERT_EQ(keyword, u"yahoo.com");
  ASSERT_FALSE(is_generated);
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       GetOrComputeKeyword_Computed_EligibleFromPlay_Yahoo) {
  auto supplied_engine = GenerateDummyTemplateURLData("yahoo.com");
  supplied_engine->created_from_play_api = true;
  supplied_engine->SetURL("https://de.yahoo.com");
  holder_.SetSearchEngineBypassingReconciliationForTesting(
      std::move(supplied_engine));

  auto [keyword, is_generated] = holder_.GetOrComputeKeyword();
  ASSERT_EQ(keyword, u"de.yahoo.com");
  ASSERT_TRUE(is_generated);
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       GetOrComputeKeyword_Computed_EligibleFromPlay_Seznam) {
  const char* const variants[] = {"seznam.cz", "seznam.sk"};

  for (const auto* variant : variants) {
    auto supplied_engine = GenerateDummyTemplateURLData(variant);
    supplied_engine->created_from_play_api = true;
    holder_.SetSearchEngineBypassingReconciliationForTesting(
        std::move(supplied_engine));

    auto [keyword, is_generated] = holder_.GetOrComputeKeyword();
    ASSERT_EQ(keyword, u"seznam");
    ASSERT_TRUE(is_generated);
  }
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       FindMatchingBuiltInDefinitionsById_UnknownID) {
  auto engine = holder_.FindMatchingBuiltInDefinitionsById(~0);
  // Expect to see no definitions.
  ASSERT_FALSE(engine);
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       FindMatchingBuiltInDefinitionsById_ValidID_CountryAppropriate) {
  auto engine = holder_.FindMatchingBuiltInDefinitionsById(/* duckduckgo */ 92);
  ASSERT_TRUE(engine);
  ASSERT_EQ(u"duckduckgo.com", engine->keyword());
}

TEST_F(
    ReconcilingTemplateURLDataHolderTest,
    FindMatchingBuiltInDefinitionsById_ValidID_FromPrepopulatedEngines_GlobalReonciliationDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      switches::kTemplateUrlReconciliation,
      {{switches::kReconcileWithAllKnownEngines.name, "false"}});

  auto engine =
      holder_.FindMatchingBuiltInDefinitionsById(/* search.brave.com */ 109);

  ASSERT_FALSE(engine);
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       FindMatchingBuiltInDefinitionsById_ValidID_FromPrepopulatedEngines) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      switches::kTemplateUrlReconciliation,
      {{switches::kReconcileWithAllKnownEngines.name, "true"}});

  auto engine =
      holder_.FindMatchingBuiltInDefinitionsById(/* search.brave.com */ 109);

  ASSERT_TRUE(engine);
  ASSERT_EQ(u"search.brave.com", engine->keyword());
  ASSERT_EQ(109, engine->prepopulate_id);
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       FindMatchingBuiltInDefinitionsByKeyword_UnknownKeyword) {
  auto engine = holder_.FindMatchingBuiltInDefinitionsByKeyword(u"bazzinga");
  // Expect to see no definitions.
  ASSERT_FALSE(engine);
}

TEST_F(
    ReconcilingTemplateURLDataHolderTest,
    FindMatchingBuiltInDefinitionsByKeyword_ValidKeyword_CountryAppropriate) {
  auto engine =
      holder_.FindMatchingBuiltInDefinitionsByKeyword(u"duckduckgo.com");
  ASSERT_TRUE(engine);
  ASSERT_EQ(u"duckduckgo.com", engine->keyword());
}

TEST_F(
    ReconcilingTemplateURLDataHolderTest,
    FindMatchingBuiltInDefinitionsByKeyword_ValidKeyword_FromPrepopulatedEngines_GlobalReonciliationDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      switches::kTemplateUrlReconciliation,
      {{switches::kReconcileWithAllKnownEngines.name, "false"}});

  auto engine =
      holder_.FindMatchingBuiltInDefinitionsByKeyword(u"search.brave.com");

  ASSERT_FALSE(engine);
}

TEST_F(
    ReconcilingTemplateURLDataHolderTest,
    FindMatchingBuiltInDefinitionsByKeyword_ValidKeyword_FromPrepopulatedEngines) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      switches::kTemplateUrlReconciliation,
      {{switches::kReconcileWithAllKnownEngines.name, "true"}});

  auto engine =
      holder_.FindMatchingBuiltInDefinitionsByKeyword(u"search.brave.com");

  ASSERT_TRUE(engine);
  ASSERT_EQ(u"search.brave.com", engine->keyword());
  ASSERT_EQ(109, engine->prepopulate_id);
}
