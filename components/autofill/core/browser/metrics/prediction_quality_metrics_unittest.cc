// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/prediction_quality_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

// This is defined in the prediction_quality_metrics.cc implementation file.
int GetFieldTypeGroupPredictionQualityMetric(
    FieldType field_type,
    autofill_metrics::FieldTypeQualityMetric metric);

namespace {

using ::autofill::test::CreateTestFormField;
using ::base::Bucket;
using ::base::BucketsAre;

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
  scoped_feature_list.InitWithFeatures(
      {features::kAutofillFixValueSemantics,
       features::kAutofillFixInitialValueOfSelect,
       features::kAutofillFixCurrentValueInImport,
       // Enable model predictions but don't make it the active source.
       features::kAutofillModelPredictions},
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

  std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>
      all_heuristic_types;
  ASSERT_EQ(heuristic_types.size(), ml_types.size());
  for (size_t i = 0; i < heuristic_types.size(); ++i) {
    all_heuristic_types.push_back(
        {{GetActiveHeuristicSource(), heuristic_types[i]},
         {HeuristicSource::kAutofillMachineLearning, ml_types[i]}});
  }

  autofill_manager().AddSeenForm(test::WithoutValues(form), all_heuristic_types,
                                 server_types);

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

    std::vector<FieldType>& predicted_type = [&]() -> std::vector<FieldType>& {
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
        // The first field has the ML prediction type NO_SERVER_DATA because the
        // ML predictions were never executed and NO_SERVER_DATA is used to
        // indicate that a specific heuristic type is unset.
        BucketsAre(source == "Server" || source == "ML"
                       ? Bucket((NO_SERVER_DATA << 16) | EMPTY_TYPE, 1)
                       : Bucket((UNKNOWN_TYPE << 16) | EMPTY_TYPE, 1),
                   Bucket((predicted_type[0] << 16) | actual_types[0], 1),
                   Bucket((predicted_type[1] << 16) | actual_types[1], 1),
                   Bucket((predicted_type[2] << 16) | actual_types[2], 1),
                   Bucket((predicted_type[3] << 16) | actual_types[3], 1)));
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

}  // namespace

}  // namespace autofill::autofill_metrics
