// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/shadow_prediction_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::Bucket;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

namespace autofill::autofill_metrics {

namespace {

#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
void LogShadowPredictions(FormStructure& form, HeuristicSource active_source) {
  for (const std::unique_ptr<AutofillField>& field : form) {
    LogShadowPredictionComparison(*field, active_source);
  }
}
#endif

// These mirrors some of the values of `AutofillPredictionsComparisonResult`
// defined in tools/metrics/histograms/enums.xml. The
// `AutofillPredictionsComparisonResult` represents all the type-specific
// `ShadowPredictionComparison` results returned by `GetShadowPrediction()`.
// Only a subset of them are tested.
// This is intentionally not an enum class to implicitly convert the values to
// ints in comparisons with `GetShadowPrediction()`.
enum AutofillPredictionsComparisonResult {
  kNoPrediction = 0,
  kNameFirstSamePredictionValueAgrees = 19,
  kNameFirstSamePredictionValueDisagrees = 20,
  kNameFirstDifferentPredictionsValueAgreesWithOld = 21,
  kNameFirstDifferentPredictionsValueAgreesWithBoth = 24,
  kNameFirstDifferentPredictionsValueAgreesWithNeither = 23,
  kEmailAddressDifferentPredictionsValueAgreesWithNew = 58,
  kNameFullSamePredictionValueAgrees = 43,
  kSearchTermDifferentPredictionsValueAgreesWithNew = 586,
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  kNameFullDifferentPredictionsValueAgreesWithOld = 45,
  kEmailAddressDifferentPredictionsValueAgreesWithOld = 57,
  kSearchTermSamePredictionValueDisagrees = 584,
#endif
};

// Test that various combinations of predictions and values are mapped to the
// correct value in the metric enum.
TEST(AutofillShadowPredictionComparisonTest,
     PredictionsMapToPredictionComparison) {
  EXPECT_EQ(kNoPrediction, GetShadowPrediction(NO_SERVER_DATA, NO_SERVER_DATA,
                                               {NO_SERVER_DATA}));

  EXPECT_EQ(kNoPrediction,
            GetShadowPrediction(NAME_FIRST, NO_SERVER_DATA, {NAME_FIRST}));

  EXPECT_EQ(kNameFirstSamePredictionValueAgrees,
            GetShadowPrediction(NAME_FIRST, NAME_FIRST, {NAME_FIRST}));

  EXPECT_EQ(kNameFirstSamePredictionValueDisagrees,
            GetShadowPrediction(NAME_FIRST, NAME_FIRST, {EMAIL_ADDRESS}));

  EXPECT_EQ(kEmailAddressDifferentPredictionsValueAgreesWithNew,
            GetShadowPrediction(EMAIL_ADDRESS, NAME_FIRST, {NAME_FIRST}));

  EXPECT_EQ(kNameFirstDifferentPredictionsValueAgreesWithOld,
            GetShadowPrediction(NAME_FIRST, EMAIL_ADDRESS, {NAME_FIRST}));

  EXPECT_EQ(kNameFirstDifferentPredictionsValueAgreesWithNeither,
            GetShadowPrediction(NAME_FIRST, EMAIL_ADDRESS, {NAME_LAST}));

  EXPECT_EQ(kNameFirstDifferentPredictionsValueAgreesWithBoth,
            GetShadowPrediction(NAME_FIRST, EMAIL_ADDRESS,
                                {NAME_FIRST, EMAIL_ADDRESS}));
}

// Test that all `FieldType`s have corresponding values in the enum.
TEST(AutofillShadowPredictionComparisonTest, ComparisonContainsAllTypes) {
  // If this test fails after adding a type, update
  // `AutofillPredictionsComparisonResult` in tools/metrics/histograms/enums.xml
  // and set `last_known_type` to the last entry in the enum.
  FieldType last_known_type = MAX_VALID_FIELD_TYPE;
  for (int type_int = MAX_VALID_FIELD_TYPE - 1; type_int >= NO_SERVER_DATA;
       type_int--) {
    auto type = ToSafeFieldType(type_int, MAX_VALID_FIELD_TYPE);
    if (type != MAX_VALID_FIELD_TYPE) {
      last_known_type = type;
      break;
    }
  }

  int max_comparison =
      GetShadowPrediction(last_known_type, NAME_FIRST, {NAME_LAST});

  for (int type_int = NO_SERVER_DATA; type_int <= MAX_VALID_FIELD_TYPE;
       type_int++) {
    auto type = ToSafeFieldType(type_int, NO_SERVER_DATA);
    EXPECT_LE(GetShadowPrediction(type, NAME_FIRST, {NAME_LAST}),
              max_comparison)
        << FieldTypeToStringView(type) << " has no mapping.";
  }
}

#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
// When default is active, no shadow predictions metrics should be reported.
TEST(AutofillShadowPredictionMetricsTest, SubmissionWithoutShadowPredictions) {
  FormStructure form(FormData{});
  test_api(form).PushField().set_possible_types({NAME_FULL});
  test_api(form).PushField().set_possible_types({EMAIL_ADDRESS});
  test_api(form).SetFieldTypes(/*heuristic_types=*/{NAME_FULL, EMAIL_ADDRESS},
                               /*server_types=*/{NAME_FULL, EMAIL_ADDRESS});

  base::HistogramTester histogram_tester;
  LogShadowPredictions(form, HeuristicSource::kDefaultRegexes);

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.ShadowPredictions.ExperimentalToDefault"),
              IsEmpty());
}

// Test that Autofill.ShadowPredictions.* describes the differences between the
// predictions and the submitted values.
TEST(AutofillShadowPredictionMetricsTest,
     SubmissionWithAgreeingShadowPredictions) {
  FormStructure form(FormData{});
  test_api(form).PushField().set_possible_types({NAME_FULL});
  test_api(form).PushField().set_possible_types({EMAIL_ADDRESS});
  test_api(form)
      .SetFieldTypes(/*heuristic_types=*/
                     {{{HeuristicSource::kDefaultRegexes, NAME_FULL},
                       {HeuristicSource::kExperimentalRegexes, NAME_FULL}},
                      {{HeuristicSource::kDefaultRegexes, SEARCH_TERM},
                       {HeuristicSource::kExperimentalRegexes, EMAIL_ADDRESS}}},
                     {NAME_FULL, EMAIL_ADDRESS});

  base::HistogramTester histogram_tester;
  // Shadow predictions between default and experiment are only emitted if
  // experimental is active.
  LogShadowPredictions(form, HeuristicSource::kExperimentalRegexes);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ShadowPredictions.ExperimentalToDefault"),
      UnorderedElementsAre(
          Bucket(kNameFullSamePredictionValueAgrees, 1),
          Bucket(kSearchTermDifferentPredictionsValueAgreesWithNew, 1)));
}

// Test that Autofill.ShadowPredictions.DefaultHeuristicToDefaultServer compares
// heuristics to server predictions.
TEST(AutofillShadowPredictionMetricsTest, CompareHeuristicsAndServer) {
  FormStructure form(FormData{});
  test_api(form).PushField().set_possible_types({NAME_FULL});
  test_api(form).PushField().set_possible_types({EMAIL_ADDRESS});
  test_api(form).SetFieldTypes(/*heuristic_types=*/{NAME_FULL, SEARCH_TERM},
                               /*server_types=*/{NAME_FULL, EMAIL_ADDRESS});

  base::HistogramTester histogram_tester;
  LogShadowPredictions(form, HeuristicSource::kDefaultRegexes);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ShadowPredictions.DefaultHeuristicToDefaultServer"),
      UnorderedElementsAre(
          Bucket(kNameFullSamePredictionValueAgrees, 1),
          Bucket(kSearchTermDifferentPredictionsValueAgreesWithNew, 1)));
}
#endif

}  // namespace

}  // namespace autofill::autofill_metrics
