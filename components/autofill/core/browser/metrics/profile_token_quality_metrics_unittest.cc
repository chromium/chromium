// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/profile_token_quality_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/profile_token_quality.h"
#include "components/autofill/core/browser/profile_token_quality_test_api.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

using ObservationType = ProfileTokenQuality::ObservationType;

TEST(ProfileTokenQualityMetricsTest, LogStoredObservationCount) {
  AutofillProfile profile = test::GetFullProfile();
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST, ObservationType::kAccepted);
  test_api(profile.token_quality())
      .AddObservation(ADDRESS_HOME_STATE,
                      ObservationType::kEditedToSimilarValue);

  base::HistogramTester histogram_tester;
  LogStoredProfileTokenQualityMetrics({&profile});
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileTokenQuality.StoredObservationsCount.PerProfile", 2, 1);
}

TEST(ProfileTokenQualityMetricsTest, LogStoredObservationsPerType) {
  AutofillProfile profile = test::GetFullProfile();
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST, ObservationType::kAccepted);
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST, ObservationType::kEditedFallback);
  test_api(profile.token_quality())
      .AddObservation(ADDRESS_HOME_STATE,
                      ObservationType::kEditedToSimilarValue);

  base::HistogramTester histogram_tester;
  LogStoredProfileTokenQualityMetrics({&profile});
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.ProfileTokenQuality.StoredObservationTypes.NAME_FIRST"),
      base::BucketsAre(base::Bucket(ObservationType::kAccepted, 1),
                       base::Bucket(ObservationType::kEditedFallback, 1)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileTokenQuality.StoredObservationTypes.ADDRESS_HOME_STATE",
      ObservationType::kEditedToSimilarValue, 1);
}

TEST(ProfileTokenQualityMetricsTest, LogStoredTokenQuality) {
  AutofillProfile profile = test::GetFullProfile();
  // NAME_FIRST has a 50% acceptance rate.
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST, ObservationType::kAccepted);
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST, ObservationType::kEditedFallback);
  // ADDRESS_HOME_STATE only has neutral observations -> no metric emitted.
  test_api(profile.token_quality())
      .AddObservation(ADDRESS_HOME_STATE,
                      ObservationType::kEditedToSimilarValue);
  // ADDRESS_HOME_STREET_ADDRESS has a 0% acceptance rate.
  test_api(profile.token_quality())
      .AddObservation(ADDRESS_HOME_STREET_ADDRESS,
                      ObservationType::kEditedToDifferentTokenOfSameProfile);

  base::HistogramTester histogram_tester;
  LogStoredProfileTokenQualityMetrics({&profile});
  histogram_tester.ExpectUniqueSample("Autofill.ProfileTokenQuality.NAME_FIRST",
                                      50, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.ProfileTokenQuality.ADDRESS_HOME_STATE", 0);
  // 33% due to the single accepted observation for NAME_FIRST and the two
  // non-neutral edited observations for NAME_FIRST and
  // ADDRESS_HOME_STREET_ADDRESS. Even though ADDRESS_HOME_LINE1 shares the
  // ADDRESS_HOME_STREET_ADDRESS observations, they are not double counted.
  histogram_tester.ExpectUniqueSample("Autofill.ProfileTokenQuality.PerProfile",
                                      33, 1);
}

TEST(ProfileTokenQualityMetricsTest,
     LogObservationCountBeforeSubmissionMetric) {
  AutofillProfile profile = test::GetFullProfile();
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST, ObservationType::kAccepted);
  test_api(profile.token_quality())
      .AddObservation(NAME_LAST, ObservationType::kAccepted);
  test_api(profile.token_quality())
      .AddObservation(NAME_LAST, ObservationType::kEditedFallback);
  TestPersonalDataManager test_pdm;
  test_pdm.address_data_manager().AddProfile(profile);

  // Create a dummy FormStructure and simulate that the first two fields were
  // filled.
  FormData form_data;
  form_data.set_fields({FormFieldData(), FormFieldData(), FormFieldData()});
  FormStructure form(form_data);
  test_api(form).SetFieldTypes({NAME_FIRST, NAME_LAST, ADDRESS_HOME_CITY});
  form.field(0)->set_autofill_source_profile_guid(profile.guid());
  form.field(1)->set_autofill_source_profile_guid(profile.guid());

  base::HistogramTester histogram_tester;
  LogObservationCountBeforeSubmissionMetric(form, test_pdm);
  const std::string kBaseMetricName =
      "Autofill.ProfileTokenQuality.ObservationCountBeforeSubmission.";
  histogram_tester.ExpectUniqueSample(kBaseMetricName + "NAME_FIRST", 1, 1);
  histogram_tester.ExpectUniqueSample(kBaseMetricName + "NAME_LAST", 2, 1);
  histogram_tester.ExpectTotalCount(kBaseMetricName + "ADDRESS_HOME_CITY", 0);
  histogram_tester.ExpectUniqueSample(kBaseMetricName + "PerProfile", 3, 1);
}

// Tests that "Autofill.ProfileTokenQualityScore" is emitted with
// correctly calculated bucket values and quality scores.
TEST(ProfileTokenQualityMetricsTest, LogProfileTokenQualityScoreMetric) {
  AutofillProfile profile = test::GetFullProfile();
  // `NAME_FIRST` has one good and one bad observation, resulting in a quality
  // score of 5.
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST, ObservationType::kAccepted);
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST, ObservationType::kEditedFallback);
  // `NAME_LAST` has one good observation, resulting in a quality score of 10.
  test_api(profile.token_quality())
      .AddObservation(NAME_LAST, ObservationType::kAccepted);
  // `ADDRESS_HOME_STATE` has one neutral observation, resulting in a quality
  // score of 5.
  test_api(profile.token_quality())
      .AddObservation(ADDRESS_HOME_STATE,
                      ObservationType::kEditedToSimilarValue);
  // `ADDRESS_HOME_STATE` has one bad observation, resulting in a quality level
  // of 0.
  test_api(profile.token_quality())
      .AddObservation(ADDRESS_HOME_STREET_ADDRESS,
                      ObservationType::kEditedToDifferentTokenOfSameProfile);
  TestPersonalDataManager test_pdm;
  test_pdm.address_data_manager().AddProfile(profile);

  // Create a dummy FormStructure and simulate that the first two fields
  // were filled.
  FormData form_data;
  form_data.set_fields({FormFieldData(), FormFieldData(), FormFieldData(),
                        FormFieldData(), FormFieldData()});
  FormStructure form(form_data);
  test_api(form).SetFieldTypes({NAME_FIRST, NAME_LAST, ADDRESS_HOME_STATE,
                                ADDRESS_HOME_STREET_ADDRESS,
                                ADDRESS_HOME_CITY});
  form.field(0)->set_autofill_source_profile_guid(profile.guid());
  form.field(1)->set_autofill_source_profile_guid(profile.guid());
  form.field(2)->set_autofill_source_profile_guid(profile.guid());
  form.field(3)->set_autofill_source_profile_guid(profile.guid());

  base::HistogramTester histogram_tester;
  LogProfileTokenQualityScoreMetric(form, test_pdm);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ProfileTokenQualityScore"),
      base::BucketsAre(
          // 850 corresponds to n = 2, quality score = 5, field type =
          // `NAME_FIRST`.
          base::Bucket(850, 1),
          // 1441 corresponds to n = 1, quality score = 10, field type =
          // `NAME_LAST`.
          base::Bucket(1441, 1),
          // 8785 corresponds to n = 1, quality score = 5, field type =
          // `ADDRESS_HOME_STATE`.
          base::Bucket(8785, 1),
          // 19713 corresponds to n = 1, quality score = 0, field type =
          // `ADDRESS_HOME_STREET_ADDRESS`.
          base::Bucket(19713, 1)));
}

}  // namespace

}  // namespace autofill::autofill_metrics
