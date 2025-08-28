// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/save_and_fill_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

class SaveAndFillMetricsTest : public AutofillMetricsBaseTest,
                               public testing::Test {
 public:
  SaveAndFillMetricsTest() = default;
  ~SaveAndFillMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }
};

TEST_F(SaveAndFillMetricsTest, LogSuggestionShown) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillFormEvent(SaveAndFillFormEvent::kSuggestionShown);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.SaveAndFill",
      /*sample=*/SaveAndFillFormEvent::kSuggestionShown,
      /*expected_count=*/1);
}

TEST_F(SaveAndFillMetricsTest, LogSuggestionAccepted) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillFormEvent(SaveAndFillFormEvent::kSuggestionAccepted);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.SaveAndFill",
      /*sample=*/SaveAndFillFormEvent::kSuggestionAccepted,
      /*expected_count=*/1);
}

TEST_F(SaveAndFillMetricsTest,
       LogSaveAndFillSuggestionNotShownReason_HasSavedCards) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillSuggestionNotShownReason(
      SaveAndFillSuggestionNotShownReason::kHasSavedCards);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveAndFill.SuggestionNotShownReason",
      SaveAndFillSuggestionNotShownReason::kHasSavedCards, 1);
}

TEST_F(SaveAndFillMetricsTest,
       LogSaveAndFillSuggestionNotShownReason_BlockedByStrikeDatabase) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillSuggestionNotShownReason(
      SaveAndFillSuggestionNotShownReason::kBlockedByStrikeDatabase);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveAndFill.SuggestionNotShownReason",
      SaveAndFillSuggestionNotShownReason::kBlockedByStrikeDatabase, 1);
}

TEST_F(SaveAndFillMetricsTest,
       LogSaveAndFillSuggestionNotShownReason_UserInIncognito) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillSuggestionNotShownReason(
      SaveAndFillSuggestionNotShownReason::kUserInIncognito);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveAndFill.SuggestionNotShownReason",
      SaveAndFillSuggestionNotShownReason::kUserInIncognito, 1);
}

TEST_F(SaveAndFillMetricsTest,
       LogSaveAndFillSuggestionNotShownReason_IncompleteCreditCardForm) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillSuggestionNotShownReason(
      SaveAndFillSuggestionNotShownReason::kIncompleteCreditCardForm);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveAndFill.SuggestionNotShownReason",
      SaveAndFillSuggestionNotShownReason::kIncompleteCreditCardForm, 1);
}

TEST_F(SaveAndFillMetricsTest,
       LogGetDetailsForCreateCardRequestLatencyAndResult) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillGetDetailsForCreateCardResultAndLatency(
      /*succeeded=*/true, base::Milliseconds(600));

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveAndFill.GetDetailsForCreateCard.Latency",
      /*sample=*/600, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveAndFill.GetDetailsForCreateCard.Latency.Success",
      /*sample=*/600, 1);
}

TEST_F(SaveAndFillMetricsTest, LogCreateCardRequestLatencyAndResult) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillCreateCardResultAndLatency(
      /*succeeded=*/false, base::Milliseconds(1000));

  histogram_tester.ExpectUniqueSample("Autofill.SaveAndFill.CreateCard.Latency",
                                      /*sample=*/1000, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveAndFill.CreateCard.Latency.Failure",
      /*sample=*/1000, 1);
}

TEST_F(SaveAndFillMetricsTest, LogStrikeDbMetrics_MaxStrikesReached) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillStrikeDatabaseBlockReason(
      AutofillMetrics::AutofillStrikeDatabaseBlockReason::
          kMaxStrikeLimitReached);

  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.SaveAndFillStrikeDatabaseBlockReason",
      /*sample=*/0, 1);
}

TEST_F(SaveAndFillMetricsTest, LogStrikeDbMetrics_RequiredDelayNotMet) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillStrikeDatabaseBlockReason(
      AutofillMetrics::AutofillStrikeDatabaseBlockReason::kRequiredDelayNotMet);

  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.SaveAndFillStrikeDatabaseBlockReason",
      /*sample=*/1, 1);
}

TEST_F(SaveAndFillMetricsTest, LogStrikeDbMetrics_NumOfStrikesPresent) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillNumOfStrikesPresentWhenDialogAccepted(5);

  histogram_tester.ExpectUniqueSample(
      "Autofill.StrikeDatabase.NumOfStrikesPresentWhenSaveAndFillAccepted",
      /*sample=*/5, 1);
}

TEST_F(SaveAndFillMetricsTest, LogDialogResult_AcceptedWithCvc) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillDialogResult(SaveAndFillDialogResult::kAcceptedWithCvc);

  histogram_tester.ExpectUniqueSample("Autofill.SaveAndFill.DialogResult",
                                      SaveAndFillDialogResult::kAcceptedWithCvc,
                                      /*expected_bucket_count=*/1);
}

TEST_F(SaveAndFillMetricsTest, LogDialogResult_AcceptedWithoutCvc) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillDialogResult(SaveAndFillDialogResult::kAcceptedWithoutCvc);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveAndFill.DialogResult",
      SaveAndFillDialogResult::kAcceptedWithoutCvc,
      /*expected_bucket_count=*/1);
}

TEST_F(SaveAndFillMetricsTest, LogDialogResult_Canceled) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillDialogResult(SaveAndFillDialogResult::kCanceled);

  histogram_tester.ExpectUniqueSample("Autofill.SaveAndFill.DialogResult",
                                      SaveAndFillDialogResult::kCanceled,
                                      /*expected_bucket_count=*/1);
}

TEST_F(SaveAndFillMetricsTest, LogDialogShown_Upload) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillDialogShown(/*is_upload=*/true);

  histogram_tester.ExpectUniqueSample("Autofill.SaveAndFill.DialogShown.Upload",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
}

TEST_F(SaveAndFillMetricsTest, LogDialogShown_Local) {
  base::HistogramTester histogram_tester;

  LogSaveAndFillDialogShown(/*is_upload=*/false);

  histogram_tester.ExpectUniqueSample("Autofill.SaveAndFill.DialogShown.Local",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
}

}  // namespace autofill::autofill_metrics
