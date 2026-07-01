// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_funnel_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/aliases.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

class AtMemoryFunnelMetricsTest : public testing::Test {
 public:
  AtMemoryFunnelMetricsTest() = default;

 protected:
  base::HistogramTester histogram_tester_;
};

// Tests that `OnPopupShown` correctly logs the "PopupDisplayed" metric when
// triggered by typing the invocation sequence.
TEST_F(AtMemoryFunnelMetricsTest, OnPopupShown_TypedTrigger) {
  AtMemoryFunnelMetrics metrics(nullptr, GURL(), std::u16string());
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.PopupDisplayed",
      AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger, 1);
}

// Tests that `OnPopupShown` correctly logs the "PopupDisplayed" metric when
// triggered via the context menu.
TEST_F(AtMemoryFunnelMetricsTest, OnPopupShown_ContextMenu) {
  AtMemoryFunnelMetrics metrics(nullptr, GURL(), std::u16string());
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryContextMenu);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.PopupDisplayed",
      AutofillMetrics::AtMemoryTriggerSource::kContextMenu, 1);
}

// Tests that `OnPopupShown` is idempotent and only logs a metric for the
// first call in a session.
TEST_F(AtMemoryFunnelMetricsTest, OnPopupShown_Idempotent) {
  AtMemoryFunnelMetrics metrics(nullptr, GURL(), std::u16string());
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
  // Second call should be ignored.
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryContextMenu);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.PopupDisplayed",
      AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger, 1);
}

// Tests that the destructor correctly logs that a query was submitted.
TEST_F(AtMemoryFunnelMetricsTest, Destructor_QuerySubmitted_True) {
  {
    AtMemoryFunnelMetrics metrics(nullptr, GURL(), std::u16string());
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnQuerySubmitted(u"some query");
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.QuerySubmitted", true, 1);
}

// Tests that the destructor correctly logs that no query was submitted
// during a shown session.
TEST_F(AtMemoryFunnelMetricsTest, Destructor_QuerySubmitted_False) {
  {
    AtMemoryFunnelMetrics metrics(nullptr, GURL(), std::u16string());
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    // No query submitted.
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.QuerySubmitted", false, 1);
}

// Tests that the destructor correctly logs that a suggestion was accepted.
TEST_F(AtMemoryFunnelMetricsTest, Destructor_SuggestionAccepted_True) {
  {
    AtMemoryFunnelMetrics metrics(nullptr, GURL(), std::u16string());
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnSuggestionAccepted();
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionAccepted", true, 1);
}

// Tests that the destructor correctly logs that no suggestion was accepted
// during a shown session.
TEST_F(AtMemoryFunnelMetricsTest, Destructor_SuggestionAccepted_False) {
  {
    AtMemoryFunnelMetrics metrics(nullptr, GURL(), std::u16string());
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    // No suggestion accepted.
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionAccepted", false, 1);
}

// Tests that `MarkFilled` correctly logs whether a suggestion was filled.
TEST_F(AtMemoryFunnelMetricsTest, MarkFilled_Filled) {
  {
    AtMemoryFunnelMetrics metrics(nullptr, GURL(), std::u16string());
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnSuggestionAccepted();
    metrics.MarkFilled();
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionFilled", true, 1);

  {
    AtMemoryFunnelMetrics metrics2(nullptr, GURL(), std::u16string());
    metrics2.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics2.OnSuggestionAccepted();
  }

  histogram_tester_.ExpectBucketCount(
      "Autofill.AtMemory.Funnel.SuggestionFilled", false, 1);
}

// Tests that the unmasking duration metric is recorded correctly.
TEST_F(AtMemoryFunnelMetricsTest, TimeToFetchUnmasked) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  {
    AtMemoryFunnelMetrics metrics(nullptr, GURL(), std::u16string());
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnSuggestionAccepted();
    metrics.OnFetchPiiStarted();
    task_environment.FastForwardBy(base::Seconds(2));
    metrics.OnFetchPiiCompleted();
    metrics.MarkFilled();
  }

  histogram_tester_.ExpectUniqueTimeSample(
      "Autofill.AtMemory.Funnel.TimeToFetchUnmasked", base::Seconds(2), 1);
}

// Tests that the ModelQualityLogEntry is correctly filled and uploaded when the
// uploader service is available and is flushed on destruction.
TEST_F(AtMemoryFunnelMetricsTest, LogEntryUploaded) {
  TestingPrefServiceSimple local_state;
  optimization_guide::model_execution::prefs::RegisterLocalStatePrefs(
      local_state.registry());
  optimization_guide::model_execution::prefs::RegisterProfilePrefs(
      local_state.registry());
  optimization_guide::TestModelQualityLogsUploaderService uploader_service(
      &local_state);

  {
    AtMemoryFunnelMetrics metrics(&uploader_service,
                                  GURL("https://example.com"), u"Example Page");
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnQuerySubmitted(u"test query");
  }

  const auto& uploaded_logs = uploader_service.uploaded_logs();
  ASSERT_EQ(uploaded_logs.size(), 1u);
  const optimization_guide::proto::AtMemoryQuality& quality =
      uploaded_logs[0]->at_memory().quality();
  EXPECT_EQ(quality.query(), "test query");
  EXPECT_EQ(quality.url(), "https://example.com/");
  EXPECT_EQ(quality.title(), "Example Page");
}

// Tests that the ModelQualityLogEntry is correctly filled and uploaded when the
// uploader service is available and is flushed on next query.
TEST_F(AtMemoryFunnelMetricsTest, LogEntryUploaded_MultipleQueries) {
  TestingPrefServiceSimple local_state;
  optimization_guide::model_execution::prefs::RegisterLocalStatePrefs(
      local_state.registry());
  optimization_guide::model_execution::prefs::RegisterProfilePrefs(
      local_state.registry());
  optimization_guide::TestModelQualityLogsUploaderService uploader_service(
      &local_state);

  {
    AtMemoryFunnelMetrics metrics(&uploader_service,
                                  GURL("https://example.com"), u"Example Page");
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnQuerySubmitted(u"test query");

    // The first query should be pending, not uploaded yet.
    EXPECT_TRUE(uploader_service.uploaded_logs().empty());

    // Submitting a new query should flush the first query.
    metrics.OnQuerySubmitted(u"next query");
    ASSERT_EQ(uploader_service.uploaded_logs().size(), 1u);
    const optimization_guide::proto::AtMemoryQuality& quality1 =
        uploader_service.uploaded_logs()[0]->at_memory().quality();
    EXPECT_EQ(quality1.query(), "test query");
    EXPECT_EQ(quality1.url(), "https://example.com/");
    EXPECT_EQ(quality1.title(), "Example Page");
  }

  // The second query is flushed on destruction of the object.
  const auto& uploaded_logs = uploader_service.uploaded_logs();
  ASSERT_EQ(uploaded_logs.size(), 2u);
  const optimization_guide::proto::AtMemoryQuality& quality2 =
      uploaded_logs[1]->at_memory().quality();
  EXPECT_EQ(quality2.query(), "next query");
  EXPECT_EQ(quality2.url(), "https://example.com/");
  EXPECT_EQ(quality2.title(), "Example Page");
}

}  // namespace autofill
