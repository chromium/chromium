// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_split_metrics.h"

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace search_engines {

namespace {

std::unique_ptr<TemplateURL> CreatePrepopulatedEngine(
    const TemplateURLPrepopulateData::PrepopulatedEngine& engine) {
  auto data = TemplateURLDataFromPrepopulatedEngine(engine);
  return std::make_unique<TemplateURL>(*data);
}

std::unique_ptr<TemplateURL> CreateRegulatoryEngine(
    const TemplateURLPrepopulateData::PrepopulatedEngine& engine) {
  auto data = TemplateURLDataFromPrepopulatedEngine(engine);
  data->regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
  return std::make_unique<TemplateURL>(*data);
}

std::unique_ptr<TemplateURL> CreateCustomEngine(const std::string& url,
                                                int prepopulate_id = 0) {
  TemplateURLData data;
  data.SetShortName(u"Test Engine");
  data.SetKeyword(u"test");
  data.SetURL(url);
  data.prepopulate_id = prepopulate_id;
  data.safe_for_autoreplace = false;
  return std::make_unique<TemplateURL>(data);
}

}  // namespace

class SearchEngineSplitMetricsTest : public testing::Test {
 protected:
  SearchTermsData search_terms_data_;
  metrics::ProfileMetricsService profile_metrics_service_{/*context=*/1};
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_{
      {switches::kPrepopulatedEnginesShadowVariants,
       switches::kApplySearchEngineTypeMigration}};
};

TEST_F(SearchEngineSplitMetricsTest, InspectYahooJapanEngineType) {
  std::unique_ptr<TemplateURL> legacy_engine =
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::yahoo_jp);
  EXPECT_EQ(InspectYahooJapanEngineType(*legacy_engine, search_terms_data_),
            OseSplitType::kLegacy);

  std::unique_ptr<TemplateURL> new_engine =
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::yahoo_jp_next);
  EXPECT_EQ(InspectYahooJapanEngineType(*new_engine, search_terms_data_),
            OseSplitType::kNew);

  std::unique_ptr<TemplateURL> custom_yahoo =
      CreateCustomEngine("https://search.yahoo.co.jp/search?p={searchTerms}");
  EXPECT_EQ(InspectYahooJapanEngineType(*custom_yahoo, search_terms_data_),
            OseSplitType::kCustom);

  std::unique_ptr<TemplateURL> unknown_id_yahoo =
      CreateCustomEngine("https://search.yahoo.co.jp/search?p={searchTerms}",
                         /*prepopulate_id=*/999);
  EXPECT_EQ(InspectYahooJapanEngineType(*unknown_id_yahoo, search_terms_data_),
            OseSplitType::kUnknown);

  std::unique_ptr<TemplateURL> google_engine =
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::google);
  EXPECT_EQ(InspectYahooJapanEngineType(*google_engine, search_terms_data_),
            std::nullopt);

  std::unique_ptr<TemplateURL> non_jp_yahoo =
      CreateCustomEngine("https://search.yahoo.com/search?p={searchTerms}");
  EXPECT_EQ(InspectYahooJapanEngineType(*non_jp_yahoo, search_terms_data_),
            std::nullopt);
}

TEST_F(SearchEngineSplitMetricsTest, InspectYahooJapanEngineState_Legacy) {
  std::unique_ptr<TemplateURL> legacy_engine =
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::yahoo_jp);

  // Legacy DSE, default config
  EXPECT_EQ(InspectYahooJapanEngineState(*legacy_engine, legacy_engine.get(),
                                         search_terms_data_),
            OseSplitEngineState::kLegacyDse);

  // Legacy not DSE, default config
  std::unique_ptr<TemplateURL> google_engine =
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::google);
  EXPECT_EQ(InspectYahooJapanEngineState(*legacy_engine, google_engine.get(),
                                         search_terms_data_),
            OseSplitEngineState::kLegacyNotDse);

  // Legacy DSE, user customized
  legacy_engine->set_safe_for_autoreplace(false);
  EXPECT_EQ(InspectYahooJapanEngineState(*legacy_engine, legacy_engine.get(),
                                         search_terms_data_),
            OseSplitEngineState::kLegacyDseCustomized);

  // Legacy not DSE, user customized
  EXPECT_EQ(InspectYahooJapanEngineState(*legacy_engine, google_engine.get(),
                                         search_terms_data_),
            OseSplitEngineState::kLegacyNotDseCustomized);
}

TEST_F(SearchEngineSplitMetricsTest,
       InspectYahooJapanEngineState_Legacy_DeviceChoice) {
  std::unique_ptr<TemplateURL> legacy_device_choice =
      CreateRegulatoryEngine(TemplateURLPrepopulateData::yahoo_jp);

  // Legacy DeviceChoice DSE
  EXPECT_EQ(InspectYahooJapanEngineState(*legacy_device_choice,
                                         legacy_device_choice.get(),
                                         search_terms_data_),
            OseSplitEngineState::kLegacyDseDeviceChoice);

  // Legacy DeviceChoice not DSE
  std::unique_ptr<TemplateURL> google_engine =
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::google);
  EXPECT_EQ(InspectYahooJapanEngineState(
                *legacy_device_choice, google_engine.get(), search_terms_data_),
            OseSplitEngineState::kLegacyNotDseDeviceChoice);
}

TEST_F(SearchEngineSplitMetricsTest, InspectYahooJapanEngineState_New) {
  std::unique_ptr<TemplateURL> new_engine =
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::yahoo_jp_next);

  // New DSE, default config
  EXPECT_EQ(InspectYahooJapanEngineState(*new_engine, new_engine.get(),
                                         search_terms_data_),
            OseSplitEngineState::kNewDse);

  // New not DSE, default config
  std::unique_ptr<TemplateURL> google_engine =
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::google);
  EXPECT_EQ(InspectYahooJapanEngineState(*new_engine, google_engine.get(),
                                         search_terms_data_),
            OseSplitEngineState::kNewNotDse);

  // New DSE, user customized
  new_engine->set_safe_for_autoreplace(false);
  EXPECT_EQ(InspectYahooJapanEngineState(*new_engine, new_engine.get(),
                                         search_terms_data_),
            OseSplitEngineState::kNewDseCustomized);

  // New not DSE, user customized
  EXPECT_EQ(InspectYahooJapanEngineState(*new_engine, google_engine.get(),
                                         search_terms_data_),
            OseSplitEngineState::kNewNotDseCustomized);
}

TEST_F(SearchEngineSplitMetricsTest,
       InspectYahooJapanEngineState_New_DeviceChoice) {
  std::unique_ptr<TemplateURL> new_device_choice =
      CreateRegulatoryEngine(TemplateURLPrepopulateData::yahoo_jp_next);

  // New DeviceChoice DSE
  EXPECT_EQ(
      InspectYahooJapanEngineState(*new_device_choice, new_device_choice.get(),
                                   search_terms_data_),
      OseSplitEngineState::kNewDseDeviceChoice);

  // New DeviceChoice not DSE
  std::unique_ptr<TemplateURL> google_engine =
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::google);
  EXPECT_EQ(InspectYahooJapanEngineState(
                *new_device_choice, google_engine.get(), search_terms_data_),
            OseSplitEngineState::kNewNotDseDeviceChoice);
}

TEST_F(SearchEngineSplitMetricsTest, InspectYahooJapanEngineState_Custom) {
  std::unique_ptr<TemplateURL> custom_yahoo =
      CreateCustomEngine("https://search.yahoo.co.jp/search?p={searchTerms}");

  // Custom DSE
  EXPECT_EQ(InspectYahooJapanEngineState(*custom_yahoo, custom_yahoo.get(),
                                         search_terms_data_),
            OseSplitEngineState::kCustomDse);

  // Custom not DSE
  std::unique_ptr<TemplateURL> google_engine =
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::google);
  EXPECT_EQ(InspectYahooJapanEngineState(*custom_yahoo, google_engine.get(),
                                         search_terms_data_),
            OseSplitEngineState::kCustomNotDse);
}

TEST_F(SearchEngineSplitMetricsTest,
       RecordSearchEngineSplitProfileLoadMetrics_NoYahoo) {
  std::vector<std::unique_ptr<TemplateURL>> engines;
  engines.push_back(
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::google));
  engines.push_back(CreatePrepopulatedEngine(TemplateURLPrepopulateData::bing));

  RecordSearchEngineSplitProfileLoadMetrics(
      engines, engines[0].get(), search_terms_data_, profile_metrics_service_);

  histogram_tester_.ExpectUniqueSample(
      "Search.OseSplitYahooJapan.CountOnProfileLoad", 0, 1);
  histogram_tester_.ExpectTotalCount(
      "Search.OseSplitYahooJapan.DseTypeOnProfileLoad", 0);
  histogram_tester_.ExpectTotalCount(
      "Search.OseSplitYahooJapan.EngineStateOnProfileLoad", 0);
}

TEST_F(SearchEngineSplitMetricsTest,
       RecordSearchEngineSplitProfileLoadMetrics_SingleNewDse) {
  std::vector<std::unique_ptr<TemplateURL>> engines;
  engines.push_back(
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::yahoo_jp_next));
  engines.push_back(
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::google));

  RecordSearchEngineSplitProfileLoadMetrics(
      engines, engines[0].get(), search_terms_data_, profile_metrics_service_);

  histogram_tester_.ExpectUniqueSample(
      "Search.OseSplitYahooJapan.CountOnProfileLoad", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Search.OseSplitYahooJapan.DseTypeOnProfileLoad", OseSplitType::kNew, 1);
  histogram_tester_.ExpectUniqueSample(
      "Search.OseSplitYahooJapan.EngineStateOnProfileLoad",
      OseSplitEngineState::kNewDse, 1);
}

TEST_F(SearchEngineSplitMetricsTest,
       RecordSearchEngineSplitProfileLoadMetrics_DualYahooLegacyDse) {
  std::vector<std::unique_ptr<TemplateURL>> engines;
  engines.push_back(
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::yahoo_jp));
  engines.push_back(
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::yahoo_jp_next));
  engines.push_back(
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::google));

  RecordSearchEngineSplitProfileLoadMetrics(
      engines, engines[0].get(), search_terms_data_, profile_metrics_service_);

  histogram_tester_.ExpectUniqueSample(
      "Search.OseSplitYahooJapan.CountOnProfileLoad", 2, 1);
  histogram_tester_.ExpectUniqueSample(
      "Search.OseSplitYahooJapan.DseTypeOnProfileLoad", OseSplitType::kLegacy,
      1);
  histogram_tester_.ExpectBucketCount(
      "Search.OseSplitYahooJapan.EngineStateOnProfileLoad",
      OseSplitEngineState::kLegacyDse, 1);
  histogram_tester_.ExpectBucketCount(
      "Search.OseSplitYahooJapan.EngineStateOnProfileLoad",
      OseSplitEngineState::kNewNotDse, 1);
  histogram_tester_.ExpectTotalCount(
      "Search.OseSplitYahooJapan.EngineStateOnProfileLoad", 2);
}

TEST_F(SearchEngineSplitMetricsTest,
       RecordSearchEngineSplitProfileLoadMetrics_DualYahooNonYahooDse) {
  std::vector<std::unique_ptr<TemplateURL>> engines;
  engines.push_back(
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::google));
  engines.push_back(
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::yahoo_jp));
  engines.push_back(
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::yahoo_jp_next));

  RecordSearchEngineSplitProfileLoadMetrics(
      engines, engines[0].get(), search_terms_data_, profile_metrics_service_);

  histogram_tester_.ExpectUniqueSample(
      "Search.OseSplitYahooJapan.CountOnProfileLoad", 2, 1);
  histogram_tester_.ExpectTotalCount(
      "Search.OseSplitYahooJapan.DseTypeOnProfileLoad", 0);
  histogram_tester_.ExpectBucketCount(
      "Search.OseSplitYahooJapan.EngineStateOnProfileLoad",
      OseSplitEngineState::kLegacyNotDse, 1);
  histogram_tester_.ExpectBucketCount(
      "Search.OseSplitYahooJapan.EngineStateOnProfileLoad",
      OseSplitEngineState::kNewNotDse, 1);
  histogram_tester_.ExpectTotalCount(
      "Search.OseSplitYahooJapan.EngineStateOnProfileLoad", 2);
}
TEST_F(SearchEngineSplitMetricsTest,
       RecordSearchEngineSplitSettingsPageLoadMetrics) {
  TemplateURL::TemplateURLVector raw_engines;
  std::unique_ptr<TemplateURL> legacy =
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::yahoo_jp);
  std::unique_ptr<TemplateURL> next =
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::yahoo_jp_next);
  std::unique_ptr<TemplateURL> google =
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::google);

  raw_engines.push_back(legacy.get());
  raw_engines.push_back(next.get());
  raw_engines.push_back(google.get());

  RecordSearchEngineSplitSettingsPageLoadMetrics(
      raw_engines, legacy.get(), search_terms_data_, profile_metrics_service_);

  histogram_tester_.ExpectUniqueSample(
      "Search.OseSplitYahooJapan.DseTypeOnSettingsPageLoad",
      OseSplitType::kLegacy, 1);
  histogram_tester_.ExpectUniqueSample(
      "Search.OseSplitYahooJapan.DseTypeOnSettingsPageLoad.Profile1",
      OseSplitType::kLegacy, 1);
  histogram_tester_.ExpectUniqueSample(
      "Search.OseSplitYahooJapan.CountOnSettingsPageLoad", 2, 1);
  histogram_tester_.ExpectUniqueSample(
      "Search.OseSplitYahooJapan.CountOnSettingsPageLoad.Profile1", 2, 1);
  histogram_tester_.ExpectBucketCount(
      "Search.OseSplitYahooJapan.EngineStateOnSettingsPageLoad",
      OseSplitEngineState::kLegacyDse, 1);
  histogram_tester_.ExpectBucketCount(
      "Search.OseSplitYahooJapan.EngineStateOnSettingsPageLoad.Profile1",
      OseSplitEngineState::kLegacyDse, 1);
  histogram_tester_.ExpectBucketCount(
      "Search.OseSplitYahooJapan.EngineStateOnSettingsPageLoad",
      OseSplitEngineState::kNewNotDse, 1);
  histogram_tester_.ExpectBucketCount(
      "Search.OseSplitYahooJapan.EngineStateOnSettingsPageLoad.Profile1",
      OseSplitEngineState::kNewNotDse, 1);
  histogram_tester_.ExpectTotalCount(
      "Search.OseSplitYahooJapan.EngineStateOnSettingsPageLoad", 2);
  histogram_tester_.ExpectTotalCount(
      "Search.OseSplitYahooJapan.EngineStateOnSettingsPageLoad.Profile1", 2);
}

}  // namespace search_engines
