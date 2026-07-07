// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/signatures.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

namespace {

using ::accessibility_annotator::MemoryDataType;
using ::accessibility_annotator::MemoryEntrySource;
using ::accessibility_annotator::MemoryEntrySourceType;
using ::accessibility_annotator::MemorySearchResult;
using ::accessibility_annotator::MemorySearchResults;
using ::accessibility_annotator::MemorySearchStatus;

class AtMemoryMetricsRecorderTest : public testing::Test {
 public:
  AtMemoryMetricsRecorderTest() = default;

 protected:
  void SendResponse(AtMemoryMetricsRecorder& metrics) {
    metrics.OnQueryResponseReceived(
        MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                            {MemorySearchResult(MemoryDataType::kAddressFull,
                                                u"Address", u"123 Main St")}));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
};

// Tests that `OnPopupShown` correctly logs the "PopupDisplayed" metric when
// triggered by typing the invocation sequence.
TEST_F(AtMemoryMetricsRecorderTest, OnPopupShown_TypedTrigger) {
  AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                  FormSignature(0), FieldSignature(0));
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.SearchBarDisplayed",
      AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger, 1);
}

// Tests that `OnPopupShown` correctly logs the "PopupDisplayed" metric when
// triggered via the context menu.
TEST_F(AtMemoryMetricsRecorderTest, OnPopupShown_ContextMenu) {
  AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                  FormSignature(0), FieldSignature(0));
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryContextMenu);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.SearchBarDisplayed",
      AutofillMetrics::AtMemoryTriggerSource::kContextMenu, 1);
}

// Tests that `OnPopupShown` is idempotent and only logs a metric for the
// first call in a session.
TEST_F(AtMemoryMetricsRecorderTest, OnPopupShown_Idempotent) {
  AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                  FormSignature(0), FieldSignature(0));
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
  // Second call should be ignored.
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryContextMenu);

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.SearchBarDisplayed",
      AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger, 1);
}

// Tests that the destructor correctly logs that a query was submitted.
TEST_F(AtMemoryMetricsRecorderTest, Destructor_QuerySubmitted_True) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnQuerySubmitted(u"some query");
  }

  histogram_tester_.ExpectUniqueSample("Autofill.AtMemory.QuerySubmitted", true,
                                       1);
}

// Tests that the destructor correctly logs that no query was submitted
// during a shown session.
TEST_F(AtMemoryMetricsRecorderTest, Destructor_QuerySubmitted_False) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    // No query submitted.
  }

  histogram_tester_.ExpectUniqueSample("Autofill.AtMemory.QuerySubmitted",
                                       false, 1);
}

// Tests that the destructor correctly logs that a suggestion was accepted.
TEST_F(AtMemoryMetricsRecorderTest, Destructor_SuggestionAccepted_True) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnQuerySubmitted(u"query");
    SendResponse(metrics);
    metrics.OnSuggestionAccepted(MemoryDataType::kAddressFull);
  }

  histogram_tester_.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                       true, 1);
}

// Tests that the metric is NOT logged if no query was submitted.
TEST_F(AtMemoryMetricsRecorderTest,
       Destructor_NoQuerySubmitted_NoSuggestionAcceptedMetric) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    // No query submitted, no suggestion accepted.
  }

  histogram_tester_.ExpectTotalCount("Autofill.AtMemory.SuggestionAccepted", 0);
}

// Tests that the destructor correctly logs that no suggestion was accepted
// if a query was submitted.
TEST_F(AtMemoryMetricsRecorderTest,
       Destructor_QuerySubmitted_SuggestionAccepted_False) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnQuerySubmitted(u"query");
    SendResponse(metrics);
    // No suggestion accepted.
  }

  histogram_tester_.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                       false, 1);
}

// Tests that suggestion accepted metric is logged for multiple queries.
// Specifically, a query that is not accepted, followed by a query that is
// accepted.
TEST_F(AtMemoryMetricsRecorderTest,
       MultipleQueries_SuggestionAccepted_MultipleEmissions) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);

    // Query 1: suggestion not accepted.
    metrics.OnQuerySubmitted(u"query 1");
    SendResponse(metrics);

    // Query 2: suggestion accepted.
    // Submitting query 2 should log the result of query 1 (false).
    metrics.OnQuerySubmitted(u"query 2");
    SendResponse(metrics);
    histogram_tester_.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                         false, 1);

    metrics.OnSuggestionAccepted(MemoryDataType::kAddressFull);
    // Destructor should log the result of query 2 (true).
  }

  histogram_tester_.ExpectBucketCount("Autofill.AtMemory.SuggestionAccepted",
                                      true, 1);
  histogram_tester_.ExpectBucketCount("Autofill.AtMemory.SuggestionAccepted",
                                      false, 1);
}

// Tests that QueryCountBeforeAcceptance logs 1 if only one query was submitted
// before acceptance.
TEST_F(AtMemoryMetricsRecorderTest, QueryCountBeforeAcceptance_OneQuery) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnQuerySubmitted(u"query 1");
    SendResponse(metrics);
    metrics.OnSuggestionAccepted(MemoryDataType::kAddressFull);
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.QueryCountBeforeAcceptance", 1, 1);
}

// Tests that QueryCountBeforeAcceptance logs the correct count if multiple
// queries were submitted before acceptance.
TEST_F(AtMemoryMetricsRecorderTest,
       QueryCountBeforeAcceptance_MultipleQueries) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnQuerySubmitted(u"query 1");
    SendResponse(metrics);
    metrics.OnQuerySubmitted(u"query 2");
    SendResponse(metrics);
    metrics.OnSuggestionAccepted(MemoryDataType::kAddressFull);
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.QueryCountBeforeAcceptance", 2, 1);
}

// Tests that QueryCountBeforeAcceptance is not logged if no suggestion was
// accepted.
TEST_F(AtMemoryMetricsRecorderTest, QueryCountBeforeAcceptance_NoAcceptance) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnQuerySubmitted(u"query 1");
    SendResponse(metrics);
    metrics.OnQuerySubmitted(u"query 2");
    SendResponse(metrics);
    // No suggestion accepted.
  }

  histogram_tester_.ExpectTotalCount(
      "Autofill.AtMemory.QueryCountBeforeAcceptance", 0);
}

// Tests that `MarkFilled` correctly logs whether a suggestion was filled.
TEST_F(AtMemoryMetricsRecorderTest, MarkFilled_Filled) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnSuggestionAccepted(MemoryDataType::kAddressFull);
    metrics.MarkFilled();
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.Funnel.SuggestionFilled", true, 1);

  {
    AtMemoryMetricsRecorder metrics2(nullptr, GURL(), std::u16string(),
                                     FormSignature(0), FieldSignature(0));
    metrics2.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics2.OnSuggestionAccepted(MemoryDataType::kAddressFull);
  }

  histogram_tester_.ExpectBucketCount(
      "Autofill.AtMemory.Funnel.SuggestionFilled", false, 1);
}

// Tests that the unmasking duration metric is recorded correctly.
TEST_F(AtMemoryMetricsRecorderTest, TimeToFetchUnmasked) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnSuggestionAccepted(MemoryDataType::kAddressFull);
    metrics.OnFetchPiiStarted();
    task_environment_.FastForwardBy(base::Seconds(2));
    metrics.OnFetchPiiCompleted();
    metrics.MarkFilled();
  }

  histogram_tester_.ExpectUniqueTimeSample(
      "Autofill.AtMemory.Funnel.TimeToFetchUnmasked", base::Seconds(2), 1);
}

// Tests that the ModelQualityLogEntry is correctly filled and uploaded when the
// uploader service is available and is flushed on destruction.
TEST_F(AtMemoryMetricsRecorderTest, LogEntryUploaded) {
  base::HistogramTester histogram_tester;

  TestingPrefServiceSimple local_state;
  optimization_guide::model_execution::prefs::RegisterLocalStatePrefs(
      local_state.registry());
  optimization_guide::model_execution::prefs::RegisterProfilePrefs(
      local_state.registry());
  optimization_guide::TestModelQualityLogsUploaderService uploader_service(
      &local_state);

  {
    AtMemoryMetricsRecorder metrics(
        &uploader_service, GURL("https://example.com"), u"Example Page",
        FormSignature(123), FieldSignature(456));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnQuerySubmitted(u"test query");
    task_environment_.FastForwardBy(base::Milliseconds(100));
    MemorySearchResult local_suggestion(MemoryDataType::kAddressFull, u"key 1",
                                        u"value1");
    local_suggestion.sources.push_back(
        MemoryEntrySource(MemoryEntrySourceType::kAutofill));
    MemorySearchResult remote_suggestion(MemoryDataType::kUnknown, u"key 2",
                                         u"value2");
    remote_suggestion.remote_response_index = 0;
    remote_suggestion.sources = {
        MemoryEntrySource(MemoryEntrySourceType::kGmail)};
    metrics.OnQueryResponseReceived(
        MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                            {local_suggestion, remote_suggestion}));
  }

  const auto& uploaded_logs = uploader_service.uploaded_logs();
  ASSERT_EQ(uploaded_logs.size(), 1u);
  const optimization_guide::proto::AtMemoryQuality& quality =
      uploaded_logs[0]->at_memory().quality();
  EXPECT_EQ(quality.query(), "test query");
  EXPECT_EQ(quality.url(), "https://example.com/");
  EXPECT_EQ(quality.title(), "Example Page");
  EXPECT_EQ(quality.form_signature(), 123u);
  EXPECT_EQ(quality.field_signature(), 456u);
  EXPECT_FALSE(quality.session_id().empty());
  EXPECT_EQ(quality.query_submitted_to_suggestions_shown_ms(), 100);
  EXPECT_EQ(quality.suggestions_size(), 2);
  EXPECT_EQ(quality.suggestions(0).source(),
            optimization_guide::proto::AT_MEMORY_SUGGESTION_SOURCE_AUTOFILL);
  EXPECT_EQ(quality.suggestions(0).response_results_index(), -1);
  EXPECT_EQ(quality.suggestions(1).source(),
            optimization_guide::proto::
                AT_MEMORY_SUGGESTION_SOURCE_CONTEXT_MEMORY_SERVICE);
  EXPECT_EQ(quality.suggestions(1).response_results_index(), 0);

  histogram_tester.ExpectTotalCount("Autofill.AtMemory.Latency.Query", 1);
}

// Tests that the ModelQualityLogEntry is correctly filled and uploaded when the
// uploader service is available and is flushed on next query.
TEST_F(AtMemoryMetricsRecorderTest, LogEntryUploaded_MultipleQueries) {
  TestingPrefServiceSimple local_state;
  optimization_guide::model_execution::prefs::RegisterLocalStatePrefs(
      local_state.registry());
  optimization_guide::model_execution::prefs::RegisterProfilePrefs(
      local_state.registry());
  optimization_guide::TestModelQualityLogsUploaderService uploader_service(
      &local_state);

  std::string quality1_session_id;
  {
    AtMemoryMetricsRecorder metrics(
        &uploader_service, GURL("https://example.com"), u"Example Page",
        FormSignature(123), FieldSignature(456));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnQuerySubmitted(u"test query");

    // The first query should be pending, not uploaded yet.
    EXPECT_TRUE(uploader_service.uploaded_logs().empty());

    // Submitting a new query should flush the first query.
    metrics.OnQuerySubmitted(u"next query");
    ASSERT_EQ(uploader_service.uploaded_logs().size(), 1u);
    const optimization_guide::proto::AtMemoryQuality& quality1 =
        uploader_service.uploaded_logs()[0]->at_memory().quality();
    quality1_session_id = quality1.session_id();
    EXPECT_EQ(quality1.query(), "test query");
    EXPECT_EQ(quality1.url(), "https://example.com/");
    EXPECT_EQ(quality1.title(), "Example Page");
    EXPECT_EQ(quality1.form_signature(), 123u);
    EXPECT_EQ(quality1.field_signature(), 456u);
    EXPECT_FALSE(quality1.session_id().empty());
  }

  // The second query is flushed on destruction of the object.
  const auto& uploaded_logs = uploader_service.uploaded_logs();
  ASSERT_EQ(uploaded_logs.size(), 2u);
  const optimization_guide::proto::AtMemoryQuality& quality2 =
      uploaded_logs[1]->at_memory().quality();
  EXPECT_EQ(quality2.query(), "next query");
  EXPECT_EQ(quality2.url(), "https://example.com/");
  EXPECT_EQ(quality2.title(), "Example Page");
  EXPECT_EQ(quality2.form_signature(), 123u);
  EXPECT_EQ(quality2.field_signature(), 456u);
  EXPECT_FALSE(quality2.session_id().empty());
  EXPECT_EQ(quality2.session_id(), quality1_session_id);
}

// Tests that `OnSuggestionAccepted` correctly logs the accepted suggestion
// indices to UMA.
TEST_F(AtMemoryMetricsRecorderTest, OnSuggestionAccepted_LogsIndices) {
  // Test case 1: Accept root suggestion (index 2).
  {
    base::HistogramTester histogram_tester;
    AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnSuggestionAccepted(
        MemoryDataType::kAddressFull,
        AutofillSuggestionDelegate::SuggestionMetadata{.multi_index = {2}});
    histogram_tester.ExpectUniqueSample(
        "Autofill.AtMemory.AcceptedSuggestionIndex", 2, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.AtMemory.AcceptedSuggestionSecondaryIndex", -1, 1);
  }

  // Test case 2: Accept sub-suggestion (parent index 2, child index 1).
  {
    base::HistogramTester histogram_tester;
    AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnSuggestionAccepted(
        MemoryDataType::kAddressFull,
        AutofillSuggestionDelegate::SuggestionMetadata{.multi_index = {2, 1}});
    histogram_tester.ExpectUniqueSample(
        "Autofill.AtMemory.AcceptedSuggestionIndex", 2, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.AtMemory.AcceptedSuggestionSecondaryIndex", 1, 1);
  }
}

// Tests that if we receive an empty response, SuggestionAccepted is not logged.
TEST_F(AtMemoryMetricsRecorderTest, EmptyResponse_NoSuggestionAcceptedMetric) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemory);
    metrics.OnQuerySubmitted(u"query");

    // Simulate empty response.
    metrics.OnQueryResponseReceived(
        MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess, {}));
  }

  histogram_tester_.ExpectTotalCount("Autofill.AtMemory.SuggestionAccepted", 0);
}

struct QueryCompletedTestCase {
  MemorySearchResults search_result;
  std::optional<AtMemoryQueryCompletedStatus> expected_status;
};

class AtMemoryMetricsRecorderQueryCompletedTest
    : public AtMemoryMetricsRecorderTest,
      public testing::WithParamInterface<QueryCompletedTestCase> {};

// Tests that `OnQueryResponseReceived` logs the "QueryCompleted" metric
// with the appropriate status.
TEST_P(AtMemoryMetricsRecorderQueryCompletedTest, LogsQueryCompletedMetric) {
  const QueryCompletedTestCase& test_case = GetParam();
  AtMemoryMetricsRecorder metrics(nullptr, GURL(), std::u16string(),
                                  FormSignature(0), FieldSignature(0));

  metrics.OnQueryResponseReceived(test_case.search_result);

  if (test_case.expected_status) {
    histogram_tester_.ExpectUniqueSample("Autofill.AtMemory.QueryCompleted",
                                         *test_case.expected_status, 1);
  } else {
    histogram_tester_.ExpectTotalCount("Autofill.AtMemory.QueryCompleted", 0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    AtMemoryMetricsRecorderTest,
    AtMemoryMetricsRecorderQueryCompletedTest,
    testing::Values(
        // Success with data.
        QueryCompletedTestCase{
            .search_result =
                MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                                    /*entries=*/{{MemoryDataType::kAddressFull,
                                                  u"Address", u"Value"}}),
            .expected_status =
                AtMemoryQueryCompletedStatus::kQueryReturnedData},
        QueryCompletedTestCase{
            .search_result =
                MemorySearchResults(MemorySearchStatus::kUnsupportedQuery,
                                    /*entries=*/{}),
            .expected_status = AtMemoryQueryCompletedStatus::kQueryUnsupported},
        // Success without data.
        QueryCompletedTestCase{
            .search_result =
                MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                                    /*entries=*/{}),
            .expected_status = AtMemoryQueryCompletedStatus::kNoData},
        QueryCompletedTestCase{
            .search_result =
                MemorySearchResults(MemorySearchStatus::kNoConnectionFailure,
                                    /*entries=*/{}),
            .expected_status = AtMemoryQueryCompletedStatus::kNetworkError},
        QueryCompletedTestCase{
            .search_result =
                MemorySearchResults(MemorySearchStatus::kInferenceFailure,
                                    /*entries=*/{}),
            .expected_status = AtMemoryQueryCompletedStatus::kInferenceFailure},
        QueryCompletedTestCase{
            .search_result =
                MemorySearchResults(MemorySearchStatus::kInternalFailure,
                                    /*entries=*/{}),
            .expected_status = AtMemoryQueryCompletedStatus::kInternalError},
        // Partial response (ignored).
        QueryCompletedTestCase{.search_result = MemorySearchResults(
                                   MemorySearchStatus::kPartialResponseSuccess,
                                   /*entries=*/{}),
                               .expected_status = std::nullopt}));

}  // namespace

}  // namespace autofill
