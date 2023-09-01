// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/shadow_prediction_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::autofill::mojom::SubmissionSource;
using ::base::Bucket;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

namespace autofill::autofill_metrics {

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

namespace {

// Get a form with 2 fields.
FormData GetFormWith2Fields(const GURL& form_origin) {
  return test::GetFormData(
      {.description_for_logging = "ShadowPredictions",
       .fields =
           {
               {
                   .label = u"Name",
                   .name = u"name",
               },
               {
                   .label = u"Email",
                   .name = u"email",
               },
           },
       .unique_renderer_id = test::MakeFormRendererId(),
       .main_frame_origin = url::Origin::Create(form_origin)});
}

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

// Test that all `ServerFieldType`s have corresponding values in the enum.
TEST(AutofillShadowPredictionComparisonTest, ComparisonContainsAllTypes) {
  // If this test fails after adding a type, update
  // `AutofillPredictionsComparisonResult` in tools/metrics/histograms/enums.xml
  // and set `last_known_type` to the last entry in the enum.
  ServerFieldType last_known_type = MAX_VALID_FIELD_TYPE;
  for (int type_int = MAX_VALID_FIELD_TYPE - 1; type_int >= NO_SERVER_DATA;
       type_int--) {
    auto type = ToSafeServerFieldType(type_int, MAX_VALID_FIELD_TYPE);
    if (type != MAX_VALID_FIELD_TYPE) {
      last_known_type = type;
      break;
    }
  }

  int max_comparison =
      GetShadowPrediction(last_known_type, NAME_FIRST, {NAME_LAST});

  for (int type_int = NO_SERVER_DATA; type_int <= MAX_VALID_FIELD_TYPE;
       type_int++) {
    auto type = ToSafeServerFieldType(type_int, NO_SERVER_DATA);
    EXPECT_LE(GetShadowPrediction(type, NAME_FIRST, {NAME_LAST}),
              max_comparison)
        << FieldTypeToStringPiece(type) << " has no mapping.";
  }
}

class AutofillShadowPredictionMetricsTest : public AutofillMetricsBaseTest,
                                            public testing::Test {
 public:
  AutofillShadowPredictionMetricsTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(
            features::kAutofillParsingPatternProvider,
            {{"prediction_source", "default"}})},
        {});
  }

  ~AutofillShadowPredictionMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// When shadow predictions are not calculated, the shadow prediction metrics
// should report `0`.
TEST_F(AutofillShadowPredictionMetricsTest,
       SubmissionWithoutShadowPredictions) {
  FormData form = GetFormWith2Fields(autofill_client_->form_origin());
  form.fields[0].value = u"Elvis Aaron Presley";  // A known `NAME_FULL`.
  form.fields[1].value = u"buddy@gmail.com";      // A known `EMAIL_ADDRESS`.

  std::vector<ServerFieldType> heuristic_types = {NAME_FULL, EMAIL_ADDRESS};
  std::vector<ServerFieldType> server_types = {NAME_FULL, EMAIL_ADDRESS};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  histogram_tester.ExpectBucketCount(
      "Autofill.ShadowPredictions.ExperimentalToDefault", kNoPrediction, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.ShadowPredictions.NextGenToDefault", kNoPrediction, 2);
#else
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.ShadowPredictions.ExperimentalToDefault"),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.ShadowPredictions.NextGenToDefault"),
              IsEmpty());
#endif
}

#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
// Test that Autofill.ShadowPredictions.* describes the differences between the
// predictions and the submitted values.
TEST_F(AutofillShadowPredictionMetricsTest,
       SubmissionWithAgreeingShadowPredictions) {
  FormData form = GetFormWith2Fields(autofill_client_->form_origin());
  form.fields[0].value = u"Elvis Aaron Presley";  // A known `NAME_FULL`.
  form.fields[1].value = u"buddy@gmail.com";      // A known `EMAIL_ADDRESS`.

  std::vector<ServerFieldType> server_types = {NAME_FULL, EMAIL_ADDRESS};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(
      form,
      {// Field 0
       {{HeuristicSource::kDefault, NAME_FULL},
        {HeuristicSource::kExperimental, NAME_FULL},
        {HeuristicSource::kNextGen, NAME_FIRST}},
       // Field 1
       {{HeuristicSource::kDefault, SEARCH_TERM},
        {HeuristicSource::kExperimental, EMAIL_ADDRESS},
        {HeuristicSource::kNextGen, SEARCH_TERM}}},
      server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ShadowPredictions.ExperimentalToDefault"),
      UnorderedElementsAre(
          Bucket(kNameFullSamePredictionValueAgrees, 1),
          Bucket(kSearchTermDifferentPredictionsValueAgreesWithNew, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.ShadowPredictions.NextGenToDefault"),
              UnorderedElementsAre(
                  Bucket(kNameFullDifferentPredictionsValueAgreesWithOld, 1),
                  Bucket(kSearchTermSamePredictionValueDisagrees, 1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ShadowPredictions.NextGenToExperimental"),
      UnorderedElementsAre(
          Bucket(kNameFullDifferentPredictionsValueAgreesWithOld, 1),
          Bucket(kEmailAddressDifferentPredictionsValueAgreesWithOld, 1)));
}
#endif

// Test that Autofill.ShadowPredictions.DefaultHeuristicToDefaultServer compares
// heuristics to server predictions.
TEST_F(AutofillShadowPredictionMetricsTest, CompareHeuristicsAndServer) {
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  constexpr HeuristicSource source = HeuristicSource::kDefault;
#else
  constexpr HeuristicSource source = HeuristicSource::kLegacy;
#endif

  FormData form = GetFormWith2Fields(autofill_client_->form_origin());
  form.fields[0].value = u"Elvis Aaron Presley";  // A known `NAME_FULL`.
  form.fields[1].value = u"buddy@gmail.com";      // A known `EMAIL_ADDRESS`.

  std::vector<ServerFieldType> server_types = {NAME_FULL, EMAIL_ADDRESS};

  // Simulate having seen this form on page load.
  autofill_manager().AddSeenForm(form,
                                 {// Field 0
                                  {{source, NAME_FULL}},
                                  // Field 1
                                  {{source, SEARCH_TERM}}},
                                 server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormSubmitted(form, /*known_success=*/false,
                                     SubmissionSource::FORM_SUBMISSION);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ShadowPredictions.DefaultHeuristicToDefaultServer"),
      UnorderedElementsAre(
          Bucket(kNameFullSamePredictionValueAgrees, 1),
          Bucket(kSearchTermDifferentPredictionsValueAgreesWithNew, 1)));
}

}  // namespace

}  // namespace autofill::autofill_metrics
