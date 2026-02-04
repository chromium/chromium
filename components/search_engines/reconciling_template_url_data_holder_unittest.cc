// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/reconciling_template_url_data_holder.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

namespace {

using ::TemplateURLPrepopulateData::brave;
using ::TemplateURLPrepopulateData::duckduckgo;
using ::TemplateURLPrepopulateData::google;
using ::TemplateURLPrepopulateData::seznam;
using ::TemplateURLPrepopulateData::yahoo;
using ::TemplateURLPrepopulateData::yahoo_de;

// Sample regional set of prepopulated engines. Reconciliation will be referring
// to this one in priority
const TemplateURLPrepopulateData::PrepopulatedEngine* sample_regional_set[] = {
    &google,
    &duckduckgo,
    &yahoo_de,
};

class ReconcilingTemplateURLDataHolderTest : public testing::Test {
 public:
  ReconcilingTemplateURLDataHolderTest()
      : holder_(search_engines_test_environment_.prepopulate_data_resolver()) {
    regional_capabilities::SetPrepopulatedEnginesOverrideForTesting(
        sample_regional_set);
  }

  ~ReconcilingTemplateURLDataHolderTest() override {
    regional_capabilities::ClearPrepopulatedEnginesOverrideForTesting();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  base::HistogramTester histogram_tester_;
  ReconcilingTemplateURLDataHolder holder_;
};

TEST_F(ReconcilingTemplateURLDataHolderTest,
       GetOrComputeKeyword_Get_NonEligiblePlayKeyword) {
  auto supplied_engine = GenerateDummyTemplateURLData("searchengine.com");
  supplied_engine->regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
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
  supplied_engine->regulatory_origin = RegulatoryExtensionType::kDefault;
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
  supplied_engine->regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
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
    supplied_engine->regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
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
  EXPECT_FALSE(engine);
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       FindMatchingBuiltInDefinitionsById_ValidID_RegionalPrepopulatedEngine) {
  auto engine = holder_.FindMatchingBuiltInDefinitionsById(duckduckgo.id);
  ASSERT_TRUE(engine);
  EXPECT_EQ(duckduckgo.keyword, engine->keyword());
  EXPECT_EQ(duckduckgo.id, engine->prepopulate_id);
}

TEST_F(
    ReconcilingTemplateURLDataHolderTest,
    FindMatchingBuiltInDefinitionsById_ValidID_NonRegionalPrepopulatedEngine) {
  auto engine = holder_.FindMatchingBuiltInDefinitionsById(brave.id);
  ASSERT_TRUE(engine);
  EXPECT_EQ(brave.keyword, engine->keyword());
  EXPECT_EQ(brave.id, engine->prepopulate_id);
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       FindMatchingBuiltInDefinitionsByKeyword_UnknownKeyword) {
  auto engine = holder_.FindMatchingBuiltInDefinitionsByKeyword(u"bazzinga");
  // Expect to see no definitions.
  ASSERT_FALSE(engine);
}

TEST_F(
    ReconcilingTemplateURLDataHolderTest,
    FindMatchingBuiltInDefinitionsByKeyword_ValidKeyword_RegionalPrepopulatedEngine) {
  auto engine =
      holder_.FindMatchingBuiltInDefinitionsByKeyword(duckduckgo.keyword);
  ASSERT_TRUE(engine);
  ASSERT_EQ(duckduckgo.keyword, engine->keyword());
  ASSERT_EQ(duckduckgo.id, engine->prepopulate_id);
}

TEST_F(
    ReconcilingTemplateURLDataHolderTest,
    FindMatchingBuiltInDefinitionsByKeyword_ValidKeyword_NonRegionalPrepopulatedEngine) {
  auto engine = holder_.FindMatchingBuiltInDefinitionsByKeyword(brave.keyword);

  ASSERT_TRUE(engine);
  ASSERT_EQ(brave.keyword, engine->keyword());
  ASSERT_EQ(brave.id, engine->prepopulate_id);
}

TEST_F(ReconcilingTemplateURLDataHolderTest, Set_SafeWithEmptyPointer) {
  holder_.SetAndReconcile({});

  histogram_tester_.ExpectTotalCount("Omnibox.TemplateUrl.Reconciliation.Type",
                                     0);

  ASSERT_EQ(nullptr, holder_.Get());
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       SetAndReconcile_AndroidProvidedUnknown) {
  auto supplied_engine = GenerateDummyTemplateURLData("unknown.chromium.org");
  supplied_engine->regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
  supplied_engine->SetURL(yahoo_de.search_url);

  holder_.SetAndReconcile(std::move(supplied_engine));

  histogram_tester_.ExpectUniqueSample(
      "Omnibox.TemplateUrl.Reconciliation.Type", 2 /* kByKeyword */, 1);
  histogram_tester_.ExpectUniqueSample(
      "Omnibox.TemplateUrl.Reconciliation.ByKeyword.Result", false, 1);

  EXPECT_EQ(holder_.Get()->keyword(), u"unknown.chromium.org");
  EXPECT_EQ(holder_.Get()->prepopulate_id, 0);
}

TEST_F(ReconcilingTemplateURLDataHolderTest, SetAndReconcile_CustomYahoo) {
  auto supplied_engine = GenerateDummyTemplateURLData("yahoo.com");
  supplied_engine->regulatory_origin = RegulatoryExtensionType::kDefault;
  supplied_engine->SetURL(yahoo_de.search_url);

  holder_.SetAndReconcile(std::move(supplied_engine));

  histogram_tester_.ExpectUniqueSample(
      "Omnibox.TemplateUrl.Reconciliation.Type", 0 /* kNone */, 1);

  EXPECT_EQ(holder_.Get()->keyword(), u"yahoo.com");
  EXPECT_EQ(holder_.Get()->prepopulate_id, 0);
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       SetAndReconcile_AndroidProvidedYahoo) {
  auto supplied_engine = GenerateDummyTemplateURLData("yahoo.com");
  supplied_engine->regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
  supplied_engine->SetURL(yahoo_de.search_url);

  holder_.SetAndReconcile(std::move(supplied_engine));

  histogram_tester_.ExpectUniqueSample(
      "Omnibox.TemplateUrl.Reconciliation.Type", 3 /* ByDomainBasedKeyword */,
      1);
  histogram_tester_.ExpectUniqueSample(
      "Omnibox.TemplateUrl.Reconciliation.ByDomainBasedKeyword.Result", true,
      1);

  EXPECT_EQ(holder_.Get()->keyword(), yahoo_de.keyword);
  EXPECT_EQ(holder_.Get()->prepopulate_id, yahoo_de.id);
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       SetAndReconcile_AndoidProvidedSeznam) {
  const char* const variants[] = {"seznam.cz", "seznam.sk"};

  for (const auto* variant : variants) {
    base::HistogramTester histogram_tester;
    auto supplied_engine = GenerateDummyTemplateURLData(variant);
    supplied_engine->regulatory_origin = RegulatoryExtensionType::kAndroidEEA;

    holder_.SetAndReconcile(std::move(supplied_engine));

    histogram_tester.ExpectUniqueSample(
        "Omnibox.TemplateUrl.Reconciliation.Type", 3 /* ByDomainBasedKeyword */,
        1);
    histogram_tester.ExpectUniqueSample(
        "Omnibox.TemplateUrl.Reconciliation.ByDomainBasedKeyword.Result", true,
        1);

    EXPECT_EQ(holder_.Get()->keyword(), seznam.keyword);
    EXPECT_EQ(holder_.Get()->prepopulate_id, seznam.id);
  }
}

TEST_F(ReconcilingTemplateURLDataHolderTest, SetAndReconcile_UnknownEngine) {
  TemplateURLData supplied_engine;
  supplied_engine.prepopulate_id = ~0;

  holder_.SetAndReconcile(std::make_unique<TemplateURLData>(supplied_engine));

  histogram_tester_.ExpectUniqueSample(
      "Omnibox.TemplateUrl.Reconciliation.Type", 1 /* kByID */, 1);
  histogram_tester_.ExpectUniqueSample(
      "Omnibox.TemplateUrl.Reconciliation.ByID.Result", false, 1);

  // No reconciliation, the source data is just set.
  EXPECT_EQ(holder_.Get()->keyword(), supplied_engine.keyword());
  EXPECT_EQ(holder_.Get()->prepopulate_id, supplied_engine.prepopulate_id);
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       SetAndReconcile_RegionalPrepopulatedEngine) {
  holder_.SetAndReconcile(TemplateURLDataFromPrepopulatedEngine(yahoo));

  histogram_tester_.ExpectUniqueSample(
      "Omnibox.TemplateUrl.Reconciliation.Type", 1 /* kByID */, 1);
  histogram_tester_.ExpectUniqueSample(
      "Omnibox.TemplateUrl.Reconciliation.ByID.Result", true, 1);

  // For engines with regional variant,
  // the variant used is in priority the one in the regional set.
  EXPECT_NE(holder_.Get()->keyword(), yahoo.keyword);
  EXPECT_EQ(holder_.Get()->keyword(), yahoo_de.keyword);
  EXPECT_EQ(holder_.Get()->url(), yahoo_de.search_url);
  EXPECT_EQ(holder_.Get()->prepopulate_id, yahoo_de.id);
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       SetAndReconcile_NonRegionalPrepopulatedEngine) {
  auto engine = GenerateDummyTemplateURLData("made.up.keyword");
  engine->prepopulate_id = brave.id;

  holder_.SetAndReconcile(std::move(engine));

  histogram_tester_.ExpectUniqueSample(
      "Omnibox.TemplateUrl.Reconciliation.Type", 1 /* kByID */, 1);
  histogram_tester_.ExpectUniqueSample(
      "Omnibox.TemplateUrl.Reconciliation.ByID.Result", true, 1);

  EXPECT_EQ(holder_.Get()->keyword(), brave.keyword);
  EXPECT_EQ(holder_.Get()->prepopulate_id, brave.id);
}

TEST_F(ReconcilingTemplateURLDataHolderTest,
       SetAndReconcile_PrepopulatedEngineWithChanges) {
  auto engine = GenerateDummyTemplateURLData("made.up.keyword");
  engine->prepopulate_id = brave.id;
  engine->safe_for_autoreplace = false;

  holder_.SetAndReconcile(std::move(engine));

  histogram_tester_.ExpectUniqueSample(
      "Omnibox.TemplateUrl.Reconciliation.Type", 1 /* kByID */, 1);
  histogram_tester_.ExpectUniqueSample(
      "Omnibox.TemplateUrl.Reconciliation.ByID.Result", true, 1);

  // The user-modifiable properties (e.g. keyword) from the base data are
  // preserved, but others (e.g. search URL) use the prepopulated values.
  EXPECT_NE(holder_.Get()->keyword(), brave.keyword);
  EXPECT_NE(holder_.Get()->short_name(), brave.name);
  EXPECT_EQ(holder_.Get()->keyword(), u"made.up.keyword");
  EXPECT_EQ(holder_.Get()->prepopulate_id, brave.id);
  EXPECT_EQ(holder_.Get()->url(), brave.search_url);
}

}  // namespace
