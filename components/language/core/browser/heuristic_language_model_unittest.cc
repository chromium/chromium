// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/heuristic_language_model.h"

#include <cmath>

#include "base/cxx17_backports.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {

using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

using LangScoreMap = std::unordered_map<std::string, float>;
using Ld = LanguageModel::LanguageDetails;

// Compares LanguageDetails.
MATCHER_P(EqualsLd, lang_details, "") {
  const float float_eps = 0.00001f;
  return arg.lang_code == lang_details.lang_code &&
         std::abs(arg.score - lang_details.score) < float_eps;
}

// HasBaseAndRegion tests.

// Empty string.
TEST(HasBaseAndRegionTest, Empty) {
  std::string base;
  EXPECT_THAT(HasBaseAndRegion("", &base), Eq(false));
  EXPECT_THAT(base, Eq(""));
}

// Two and three char base language codes.
TEST(HasBaseAndRegionTest, BaseOnly) {
  std::string base;
  EXPECT_THAT(HasBaseAndRegion("it", &base), Eq(false));
  EXPECT_THAT(base, Eq("it"));
  EXPECT_THAT(HasBaseAndRegion("haw", &base), Eq(false));
  EXPECT_THAT(base, Eq("haw"));
}

// Two and three char language codes with region tags.
TEST(HasBaseAndRegionTest, BaseAndRegion) {
  std::string base;
  EXPECT_THAT(HasBaseAndRegion("en-AU", &base), Eq(true));
  EXPECT_THAT(base, Eq("en"));
  EXPECT_THAT(HasBaseAndRegion("yue-HK", &base), Eq(true));
  EXPECT_THAT(base, Eq("yue"));
}

// Malformed input.
TEST(HasBaseAndRegionTest, Malformed) {
  std::string base;
  EXPECT_THAT(HasBaseAndRegion("-", &base), Eq(false));
  EXPECT_THAT(base, Eq(""));
  EXPECT_THAT(HasBaseAndRegion("it-", &base), Eq(false));
  EXPECT_THAT(base, Eq("it"));
  EXPECT_THAT(HasBaseAndRegion("yue--", &base), Eq(false));
  EXPECT_THAT(base, Eq("yue"));
  EXPECT_THAT(HasBaseAndRegion("es--AR", &base), Eq(true));
  EXPECT_THAT(base, Eq("es"));
}

// PromoteRegion tests.

// Basic tests without region-specific languages.
TEST(PromoteRegionsTest, NoRegions) {
  EXPECT_THAT(PromoteRegions({}), IsEmpty());
  EXPECT_THAT(PromoteRegions({"it"}), ElementsAre("it"));
  EXPECT_THAT(PromoteRegions({"en", "fr", "it"}),
              ElementsAre("en", "fr", "it"));
}

// Test a single region for a single language.
TEST(PromoteRegionsTest, SingleRegion) {
  EXPECT_THAT(PromoteRegions({"en-AU"}), ElementsAre("en-AU"));
  EXPECT_THAT(PromoteRegions({"en-AU", "en"}), ElementsAre("en-AU"));
  EXPECT_THAT(PromoteRegions({"en", "en-AU"}), ElementsAre("en-AU"));
}

// Test multiple regions for a single language.
TEST(PromoteRegionsTest, MultipleRegions) {
  EXPECT_THAT(PromoteRegions({"en-AU", "en-US"}),
              ElementsAre("en-AU", "en-US"));
  EXPECT_THAT(PromoteRegions({"en-AU", "en-US", "en"}),
              ElementsAre("en-AU", "en-US"));
  EXPECT_THAT(PromoteRegions({"en", "en-AU", "en-US"}),
              ElementsAre("en-AU", "en-US"));
}

// Test multiple regions for multiple languages.
TEST(PromoteRegionsTest, MultipleBases) {
  EXPECT_THAT(PromoteRegions({"en-AU", "fr-FR", "en-US", "fr-CA"}),
              ElementsAre("en-AU", "fr-FR", "en-US", "fr-CA"));
  EXPECT_THAT(PromoteRegions({"en-AU", "en-US", "fr", "en"}),
              ElementsAre("en-AU", "en-US", "fr"));
  EXPECT_THAT(PromoteRegions({"fr", "en", "fr-FR", "fr-CA", "en-AU", "en-US"}),
              ElementsAre("fr-FR", "fr-CA", "en-AU", "en-US"));
}

// PromoteRegionScores tests.

// Basic tests without region-specific languages.
TEST(PromoteRegionScoresTest, NoRegions) {
  EXPECT_THAT(PromoteRegionScores({}), IsEmpty());
  EXPECT_THAT(PromoteRegionScores({{"it", 1.0f}}),
              Eq(LangScoreMap({{"it", 1.0f}})));
  EXPECT_THAT(PromoteRegionScores({{"en", 1.0f}, {"fr", 0.5f}, {"it", 0.25f}}),
              Eq(LangScoreMap({{"en", 1.0f}, {"fr", 0.5f}, {"it", 0.25f}})));
}

// Test a single region for a single language.
TEST(PromoteRegionScores, SingleRegion) {
  EXPECT_THAT(PromoteRegionScores({{"en-AU", 1.0f}}),
              Eq(LangScoreMap({{"en-AU", 1.0f}})));
  EXPECT_THAT(PromoteRegionScores({{"en-AU", 1.0f}, {"en", 0.5f}}),
              Eq(LangScoreMap({{"en-AU", 1.5f}})));
}

// Test multiple regions for a single language.
TEST(PromoteRegionScores, MultipleRegions) {
  EXPECT_THAT(PromoteRegionScores({{"en-AU", 1.0f}, {"en-US", 0.5f}}),
              Eq(LangScoreMap({{"en-AU", 1.0f}, {"en-US", 0.5f}})));
  EXPECT_THAT(
      PromoteRegionScores({{"en-AU", 1.0f}, {"en-US", 0.5f}, {"en", 0.25f}}),
      Eq(LangScoreMap({{"en-AU", 1.25f}, {"en-US", 0.75f}})));
}

// Test multiple regions for multiple languages.
TEST(PromoteRegionScoresTest, MultipleBases) {
  EXPECT_THAT(PromoteRegionScores({{"en-AU", 1.0f},
                                   {"fr-FR", 0.5f},
                                   {"en-US", 0.25f},
                                   {"fr-CA", 0.125f}}),
              Eq(LangScoreMap({{"en-AU", 1.0f},
                               {"fr-FR", 0.5f},
                               {"en-US", 0.25f},
                               {"fr-CA", 0.125f}})));

  EXPECT_THAT(
      PromoteRegionScores(
          {{"en-AU", 1.0f}, {"en-US", 0.5f}, {"en", 0.25f}, {"fr", 0.125f}}),
      Eq(LangScoreMap({{"en-AU", 1.25f}, {"en-US", 0.75f}, {"fr", 0.125f}})));

  EXPECT_THAT(PromoteRegionScores({{"fr", 1.0f},
                                   {"en", 0.9f},
                                   {"fr-FR", 0.8f},
                                   {"fr-CA", 0.7f},
                                   {"en-AU", 0.6f},
                                   {"en-US", 0.5f}}),
              Eq(LangScoreMap({{"fr-FR", 1.8f},
                               {"fr-CA", 1.7f},
                               {"en-AU", 1.5f},
                               {"en-US", 1.4f}})));
}

// UpdateLanguageScores tests.
// Note that these favor "round" numbers to make the floating point calculation
// exact.

// Test that nothing is output when combining empty maps.
TEST(UpdateLanguageScoresTest, EmptyCombination) {
  LangScoreMap out;
  UpdateLanguageScores(0.1f, 1.0f, {}, &out);
  UpdateLanguageScores(0.1f, 1.0f, {}, &out);
  EXPECT_THAT(out, IsEmpty());
}

// Test standard usage.
TEST(UpdateLanguageScoresTest, NonEmptyCombination) {
  const std::vector<std::string> in1 = {"en", "it", "zh"},
                                 in2 = {"it", "en", "fr"};
  LangScoreMap out;

  UpdateLanguageScores(0.25f, 1.00f, in1, &out);
  UpdateLanguageScores(0.25f, 0.75f, in2, &out);

  EXPECT_THAT(out,
              Eq(LangScoreMap(
                  {{"en", 1.5f}, {"it", 1.5f}, {"zh", 0.5f}, {"fr", 0.25f}})));
}

// Test with high enough delta that some entries are scored 0.
TEST(UpdateLanguageScoresTest, MaxPenalty) {
  const std::vector<std::string> in1 = {"en", "it", "zh"},
                                 in2 = {"it", "en", "fr"};
  LangScoreMap out;

  UpdateLanguageScores(0.75f, 1.0f, in1, &out);
  UpdateLanguageScores(0.50f, 0.5f, in2, &out);

  EXPECT_THAT(out,
              Eq(LangScoreMap(
                  {{"en", 1.0f}, {"it", 0.75f}, {"zh", 0.0f}, {"fr", 0.0f}})));
}

// ScoresToDetails tests.

// Test empty input map.
TEST(ScoresToDetailsTest, Empty) {
  EXPECT_THAT(ScoresToDetails({}), IsEmpty());
}

// Test single entry.
TEST(ScoresToDetailsTest, Singleton) {
  EXPECT_THAT(ScoresToDetails({{"en", 1.0f}}),
              ElementsAre(EqualsLd(Ld("en", 1.0f))));
  EXPECT_THAT(ScoresToDetails({{"en", 0.5f}}),
              ElementsAre(EqualsLd(Ld("en", 1.0f))));
  EXPECT_THAT(ScoresToDetails({{"en", 0.0f}}),
              ElementsAre(EqualsLd(Ld("en", 0.0f))));
}

// Test multiple entries.
TEST(ScoresToDetailsTest, MultipleEntries) {
  EXPECT_THAT(ScoresToDetails({{"it", 0.5f}, {"zh", 1.0f}, {"en", 2.0f}}),
              ElementsAre(EqualsLd(Ld("en", 1.0f)), EqualsLd(Ld("zh", 0.5f)),
                          EqualsLd(Ld("it", 0.25f))));

  EXPECT_THAT(ScoresToDetails({{"it", 0.5f}, {"en", 0.5f}}),
              ElementsAre(EqualsLd(Ld("it", 1.0f)), EqualsLd(Ld("en", 1.0f))));

  EXPECT_THAT(ScoresToDetails({{"en", 0.0f}, {"it", 0.0f}}),
              ElementsAre(EqualsLd(Ld("it", 0.0f)), EqualsLd(Ld("en", 0.0f))));
}

// MakeHeuristicLanguageList tests.

// Using a fixed weight set, construct a test model for the given input.
std::vector<LanguageModel::LanguageDetails> TestModel(
    const std::vector<std::vector<std::string>>& input_langs) {
  const float delta = 0.01f;
  const float weights[] = {2.0f, 1.1f, 1.0f, 0.5f};
  CHECK_EQ(base::size(weights), input_langs.size());

  // Construct input to model.
  std::vector<std::pair<float, std::vector<std::string>>> input;
  for (unsigned long i = 0; i < base::size(weights); ++i)
    input.push_back({weights[i], input_langs[i]});

  // Construct model itself.
  return MakeHeuristicLanguageList(delta, input);
}

// Using a fixed weight set, construct a test model for the given input and
// return the order of languages in this model.
std::vector<std::string> TestModelOrdering(
    const std::vector<std::vector<std::string>>& input_langs) {
  std::vector<std::string> output_langs;
  for (const auto& details : TestModel(input_langs))
    output_langs.push_back(details.lang_code);
  return output_langs;
}

// Test the model ordering for a set of hand-calculated examples.
TEST(MakeHeuristicLanguageListTest, Ordering) {
  // UI / accept only.
  EXPECT_THAT(TestModelOrdering({{"en"}, {"en"}, {}, {}}), ElementsAre("en"));

  // UI overrides accept.
  EXPECT_THAT(TestModelOrdering({{"en"}, {"fr"}, {}, {}}),
              ElementsAre("en", "fr"));

  // History + accept overrides UI.
  EXPECT_THAT(TestModelOrdering({{"en"}, {"fr"}, {"fr"}, {}}),
              ElementsAre("fr", "en"));

  // Accept overrides history.
  EXPECT_THAT(TestModelOrdering({{"en"}, {"es"}, {"fr"}, {}}),
              ElementsAre("en", "es", "fr"));

  // History + ULP overrides accept.
  EXPECT_THAT(TestModelOrdering({{"en"}, {"es"}, {"fr"}, {"fr"}}),
              ElementsAre("en", "fr", "es"));

  // Accept overrides history.
  EXPECT_THAT(TestModelOrdering({{"en"}, {"es-MX"}, {"fr"}, {}}),
              ElementsAre("en", "es-MX", "fr"));

  // Correctly handle regions.
  EXPECT_THAT(TestModelOrdering({{"en"}, {"fr", "en-US"}, {}, {}}),
              ElementsAre("en-US", "fr"));

  // Region-specific overriding UI.
  EXPECT_THAT(
      TestModelOrdering({{"fr"}, {"en", "en-CA", "en-US"}, {"en"}, {"en-US"}}),
      ElementsAre("en-US", "en-CA", "fr"));
}

// Test the scores of one hand-calculated example.
TEST(MakeHeuristicLanguageListTest, Scores) {
  EXPECT_THAT(TestModel({{"fr"}, {"en", "en-CA", "en-US"}, {"en"}, {"en-US"}}),
              ElementsAre(EqualsLd(Ld("en-US", 1.0f)),
                          EqualsLd(Ld("en-CA", 2.1f / 2.59f)),
                          EqualsLd(Ld("fr", 2.0f / 2.59f))));
}

// GetHistogramLanguages tests.

// Test an empty language histogram.
TEST(GetHistogramLanguagesTest, Empty) {
  TestingPrefServiceSimple prefs;
  UrlLanguageHistogram::RegisterProfilePrefs(prefs.registry());
  UrlLanguageHistogram hist(&prefs);
  EXPECT_THAT(GetHistogramLanguages(0.0f, hist), IsEmpty());
}

// Test that the frequency cutoff is applied correctly.
TEST(GetHistogramLanguagesTest, FrequencyCutoff) {
  TestingPrefServiceSimple prefs;
  UrlLanguageHistogram::RegisterProfilePrefs(prefs.registry());
  UrlLanguageHistogram hist(&prefs);

  // Visit sites with languages in a 3 : 2 : 1 ratio.
  for (int i = 0; i < 30; ++i) {
    hist.OnPageVisited("en");
    hist.OnPageVisited("en");
    hist.OnPageVisited("en");
    hist.OnPageVisited("it");
    hist.OnPageVisited("it");
    hist.OnPageVisited("zh");
  }

  EXPECT_THAT(GetHistogramLanguages(0.0f, hist), ElementsAre("en", "it", "zh"));
  EXPECT_THAT(GetHistogramLanguages(1.0f / 3, hist), ElementsAre("en", "it"));
  EXPECT_THAT(GetHistogramLanguages(1.0f / 2, hist), ElementsAre("en"));
}

// GetUlpLanguages tests.

// Test with no ULP languages.
TEST(GetUlpLanguagesTest, Empty) {
  const auto dict =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(R"({
    "reading": {
      "confidence": 1.0,
      "preference": []
    }
  })"));

  EXPECT_THAT(GetUlpLanguages(0.0, 0.0, dict.get()), IsEmpty());
}

// Test that ULP profile of insufficient confidence is ignored.
TEST(GetUlpLanguagesTest, ConfidenceCutoff) {
  const auto dict =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(R"({
    "reading": {
      "confidence": 0.5,
      "preference": [{"language": "en", "probability": 1.0}]
    }
  })"));

  EXPECT_THAT(GetUlpLanguages(0.0, 0.0, dict.get()), ElementsAre("en"));
  EXPECT_THAT(GetUlpLanguages(0.5, 0.0, dict.get()), ElementsAre("en"));
  EXPECT_THAT(GetUlpLanguages(0.501, 0.0, dict.get()), IsEmpty());
}

// Test that ULP languages of insufficient probability are ignored.
TEST(GetUlpLanguagesTest, ProbabilityCutoff) {
  const auto dict =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(R"({
    "reading": {
      "confidence": 1.0,
      "preference": [{"language": "en", "probability": 1.00},
                     {"language": "it", "probability": 0.50},
                     {"language": "zh", "probability": 0.25}]
    }
  })"));

  EXPECT_THAT(GetUlpLanguages(0.0, 0.0, dict.get()),
              ElementsAre("en", "it", "zh"));
  EXPECT_THAT(GetUlpLanguages(0.0, 0.25, dict.get()),
              ElementsAre("en", "it", "zh"));
  EXPECT_THAT(GetUlpLanguages(0.0, 0.2501, dict.get()),
              ElementsAre("en", "it"));
  EXPECT_THAT(GetUlpLanguages(0.0, 0.5, dict.get()), ElementsAre("en", "it"));
  EXPECT_THAT(GetUlpLanguages(0.0, 0.501, dict.get()), ElementsAre("en"));
  EXPECT_THAT(GetUlpLanguages(0.0, 1.0, dict.get()), ElementsAre("en"));
}

// Test that malformed ULP data is handled gracefully.
TEST(GetUlpLanguagesTest, Malformed) {
  const auto empty =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated("{}"));
  EXPECT_THAT(GetUlpLanguages(0.0, 0.0, empty.get()), IsEmpty());

  const auto conf =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(R"({
    "reading": {
      "preference": [{"language": "en", "probability": 1.00}]
    }
  })"));
  EXPECT_THAT(GetUlpLanguages(0.0, 0.0, conf.get()), IsEmpty());

  const auto no_prefs =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(R"({
    "reading": {
      "confidence": 1.0
    }
  })"));
  EXPECT_THAT(GetUlpLanguages(0.0, 0.0, no_prefs.get()), IsEmpty());

  const auto bad_prefs =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(R"({
    "reading": {
      "confidence": 1.0,
      "preference": [{"language": "en"}, {"probability": 1.0},
                     {"language": "it", "probability": 1.0}]
    }
  })"));
  EXPECT_THAT(GetUlpLanguages(0.0, 0.0, bad_prefs.get()), ElementsAre("it"));
}

// "Integration"-style test that loads inputs into preferences then calls the
// model object. This test relies on the values of constants (e.g. weights) in
// the model definition, so must be updated every time these are tweaked.

TEST(HeuristicLanguageModelTest, PrefReading) {
  const char accept_pref[] = "intl.accept_languages";
  const char ulp_pref[] = "language_profile";

  TestingPrefServiceSimple prefs;
  PrefRegistrySimple* const registry = prefs.registry();

  // Set up accept languages.
  registry->RegisterStringPref(accept_pref, std::string());
  prefs.SetString(accept_pref, "en,en-CA,en-US");

  // Visit enough English pages to get an "en" entry in the histogram.
  UrlLanguageHistogram::RegisterProfilePrefs(registry);
  UrlLanguageHistogram hist(&prefs);
  for (int i = 0; i < 30; ++i)
    hist.OnPageVisited("en");

  // Set up ULP languages.
  registry->RegisterDictionaryPref(ulp_pref);
  const auto ulp_value =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(R"({
    "reading": {
      "confidence": 1.0,
      "preference": [{"language": "en-US", "probability": 1.0}]
    }
  })"));
  prefs.Set(ulp_pref, *ulp_value);

  // Construct and query model.
  HeuristicLanguageModel model(&prefs, "fr", accept_pref, ulp_pref);
  EXPECT_THAT(model.GetLanguages(),
              ElementsAre(EqualsLd(Ld("en-US", 1.0f)),
                          EqualsLd(Ld("en-CA", 2.1f / 2.59f)),
                          EqualsLd(Ld("fr", 2.0f / 2.59f))));
}

}  // namespace language
