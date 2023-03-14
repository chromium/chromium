// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/precedence_over_autocomplete_metrics.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::Bucket;
using ::base::BucketsAre;

namespace autofill::autofill_metrics {

class PrecedenceOverAutocompleteMetricsTest
    : public autofill_metrics::AutofillMetricsBaseTest,
      public testing::Test {
 public:
  void SetUp() override { SetUpHelper(); }
  void TearDown() override { TearDownHelper(); }
};

// Tests the logging of autocomplete values for fields predicted as street name
// or house number by heuristics or server.
TEST_F(
    PrecedenceOverAutocompleteMetricsTest,
    AutocompleteValuesForAutofilledFieldWithStreetNameOrHouseNumberPredictions) {
  test::FormDescription form_description = {
      .description_for_logging = "NumberOfAutofilledFields",
      .fields = {{.server_type = ADDRESS_HOME_STREET_NAME,
                  .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER,
                  .value = u"405",
                  .autocomplete_attribute = "address-line1",
                  .is_autofilled = true},

                 {.server_type = ADDRESS_HOME_STREET_NAME,
                  .heuristic_type = NAME_FIRST,
                  .value = u"Some Random Street Name",
                  .autocomplete_attribute = "given-name",
                  .is_autofilled = true},

                 {.server_type = NAME_LAST,
                  .heuristic_type = ADDRESS_HOME_STREET_NAME,
                  .value = u"Some Random Street Name",
                  .autocomplete_attribute = "unrecognized",
                  .is_autofilled = true},

                 {.server_type = ADDRESS_HOME_HOUSE_NUMBER,
                  .heuristic_type = NAME_LAST,
                  .value = u"405",
                  .autocomplete_attribute = "off",
                  .is_autofilled = true}},

      .unique_renderer_id = test::MakeFormRendererId(),
      .main_frame_origin =
          url::Origin::Create(autofill_client_->form_origin())};

  FormData form = GetAndAddSeenForm(form_description);
  base::HistogramTester histogram_tester;
  SubmitForm(form);

  const std::string any_field_type = "StreetNameOrHouseNumber";
  const std::string house_number_field_type = "HouseNumber";
  const std::string street_name_field_type = "StreetName";
  const std::string any_prediction_type = "HeuristicOrServer";
  const std::string heuristic_prediction_type = "Heuristic";
  const std::string server_prediction_type = "Server";
  auto histogram_name = [&](const std::string& current_field_type,
                            const std::string& current_prediction_type) {
    return base::StrCat({"Autofill.AutocompleteAttributeForFieldsWith.",
                         current_field_type, ".", current_prediction_type,
                         ".Prediction"});
  };
  using AutocompleteCategory =
      AutocompleteValueForStructuredAddressPredictedFieldsMetric;

  EXPECT_THAT(histogram_tester.GetAllSamples(histogram_name(
                  house_number_field_type, heuristic_prediction_type)),
              BucketsAre(Bucket(AutocompleteCategory::kAddressLine1And2, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(histogram_name(
                  street_name_field_type, heuristic_prediction_type)),
              BucketsAre(Bucket(AutocompleteCategory::kUnrecognized, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  histogram_name(any_field_type, heuristic_prediction_type)),
              BucketsAre(Bucket(AutocompleteCategory::kAddressLine1And2, 1),
                         Bucket(AutocompleteCategory::kUnrecognized, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(histogram_name(
                  house_number_field_type, server_prediction_type)),
              BucketsAre(Bucket(AutocompleteCategory::kUnspecified, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(histogram_name(
                  street_name_field_type, server_prediction_type)),
              BucketsAre(Bucket(AutocompleteCategory::kAddressLine1And2, 1),
                         Bucket(AutocompleteCategory::kOtherRecognized, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  histogram_name(any_field_type, server_prediction_type)),
              BucketsAre(Bucket(AutocompleteCategory::kAddressLine1And2, 1),
                         Bucket(AutocompleteCategory::kOtherRecognized, 1),
                         Bucket(AutocompleteCategory::kUnspecified, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  histogram_name(house_number_field_type, any_prediction_type)),
              BucketsAre(Bucket(AutocompleteCategory::kAddressLine1And2, 1),
                         Bucket(AutocompleteCategory::kUnspecified, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  histogram_name(street_name_field_type, any_prediction_type)),
              BucketsAre(Bucket(AutocompleteCategory::kAddressLine1And2, 1),
                         Bucket(AutocompleteCategory::kOtherRecognized, 1),
                         Bucket(AutocompleteCategory::kUnrecognized, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  histogram_name(any_field_type, any_prediction_type)),
              BucketsAre(Bucket(AutocompleteCategory::kAddressLine1And2, 2),
                         Bucket(AutocompleteCategory::kOtherRecognized, 1),
                         Bucket(AutocompleteCategory::kUnrecognized, 1),
                         Bucket(AutocompleteCategory::kUnspecified, 1)));
}

// Tests the logging of StreetName and HouseNumber precedence over autocomplete
// correctness.
TEST_F(PrecedenceOverAutocompleteMetricsTest,
       EditedAutofilledFieldWithStreetNameOrHouseNumberPrecedenceAtSubmission) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillStreetNameOrHouseNumberPrecedenceOverAutocomplete,
      {{features::kAutofillHeuristicPrecedenceScopeOverAutocomplete.name,
        features::kAutofillHeuristicPrecedenceScopeOverAutocomplete.GetName(
            features::PrecedenceOverAutocompleteScope::kSpecified)},
       {features::kAutofillServerPrecedenceScopeOverAutocomplete.name,
        features::kAutofillServerPrecedenceScopeOverAutocomplete.GetName(
            features::PrecedenceOverAutocompleteScope::kSpecified)}});

  test::FormDescription form_description = {
      .description_for_logging = "NumberOfAutofilledFields",
      .fields = {{.server_type = ADDRESS_HOME_STREET_NAME,
                  .heuristic_type = ADDRESS_HOME_HOUSE_NUMBER,
                  .value = u"405",
                  .autocomplete_attribute = "address-line1",
                  .is_autofilled = true},

                 {.server_type = ADDRESS_HOME_STREET_NAME,
                  .heuristic_type = NAME_FIRST,
                  .value = u"Some Random Street Name",
                  .autocomplete_attribute = "unrecognized",
                  .is_autofilled = true},

                 {.server_type = ADDRESS_HOME_HOUSE_NUMBER,
                  .heuristic_type = NAME_LAST,
                  .value = u"405",
                  .autocomplete_attribute = "off",
                  .is_autofilled = true},

                 {.server_type = NAME_FIRST,
                  .heuristic_type = NAME_LAST,
                  .value = u"ojijo",
                  .autocomplete_attribute = "address-line2",
                  .is_autofilled = true}},

      .unique_renderer_id = test::MakeFormRendererId(),
      .main_frame_origin =
          url::Origin::Create(autofill_client_->form_origin())};

  FormData form = GetAndAddSeenForm(form_description);
  base::HistogramTester histogram_tester;

  // Simulate text input in the first and third fields.
  SimulateUserChangedTextField(form, form.fields[0]);
  SimulateUserChangedTextField(form, form.fields[2]);
  SubmitForm(form);

  const std::string any_precedence_type = "HeuristicOrServer";
  const std::string heuristic_precedence_type = "Heuristic";
  const std::string server_precedence_type = "Server";
  const std::string any_field_type = "StreetNameOrHouseNumber";
  const std::string house_number_field_type = "HouseNumber";
  const std::string street_name_field_type = "StreetName";
  const std::string any_autocomplete_status = "Specified";
  const std::string recognized_autocomplete_status = "Recognized";
  const std::string unrecognized_autocomplete_status = "Unrecognized";
  auto histogram_name = [&](const std::string& current_precedence_type,
                            const std::string& current_field_type,
                            const std::string& current_autocomplete_status) {
    return base::StrCat(
        {"Autofill.", current_precedence_type,
         "Precedence.OverAutocompleteForStructuredAddressFields.",
         current_field_type, ".AutocompleteIs.", current_autocomplete_status});
  };

  EXPECT_THAT(histogram_tester.GetAllSamples(histogram_name(
                  heuristic_precedence_type, house_number_field_type,
                  recognized_autocomplete_status)),
              BucketsAre(Bucket(
                  AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                      AUTOFILLED_FIELD_WAS_EDITED,
                  1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  histogram_name(any_precedence_type, house_number_field_type,
                                 recognized_autocomplete_status)),
              BucketsAre(Bucket(
                  AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                      AUTOFILLED_FIELD_WAS_EDITED,
                  1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  histogram_name(heuristic_precedence_type, any_field_type,
                                 recognized_autocomplete_status)),
              BucketsAre(Bucket(
                  AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                      AUTOFILLED_FIELD_WAS_EDITED,
                  1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(histogram_name(
          any_precedence_type, any_field_type, recognized_autocomplete_status)),
      BucketsAre(
          Bucket(AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                     AUTOFILLED_FIELD_WAS_EDITED,
                 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  histogram_name(server_precedence_type, street_name_field_type,
                                 unrecognized_autocomplete_status)),
              BucketsAre(Bucket(
                  AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                      AUTOFILLED_FIELD_WAS_NOT_EDITED,
                  1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  histogram_name(any_precedence_type, street_name_field_type,
                                 unrecognized_autocomplete_status)),
              BucketsAre(Bucket(
                  AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                      AUTOFILLED_FIELD_WAS_NOT_EDITED,
                  1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  histogram_name(server_precedence_type, any_field_type,
                                 unrecognized_autocomplete_status)),
              BucketsAre(Bucket(
                  AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                      AUTOFILLED_FIELD_WAS_NOT_EDITED,
                  1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  histogram_name(any_precedence_type, any_field_type,
                                 unrecognized_autocomplete_status)),
              BucketsAre(Bucket(
                  AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                      AUTOFILLED_FIELD_WAS_NOT_EDITED,
                  1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(histogram_name(
                  heuristic_precedence_type, house_number_field_type,
                  any_autocomplete_status)),
              BucketsAre(Bucket(
                  AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                      AUTOFILLED_FIELD_WAS_EDITED,
                  1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  histogram_name(any_precedence_type, house_number_field_type,
                                 any_autocomplete_status)),
              BucketsAre(Bucket(
                  AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                      AUTOFILLED_FIELD_WAS_EDITED,
                  1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  histogram_name(server_precedence_type, street_name_field_type,
                                 any_autocomplete_status)),
              BucketsAre(Bucket(
                  AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                      AUTOFILLED_FIELD_WAS_NOT_EDITED,
                  1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  histogram_name(any_precedence_type, street_name_field_type,
                                 any_autocomplete_status)),
              BucketsAre(Bucket(
                  AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                      AUTOFILLED_FIELD_WAS_NOT_EDITED,
                  1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(histogram_name(
          heuristic_precedence_type, any_field_type, any_autocomplete_status)),
      BucketsAre(
          Bucket(AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                     AUTOFILLED_FIELD_WAS_EDITED,
                 1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(histogram_name(
          server_precedence_type, any_field_type, any_autocomplete_status)),
      BucketsAre(
          Bucket(AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                     AUTOFILLED_FIELD_WAS_NOT_EDITED,
                 1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(histogram_name(
          any_precedence_type, any_field_type, any_autocomplete_status)),
      BucketsAre(
          Bucket(AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                     AUTOFILLED_FIELD_WAS_EDITED,
                 1),
          Bucket(AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                     AUTOFILLED_FIELD_WAS_NOT_EDITED,
                 1)));
}

}  // namespace autofill::autofill_metrics
