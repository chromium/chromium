// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/prediction_quality_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

// This is defined in the prediction_quality_metrics.cc implementation file.
int GetFieldTypeGroupPredictionQualityMetric(FieldType field_type,
                                             FieldTypeQualityMetric metric);

namespace {

using ::autofill::test::CreateTestFormField;
using ::base::Bucket;
using ::base::BucketsAre;
using ::testing::WithParamInterface;

constexpr FieldTypeSet kMLSupportedTypesForTesting = {
    UNKNOWN_TYPE,       NAME_FIRST,
    NAME_LAST,          NAME_FULL,
    EMAIL_ADDRESS,      PHONE_HOME_NUMBER,
    ADDRESS_HOME_LINE1, ADDRESS_HOME_STREET_ADDRESS,
    ADDRESS_HOME_CITY};

class PredictionQualityMetricsTest : public AutofillMetricsBaseTest,
                                     public testing::Test {
 public:
  using AutofillMetricsBaseTest::AutofillMetricsBaseTest;
  ~PredictionQualityMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }
  void TearDown() override { TearDownHelper(); }
};

// Test that we behave sanely when the cached form differs from the submitted
// one.
TEST_F(PredictionQualityMetricsTest, SaneMetricsWithCacheMismatch) {
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams model_predictions_feature_params;
  model_predictions_feature_params["model_active"] = "false";
  scoped_feature_list.InitWithFeaturesAndParameters(
      {
          // Enable the model, but not as the active heuristic source.
          {features::kAutofillModelPredictions,
           {model_predictions_feature_params}},
      },
      {});

  FormData form = CreateForm(
      {CreateTestFormField("Both match", "match", "Elvis Aaron Presley",
                           FormControlType::kInputText),
       CreateTestFormField("Both mismatch", "mismatch", "buddy@gmail.com",
                           FormControlType::kInputText),
       CreateTestFormField("Only heuristics match", "mixed", "Memphis",
                           FormControlType::kInputText),
       CreateTestFormField("Unknown", "unknown", "garbage",
                           FormControlType::kInputText)});
  test_api(form).field(0).set_is_autofilled(true);

  std::vector<FieldType> heuristic_types = {NAME_FULL, PHONE_HOME_NUMBER,
                                            ADDRESS_HOME_CITY, UNKNOWN_TYPE};
  std::vector<FieldType> server_types = {NAME_FULL, PHONE_HOME_NUMBER,
                                         PHONE_HOME_NUMBER, UNKNOWN_TYPE};
  std::vector<FieldType> ml_types = server_types;

  std::unique_ptr<FormStructure> form_structure =
      std::make_unique<FormStructure>(test::WithoutValues(form));

  for (auto [field, heuristic_type, server_type, ml_type] : base::zip(
           form_structure->fields(), heuristic_types, server_types, ml_types)) {
    field->set_heuristic_type(GetActiveHeuristicSource(), heuristic_type);
    field->set_server_predictions({test::CreateFieldPrediction(server_type)});
    // ML predictions can be overridden when regexes predict a type that the ML
    // model does not know - we need to set these so that the ML predction is
    // used.
    field->set_ml_supported_types(kMLSupportedTypesForTesting);
    field->set_heuristic_type(HeuristicSource::kAutofillMachineLearning,
                              ml_type);
  }
  test_api(autofill_manager()).AddSeenFormStructure(std::move(form_structure));

  // Add a field and re-arrange the remaining form fields before submitting. The
  // five submitted fields are filled with
  // - EMPTY_TYPE (Tennessee) - While this is an ADDRESS_HOME_STATE in theory,
  //     this field is added at runtime. As the value "Tennessee" is seen
  //     for the first time when the form is submitted, the field's initial
  //     value equates the current value. Therefore, the field is considered as
  //     not-typed and therefore empty. Also no ML heuristics are executed on
  //     the field because it just appears at form submission time.
  // - ADDRESS_HOME_CITY (Memphis)
  // - EMAIL_ADDRESS (buddy@gmail.com)
  // - garbage
  // - NAME_FULL (Elvis Aaron Presley)
  std::vector<FormFieldData> cached_fields = form.fields();
  form.set_fields({CreateTestFormField("New field", "new field", "Tennessee",
                                       FormControlType::kInputText),
                   cached_fields[2], cached_fields[1], cached_fields[3],
                   cached_fields[0]});
  std::vector<FieldType> actual_types = {NAME_FULL, EMAIL_ADDRESS,
                                         ADDRESS_HOME_CITY, UNKNOWN_TYPE};

  base::HistogramTester histogram_tester;
  SubmitForm(form);

  std::vector<std::string> sources = {"Heuristic", "Server", "Overall"};
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Quality metrics for ".ML" are only recorded if the ML predictions are
  // computed but not the active heuristic source.
  if (base::FeatureList::IsEnabled(features::kAutofillModelPredictions) &&
      GetActiveHeuristicSource() != HeuristicSource::kAutofillMachineLearning) {
    sources.push_back("ML");
  }
#endif

  for (const std::string& source : sources) {
    SCOPED_TRACE(testing::Message() << source);
    using enum FieldTypeQualityMetric;

    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FieldPredictionQuality.Aggregate." + source),
        BucketsAre(
            Bucket(TRUE_NEGATIVE_UNKNOWN, 1), Bucket(TRUE_NEGATIVE_EMPTY, 1),
            Bucket(TRUE_POSITIVE, source == "Heuristic" ? 2 : 1),
            Bucket(FALSE_NEGATIVE_MISMATCH, source == "Heuristic" ? 1 : 2)));

    auto b = [](FieldType type, FieldTypeQualityMetric metric,
                size_t count = 1) {
      return Bucket(GetFieldTypeGroupPredictionQualityMetric(type, metric),
                    count);
    };
    EXPECT_THAT(histogram_tester.GetAllSamples(
                    "Autofill.FieldPredictionQuality.ByFieldType." + source),
                BucketsAre(b(ADDRESS_HOME_CITY, source == "Heuristic"
                                                    ? TRUE_POSITIVE
                                                    : FALSE_NEGATIVE_MISMATCH),
                           b(PHONE_HOME_NUMBER, FALSE_POSITIVE_MISMATCH,
                             source != "Heuristic" ? 2 : 1),
                           b(EMAIL_ADDRESS, FALSE_NEGATIVE_MISMATCH),
                           b(NAME_FULL, TRUE_POSITIVE)));

    std::vector<FieldType>& predicted_types = [&]() -> std::vector<FieldType>& {
      if (source == "Heuristic") {
        return heuristic_types;
      } else if (source == "Server") {
        return server_types;
      } else if (source == "Overall") {
        return server_types;
      } else if (source == "ML") {
        return ml_types;
      }
      NOTREACHED();
    }();
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Autofill.FieldPrediction." + source),
        BucketsAre(source == "Server"
                       ? Bucket((NO_SERVER_DATA << 16) | EMPTY_TYPE, 1)
                       : Bucket((UNKNOWN_TYPE << 16) | EMPTY_TYPE, 1),
                   Bucket((predicted_types[0] << 16) | actual_types[0], 1),
                   Bucket((predicted_types[1] << 16) | actual_types[1], 1),
                   Bucket((predicted_types[2] << 16) | actual_types[2], 1),
                   Bucket((predicted_types[3] << 16) | actual_types[3], 1)));
  }
}

TEST_F(PredictionQualityMetricsTest,
       LogHeuristicPredictionQualityMetrics_PerLabelSource) {
  constexpr std::string_view kMetricName =
      "Autofill.FieldPredictionQuality.Aggregate.Heuristic.PTag";

  AutofillField field;
  field.set_label_source(FormFieldData::LabelSource::kPTag);
  field.set_heuristic_type(GetActiveHeuristicSource(), NAME_FULL);

  // Actual type matches.
  {
    field.set_possible_types({NAME_FULL});
    base::HistogramTester histogram_tester;
    LogHeuristicPredictionQualityPerLabelSourceMetric(field);
    histogram_tester.ExpectUniqueSample(kMetricName, true, 1);
  }
  // Actual type doesn't match.
  {
    field.set_possible_types({NAME_FIRST});
    base::HistogramTester histogram_tester;
    LogHeuristicPredictionQualityPerLabelSourceMetric(field);
    histogram_tester.ExpectUniqueSample(kMetricName, false, 1);
  }
  // Unknown type -> metric not logged.
  {
    field.set_possible_types({UNKNOWN_TYPE});
    base::HistogramTester histogram_tester;
    LogHeuristicPredictionQualityPerLabelSourceMetric(field);
    histogram_tester.ExpectTotalCount(kMetricName, 0);
  }
}

TEST_F(PredictionQualityMetricsTest, LogLocalHeuristicMatchedAttribute) {
  base::HistogramTester histogram_tester;
  LogLocalHeuristicMatchedAttribute({});  // None
  LogLocalHeuristicMatchedAttribute(
      {MatchAttribute::kLabel, MatchAttribute::kName});  // Ambiguous
  LogLocalHeuristicMatchedAttribute({MatchAttribute::kLabel});
  LogLocalHeuristicMatchedAttribute({MatchAttribute::kName});
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.LocalHeuristics.MatchedAttribute"),
              BucketsAre(Bucket(0 /* None */, 1), Bucket(1 /* Ambiguous */, 1),
                         Bucket(2 /* Label */, 1), Bucket(3 /* Name */, 1)));
}

struct PredictionOverlapMetricTestInput {
  FieldType server_predictions;
  FieldType heuristics_predictions;
  HtmlFieldType autocomplete_predictions;
  FieldPredictionOverlapSourcesSuperset expected_overlap;
};

class PredictionOverlapMetricTest
    : public PredictionQualityMetricsTest,
      public WithParamInterface<PredictionOverlapMetricTestInput> {};

TEST_P(PredictionOverlapMetricTest, LogFieldPredictionOverlapMetrics) {
  PredictionOverlapMetricTestInput input = GetParam();

  AutofillField field;
  field.set_possible_types({NAME_FIRST});
  field.set_server_predictions(
      {test::CreateFieldPrediction(input.server_predictions)});
  field.set_heuristic_type(GetActiveHeuristicSource(),
                           input.heuristics_predictions);
  field.SetHtmlType(input.autocomplete_predictions, HtmlFieldMode::kNone);

  base::HistogramTester histogram_tester;
  LogFieldPredictionOverlapMetrics(field);

  histogram_tester.ExpectUniqueSample(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributePresent.Overall."
      "AllTypes",
      input.expected_overlap, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributePresent.Overall."
      "NAME_FIRST",
      input.expected_overlap, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAggregate.Overall."
      "AllTypes",
      input.expected_overlap, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAggregate.Overall."
      "NAME_FIRST",
      input.expected_overlap, 1);
}

INSTANTIATE_TEST_SUITE_P(
    OverlapBucketsTest,
    PredictionOverlapMetricTest,
    testing::Values(
        PredictionOverlapMetricTestInput{
            NAME_FIRST, NAME_FIRST, HtmlFieldType::kGivenName,
            FieldPredictionOverlapSourcesSuperset::
                kServerHeuristicsAutocompleteCorrect},
        PredictionOverlapMetricTestInput{
            NAME_FIRST, NAME_FIRST, HtmlFieldType::kCountryName,
            FieldPredictionOverlapSourcesSuperset::kServerHeuristicsCorrect},
        PredictionOverlapMetricTestInput{
            NAME_FIRST, ADDRESS_HOME_STATE, HtmlFieldType::kGivenName,
            FieldPredictionOverlapSourcesSuperset::kServerAutocompleteCorrect},
        PredictionOverlapMetricTestInput{ADDRESS_HOME_STATE, NAME_FIRST,
                                         HtmlFieldType::kGivenName,
                                         FieldPredictionOverlapSourcesSuperset::
                                             kHeuristicsAutocompleteCorrect},
        PredictionOverlapMetricTestInput{
            ADDRESS_HOME_STATE, ADDRESS_HOME_STATE, HtmlFieldType::kGivenName,
            FieldPredictionOverlapSourcesSuperset::kAutocompleteCorrect},
        PredictionOverlapMetricTestInput{
            ADDRESS_HOME_STATE, NAME_FIRST, HtmlFieldType::kCountryName,
            FieldPredictionOverlapSourcesSuperset::kHeuristicsCorrect},
        PredictionOverlapMetricTestInput{
            NAME_FIRST, ADDRESS_HOME_STATE, HtmlFieldType::kCountryName,
            FieldPredictionOverlapSourcesSuperset::kServerCorrect},
        PredictionOverlapMetricTestInput{
            ADDRESS_HOME_STATE, ADDRESS_HOME_STATE, HtmlFieldType::kCountryName,
            FieldPredictionOverlapSourcesSuperset::kNoneCorrect}));

TEST_F(PredictionQualityMetricsTest,
       LogFieldPredictionOverlapMetrics_IllegalValuesIgnored) {
  base::HistogramTester histogram_tester;

  AutofillField field_1;
  field_1.set_possible_types({NAME_FIRST, ADDRESS_HOME_STATE});
  field_1.set_server_predictions(
      {test::CreateFieldPrediction(ADDRESS_HOME_STATE)});
  field_1.set_heuristic_type(GetActiveHeuristicSource(), ADDRESS_HOME_STATE);
  LogFieldPredictionOverlapMetrics(field_1);

  AutofillField field_2;
  field_2.set_possible_types({EMPTY_TYPE});
  field_2.set_server_predictions(
      {test::CreateFieldPrediction(ADDRESS_HOME_STATE)});
  field_2.set_heuristic_type(GetActiveHeuristicSource(), ADDRESS_HOME_STATE);
  LogFieldPredictionOverlapMetrics(field_2);

  AutofillField field_3;
  field_3.set_possible_types({UNKNOWN_TYPE});
  field_3.set_server_predictions(
      {test::CreateFieldPrediction(ADDRESS_HOME_STATE)});
  field_3.set_heuristic_type(GetActiveHeuristicSource(), ADDRESS_HOME_STATE);
  LogFieldPredictionOverlapMetrics(field_3);

  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAggregate.Overall."
      "AllTypes",
      0);
}

TEST_F(PredictionQualityMetricsTest,
       LogFieldPredictionOverlapMetrics_AutocompleteAttributeAbsent) {
  base::HistogramTester histogram_tester;

  AutofillField field;
  field.set_possible_types({NAME_FIRST});
  field.set_server_predictions(
      {test::CreateFieldPrediction(ADDRESS_HOME_STATE)});
  field.set_heuristic_type(GetActiveHeuristicSource(), ADDRESS_HOME_STATE);
  LogFieldPredictionOverlapMetrics(field);

  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributePresent.Overall."
      "AllTypes",
      0);
  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAggregate.Overall."
      "AllTypes",
      1);
  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAbsent.Overall."
      "AllTypes",
      1);
}

TEST_F(PredictionQualityMetricsTest,
       LogFieldPredictionOverlapMetrics_ActiveSourcesEmitted_Heuristics) {
  base::HistogramTester histogram_tester;

  AutofillField field;
  field.set_possible_types({NAME_FIRST});
  field.set_server_predictions({test::CreateFieldPrediction(NO_SERVER_DATA)});
  field.set_heuristic_type(GetActiveHeuristicSource(), NAME_FIRST);
  LogFieldPredictionOverlapMetrics(field);

  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAbsent."
      "HeuristicsActive.AllTypes",
      1);
  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAbsent."
      "HeuristicsActive.NAME_FIRST",
      1);
}

TEST_F(
    PredictionQualityMetricsTest,
    LogFieldPredictionOverlapMetrics_ActiveSourcesEmitted_ServerCrowdsourcing) {
  base::HistogramTester histogram_tester;

  AutofillField field;
  field.set_possible_types({NAME_FIRST});
  field.set_server_predictions({test::CreateFieldPrediction(NAME_FIRST)});
  field.set_heuristic_type(GetActiveHeuristicSource(), UNKNOWN_TYPE);
  LogFieldPredictionOverlapMetrics(field);

  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAbsent."
      "ServerCrowdsourcingActive.AllTypes",
      1);
  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAbsent."
      "ServerCrowdsourcingActive.NAME_FIRST",
      1);
}

TEST_F(PredictionQualityMetricsTest,
       LogFieldPredictionOverlapMetrics_ActiveSourcesEmitted_ServerOverride) {
  base::HistogramTester histogram_tester;

  AutofillField field;
  field.set_possible_types({NAME_FIRST});
  field.set_server_predictions(
      {test::CreateFieldPrediction(NAME_FIRST, /*is_override=*/true)});
  field.set_heuristic_type(GetActiveHeuristicSource(), UNKNOWN_TYPE);
  LogFieldPredictionOverlapMetrics(field);

  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAbsent."
      "ServerOverrideActive.AllTypes",
      1);
  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAbsent."
      "ServerOverrideActive.NAME_FIRST",
      1);
}

TEST_F(PredictionQualityMetricsTest,
       LogFieldPredictionOverlapMetrics_ActiveSourcesEmitted_Autocomplete) {
  base::HistogramTester histogram_tester;

  AutofillField field;
  field.set_possible_types({NAME_FIRST});
  field.set_server_predictions({test::CreateFieldPrediction(NO_SERVER_DATA)});
  field.set_heuristic_type(GetActiveHeuristicSource(), UNKNOWN_TYPE);
  field.SetHtmlType(HtmlFieldType::kGivenName, HtmlFieldMode::kNone);
  LogFieldPredictionOverlapMetrics(field);

  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributePresent."
      "AutocompleteAttributeActive.AllTypes",
      1);
  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributePresent."
      "AutocompleteAttributeActive.NAME_FIRST",
      1);
}

TEST_F(PredictionQualityMetricsTest,
       LogFieldPredictionOverlapMetrics_ActiveSourcesEmitted_Rationalization) {
  base::HistogramTester histogram_tester;

  AutofillField field;
  field.set_possible_types({NAME_FIRST});
  field.SetTypeTo(AutofillType(NAME_FIRST),
                  AutofillPredictionSource::kRationalization);
  LogFieldPredictionOverlapMetrics(field);

  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAbsent."
      "RationalizationActive.AllTypes",
      1);
  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAbsent."
      "RationalizationActive.NAME_FIRST",
      1);
}

TEST_F(PredictionQualityMetricsTest,
       LogFieldPredictionOverlapMetrics_ActiveSourcesEmitted_NoPrediction) {
  base::HistogramTester histogram_tester;

  AutofillField field;
  field.set_possible_types({NAME_FIRST});
  LogFieldPredictionOverlapMetrics(field);

  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAbsent."
      "NoPredictionExists.AllTypes",
      1);
  histogram_tester.ExpectTotalCount(
      "Autofill.FieldPredictionOverlap.AutocompleteAttributeAbsent."
      "NoPredictionExists.NAME_FIRST",
      1);
}

}  // namespace

}  // namespace autofill::autofill_metrics
