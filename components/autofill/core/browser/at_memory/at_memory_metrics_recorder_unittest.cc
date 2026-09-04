// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_metrics_recorder.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_util.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/signatures.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

namespace {

using ::testing::ElementsAre;
using ::testing::Property;
using ::testing::Values;

constexpr ukm::SourceId kTestSourceId = static_cast<ukm::SourceId>(123);

testing::Matcher<const optimization_guide::proto::AtMemorySuggestion&>
HasAction(optimization_guide::proto::AtMemorySuggestionAction action) {
  return Property(&optimization_guide::proto::AtMemorySuggestion::action,
                  action);
}

class AtMemoryMetricsRecorderTest : public testing::Test {
 public:
  AtMemoryMetricsRecorderTest() {
    optimization_guide::model_execution::prefs::RegisterLocalStatePrefs(
        local_state_.registry());
    optimization_guide::model_execution::prefs::RegisterProfilePrefs(
        local_state_.registry());
    uploader_service_ =
        std::make_unique<optimization_guide::TestModelQualityLogsUploaderService>(
            &local_state_);
  }

 protected:
  void SendResponse(AtMemoryMetricsRecorder& metrics) {
    metrics.OnQueryResponseReceived(
        MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                            {MemorySearchResult(MemoryDataType::kAddressFull,
                                                u"Address", u"123 Main St")}));
  }

  autofill::test::AutofillUnitTestEnvironment autofill_test_env_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<optimization_guide::TestModelQualityLogsUploaderService>
      uploader_service_;
};

// Tests that `OnPopupShown` correctly logs the "SearchBarDisplayed" metric when
// triggered by typing the invocation sequence.
TEST_F(AtMemoryMetricsRecorderTest, OnPopupShown_TypedTrigger) {
  AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                  GURL(), std::u16string(), FieldGlobalId(),
                                  FormSignature(0), FieldSignature(0));
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
                       /*metadata=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.SearchBarDisplayed",
      AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger, 1);
}

// Tests that `OnPopupShown` correctly logs the "SearchBarDisplayed" metric when
// triggered via the context menu.
TEST_F(AtMemoryMetricsRecorderTest, OnPopupShown_ContextMenu) {
  AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                  GURL(), std::u16string(), FieldGlobalId(),
                                  FormSignature(0), FieldSignature(0));
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryContextMenu,
                       /*metadata=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.SearchBarDisplayed",
      AutofillMetrics::AtMemoryTriggerSource::kContextMenu, 1);
}

// Tests that `OnPopupShown` correctly logs the "SearchBarDisplayed" metric when
// triggered via double Ctrl.
TEST_F(AtMemoryMetricsRecorderTest, OnPopupShown_DoubleCtrl) {
  AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                  GURL(), std::u16string(), FieldGlobalId(),
                                  FormSignature(0), FieldSignature(0));
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl,
                       /*metadata=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.SearchBarDisplayed",
      AutofillMetrics::AtMemoryTriggerSource::kDoubleCtrl, 1);
}

// Tests that `OnPopupShown` is idempotent and only logs a metric for the
// first call in a session.
TEST_F(AtMemoryMetricsRecorderTest, OnPopupShown_Idempotent) {
  AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                  GURL(), std::u16string(), FieldGlobalId(),
                                  FormSignature(0), FieldSignature(0));
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
                       /*metadata=*/{});
  // Second call should be ignored.
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryContextMenu,
                       /*metadata=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.SearchBarDisplayed",
      AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger, 1);
}

// Tests that the destructor correctly logs that a query was submitted.
TEST_F(AtMemoryMetricsRecorderTest, Destructor_QuerySubmitted_True) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"some query");
  }

  histogram_tester_.ExpectUniqueSample("Autofill.AtMemory.QuerySubmitted", true,
                                       1);
}

// Tests that the destructor correctly logs that no query was submitted
// during a shown session.
TEST_F(AtMemoryMetricsRecorderTest, Destructor_QuerySubmitted_False) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    // No query submitted.
  }

  histogram_tester_.ExpectUniqueSample("Autofill.AtMemory.QuerySubmitted",
                                       false, 1);
}

// Tests that the destructor correctly logs that a suggestion was accepted.
TEST_F(AtMemoryMetricsRecorderTest, Destructor_SuggestionAccepted_True) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"query");
    SendResponse(metrics);
    metrics.OnSuggestionAccepted(
        MemoryDataType::kAddressFull,
        std::to_underlying(MemoryEntrySourceType::kAutofill) |
            std::to_underlying(MemoryEntrySourceType::kGmail));
  }

  histogram_tester_.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.AcceptedSuggestionDataType",
      MemoryDataType::kAddressFull, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.AcceptedSuggestionDataSources",
      std::to_underlying(MemoryEntrySourceType::kAutofill) |
          std::to_underlying(MemoryEntrySourceType::kGmail),
      1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.SuggestionAcceptedInSession", true, 1);
}

// Tests that the metric is NOT logged if no query was submitted.
TEST_F(AtMemoryMetricsRecorderTest,
       Destructor_NoQuerySubmitted_NoSuggestionAcceptedMetric) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    // No query submitted, no suggestion accepted.
  }

  histogram_tester_.ExpectTotalCount("Autofill.AtMemory.SuggestionAccepted", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.AtMemory.AcceptedSuggestionDataType", 0);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.SuggestionAcceptedInSession", false, 1);
}

// Tests that the destructor correctly logs that no suggestion was accepted
// if a query was submitted.
TEST_F(AtMemoryMetricsRecorderTest,
       Destructor_QuerySubmitted_SuggestionAccepted_False) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"query");
    SendResponse(metrics);
    // No suggestion accepted.
  }

  histogram_tester_.ExpectUniqueSample("Autofill.AtMemory.SuggestionAccepted",
                                       false, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.SuggestionAcceptedInSession", false, 1);
  histogram_tester_.ExpectTotalCount(
      "Autofill.AtMemory.AcceptedSuggestionDataType", 0);
}

// Tests that suggestion accepted metric is logged for multiple queries.
// Specifically, a query that is not accepted, followed by a query that is
// accepted.
TEST_F(AtMemoryMetricsRecorderTest,
       MultipleQueries_SuggestionAccepted_MultipleEmissions) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});

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
  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.AcceptedSuggestionDataType",
      MemoryDataType::kAddressFull, 1);
}

// Tests that AcceptedSuggestionDataType logs the correct memory data type when
// a suggestion is accepted.
TEST_F(AtMemoryMetricsRecorderTest, AcceptedSuggestionDataType) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"query");
    SendResponse(metrics);
    metrics.OnSuggestionAccepted(MemoryDataType::kPassportNumber);
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.AcceptedSuggestionDataType",
      MemoryDataType::kPassportNumber, 1);
}

// Tests that QueryCountBeforeAcceptance logs 1 if only one query was submitted
// before acceptance.
TEST_F(AtMemoryMetricsRecorderTest, QueryCountBeforeAcceptance_OneQuery) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"query 1");
    SendResponse(metrics);
    metrics.OnSuggestionAccepted(MemoryDataType::kAddressFull);
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.QueryCountBeforeAcceptance", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.AcceptedSuggestionDataType",
      MemoryDataType::kAddressFull, 1);
}

// Tests that QueryCountBeforeAcceptance logs the correct count if multiple
// queries were submitted before acceptance.
TEST_F(AtMemoryMetricsRecorderTest,
       QueryCountBeforeAcceptance_MultipleQueries) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"query 1");
    SendResponse(metrics);
    metrics.OnQuerySubmitted(u"query 2");
    SendResponse(metrics);
    metrics.OnSuggestionAccepted(MemoryDataType::kAddressFull);
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.QueryCountBeforeAcceptance", 2, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.AcceptedSuggestionDataType",
      MemoryDataType::kAddressFull, 1);
}

// Tests that QueryCountBeforeAcceptance is not logged if no suggestion was
// accepted.
TEST_F(AtMemoryMetricsRecorderTest, QueryCountBeforeAcceptance_NoAcceptance) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"query 1");
    SendResponse(metrics);
    metrics.OnQuerySubmitted(u"query 2");
    SendResponse(metrics);
    // No suggestion accepted.
  }

  histogram_tester_.ExpectTotalCount(
      "Autofill.AtMemory.QueryCountBeforeAcceptance", 0);
  histogram_tester_.ExpectTotalCount(
      "Autofill.AtMemory.AcceptedSuggestionDataType", 0);
}

// Tests that `MarkFilled` correctly logs whether a suggestion was filled.
TEST_F(AtMemoryMetricsRecorderTest, MarkFilled_Filled) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnSuggestionAccepted(MemoryDataType::kAddressFull);
    metrics.MarkFilled();
  }

  histogram_tester_.ExpectUniqueSample("Autofill.AtMemory.SuggestionFilled",
                                       true, 1);

  {
    AtMemoryMetricsRecorder metrics2(
        nullptr, &test_ukm_recorder_, kTestSourceId, GURL(), std::u16string(),
        FieldGlobalId(), FormSignature(0), FieldSignature(0));
    metrics2.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics2.OnSuggestionAccepted(MemoryDataType::kAddressFull);
  }

  histogram_tester_.ExpectBucketCount("Autofill.AtMemory.SuggestionFilled",
                                      false, 1);
}

// Tests that query latency metric is logged for a single result category.
TEST_F(AtMemoryMetricsRecorderTest, QueryLatency_CategorySingleType) {
  AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                  GURL(), std::u16string(), FieldGlobalId(),
                                  FormSignature(0), FieldSignature(0));
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryContextMenu,
                       /*metadata=*/{});
  metrics.OnQuerySubmitted(u"query");
  task_environment_.FastForwardBy(base::Seconds(3));
  metrics.OnQueryResponseReceived(
      MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                          {MemorySearchResult(MemoryDataType::kPassportNumber,
                                              u"Passport", u"A1234567")}));

  histogram_tester_.ExpectUniqueTimeSample("Autofill.AtMemory.Latency.Query",
                                           base::Seconds(3), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Autofill.AtMemory.Latency.Query.Passport", base::Seconds(3), 1);
}

// Tests that query latency metric is logged under "Empty" when results are
// empty.
TEST_F(AtMemoryMetricsRecorderTest, QueryLatency_CategoryEmpty) {
  AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                  GURL(), std::u16string(), FieldGlobalId(),
                                  FormSignature(0), FieldSignature(0));
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryContextMenu,
                       /*metadata=*/{});
  metrics.OnQuerySubmitted(u"query");
  task_environment_.FastForwardBy(base::Seconds(1));
  metrics.OnQueryResponseReceived(
      MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess, {}));

  histogram_tester_.ExpectUniqueTimeSample("Autofill.AtMemory.Latency.Query",
                                           base::Seconds(1), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Autofill.AtMemory.Latency.Query.Empty", base::Seconds(1), 1);
}

// Tests that query latency metric is logged under "MultipleTypes" when results
// span multiple categories.
TEST_F(AtMemoryMetricsRecorderTest, QueryLatency_CategoryMultipleTypes) {
  AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                  GURL(), std::u16string(), FieldGlobalId(),
                                  FormSignature(0), FieldSignature(0));
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
                       /*metadata=*/{});
  metrics.OnQuerySubmitted(u"query");
  task_environment_.FastForwardBy(base::Seconds(2));
  metrics.OnQueryResponseReceived(MemorySearchResults(
      MemorySearchStatus::kFinalResponseSuccess,
      {MemorySearchResult(MemoryDataType::kAddressFull, u"Address", u"Main St"),
       MemorySearchResult(MemoryDataType::kPassportNumber, u"Passport",
                          u"A1234567")}));

  histogram_tester_.ExpectUniqueTimeSample("Autofill.AtMemory.Latency.Query",
                                           base::Seconds(2), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Autofill.AtMemory.Latency.Query.MultipleTypes", base::Seconds(2), 1);
}

// Tests that query latency metric is logged under "Unknown" for unknown data
// types.
TEST_F(AtMemoryMetricsRecorderTest, QueryLatency_CategoryUnknown) {
  AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                  GURL(), std::u16string(), FieldGlobalId(),
                                  FormSignature(0), FieldSignature(0));
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
                       /*metadata=*/{});
  metrics.OnQuerySubmitted(u"query");
  task_environment_.FastForwardBy(base::Seconds(4));
  metrics.OnQueryResponseReceived(MemorySearchResults(
      MemorySearchStatus::kFinalResponseSuccess,
      {MemorySearchResult(MemoryDataType::kUnknown, u"Unknown", u"Value")}));

  histogram_tester_.ExpectUniqueTimeSample("Autofill.AtMemory.Latency.Query",
                                           base::Seconds(4), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Autofill.AtMemory.Latency.Query.Unknown", base::Seconds(4), 1);
}

// Tests that Autofill-sourced results are excluded when logging query latency.
TEST_F(AtMemoryMetricsRecorderTest,
       QueryLatency_CategoryExcludesAutofillSourced) {
  AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                  GURL(), std::u16string(), FieldGlobalId(),
                                  FormSignature(0), FieldSignature(0));
  metrics.OnPopupShown(AutofillSuggestionTriggerSource::kAtMemoryContextMenu,
                       /*metadata=*/{});
  metrics.OnQuerySubmitted(u"query");
  task_environment_.FastForwardBy(base::Seconds(5));

  MemorySearchResult autofill_entry(MemoryDataType::kAddressFull, u"Address",
                                    u"123 Main St");
  autofill_entry.sources.emplace_back(MemoryEntrySourceType::kAutofill);

  MemorySearchResult passport_entry(MemoryDataType::kPassportNumber,
                                    u"Passport", u"A1234567");
  passport_entry.sources.emplace_back(MemoryEntrySourceType::kGmail);

  metrics.OnQueryResponseReceived(MemorySearchResults(
      MemorySearchStatus::kFinalResponseSuccess,
      {std::move(autofill_entry), std::move(passport_entry)}));

  histogram_tester_.ExpectUniqueTimeSample("Autofill.AtMemory.Latency.Query",
                                           base::Seconds(5), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Autofill.AtMemory.Latency.Query.Passport", base::Seconds(5), 1);
}

struct FetchPiiLatencyTestCase {
  AtMemoryMetricsRecorder::FetchPiiSource source;
  std::string_view histogram_name;
};

class AtMemoryMetricsRecorderFetchPiiLatencyTest
    : public AtMemoryMetricsRecorderTest,
      public ::testing::WithParamInterface<FetchPiiLatencyTestCase> {};

// Tests that the unmasking duration metric is recorded correctly for all
// sources.
TEST_P(AtMemoryMetricsRecorderFetchPiiLatencyTest, FetchPiiLatency) {
  const FetchPiiLatencyTestCase& test_case = GetParam();
  base::HistogramTester histogram_tester;
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnSuggestionAccepted(MemoryDataType::kAddressFull);
    metrics.OnFetchPiiStarted(test_case.source);
    task_environment_.FastForwardBy(base::Seconds(2));
    metrics.OnFetchPiiCompleted();
    metrics.MarkFilled();
  }

  histogram_tester.ExpectUniqueTimeSample(test_case.histogram_name,
                                          base::Seconds(2), 1);
}

INSTANTIATE_TEST_SUITE_P(
    AtMemoryMetricsRecorderTest,
    AtMemoryMetricsRecorderFetchPiiLatencyTest,
    Values(
        FetchPiiLatencyTestCase{
            AtMemoryMetricsRecorder::FetchPiiSource::kAutofillAi,
            "Autofill.AtMemory.Latency.FetchPii.AutofillAi"},
        FetchPiiLatencyTestCase{
            AtMemoryMetricsRecorder::FetchPiiSource::kCreditCard,
            "Autofill.AtMemory.Latency.FetchPii.CreditCard"},
        FetchPiiLatencyTestCase{AtMemoryMetricsRecorder::FetchPiiSource::kIban,
                                "Autofill.AtMemory.Latency.FetchPii.Iban"},
        FetchPiiLatencyTestCase{
            AtMemoryMetricsRecorder::FetchPiiSource::kPersonalContext,
            "Autofill.AtMemory.Latency.FetchPii.PersonalContext"}));

// Tests that the ModelQualityLogEntry is correctly filled and uploaded when the
// uploader service is available and is flushed on destruction.
TEST_F(AtMemoryMetricsRecorderTest, LogEntryUploaded) {
  base::HistogramTester histogram_tester;

  {
    AtMemoryMetricsRecorder metrics(
        uploader_service_.get(), &test_ukm_recorder_, kTestSourceId,
        GURL("https://example.com"), u"Example Page", FieldGlobalId(),
        FormSignature(123), FieldSignature(456));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
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
    MemorySearchResults results(MemorySearchStatus::kFinalResponseSuccess,
                                {local_suggestion, remote_suggestion});
    results.server_request_id = "server_request_id";
    metrics.OnQueryResponseReceived(results);
  }

  const auto& uploaded_logs = uploader_service_->uploaded_logs();
  ASSERT_EQ(uploaded_logs.size(), 1u);
  EXPECT_EQ(uploaded_logs[0]->model_execution_info().execution_id(),
            "server_request_id");
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
  std::string quality1_session_id;
  {
    AtMemoryMetricsRecorder metrics(
        uploader_service_.get(), &test_ukm_recorder_, kTestSourceId,
        GURL("https://example.com"), u"Example Page", FieldGlobalId(),
        FormSignature(123), FieldSignature(456));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"test query");

    // The first query should be pending, not uploaded yet.
    EXPECT_TRUE(uploader_service_->uploaded_logs().empty());

    // Submitting a new query should flush the first query.
    metrics.OnQuerySubmitted(u"next query");
    ASSERT_EQ(uploader_service_->uploaded_logs().size(), 1u);
    const optimization_guide::proto::AtMemoryQuality& quality1 =
        uploader_service_->uploaded_logs()[0]->at_memory().quality();
    quality1_session_id = quality1.session_id();
    EXPECT_EQ(quality1.query(), "test query");
    EXPECT_EQ(quality1.url(), "https://example.com/");
    EXPECT_EQ(quality1.title(), "Example Page");
    EXPECT_EQ(quality1.form_signature(), 123u);
    EXPECT_EQ(quality1.field_signature(), 456u);
    EXPECT_FALSE(quality1.session_id().empty());
  }

  // The second query is flushed on destruction of the object.
  const auto& uploaded_logs = uploader_service_->uploaded_logs();
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

// Tests that the ModelQualityLogEntry is correctly filled with the action
// for a root suggestion acceptance.
TEST_F(AtMemoryMetricsRecorderTest, LogEntryUploaded_SuggestionAccepted_Root) {
  {
    AtMemoryMetricsRecorder metrics(
        uploader_service_.get(), &test_ukm_recorder_, kTestSourceId,
        GURL("https://example.com"), u"Example Page", FieldGlobalId(),
        FormSignature(123), FieldSignature(456));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"test query");

    MemorySearchResult local_suggestion(MemoryDataType::kAddressFull, u"key 1",
                                        u"value1");
    local_suggestion.sources.push_back(
        MemoryEntrySource(MemoryEntrySourceType::kAutofill));
    metrics.OnQueryResponseReceived(MemorySearchResults(
        MemorySearchStatus::kFinalResponseSuccess, {local_suggestion}));

    metrics.OnSuggestionAccepted(
        MemoryDataType::kAddressFull, /*sources_bitmask=*/0,
        AutofillSuggestionDelegate::SuggestionMetadata{.multi_index = {0}});
  }

  const auto& uploaded_logs = uploader_service_->uploaded_logs();
  ASSERT_EQ(uploaded_logs.size(), 1u);
  const optimization_guide::proto::AtMemoryQuality& quality =
      uploaded_logs[0]->at_memory().quality();

  EXPECT_THAT(
      quality.suggestions(),
      ElementsAre(HasAction(
          optimization_guide::proto::AT_MEMORY_SUGGESTION_ACTION_ACCEPTED)));
}

// Tests that the ModelQualityLogEntry is correctly filled with the action
// for a sub-suggestion (flyout menu) acceptance.
TEST_F(AtMemoryMetricsRecorderTest, LogEntryUploaded_SuggestionAccepted_Sub) {
  {
    AtMemoryMetricsRecorder metrics(
        uploader_service_.get(), &test_ukm_recorder_, kTestSourceId,
        GURL("https://example.com"), u"Example Page", FieldGlobalId(),
        FormSignature(123), FieldSignature(456));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"test query");

    MemorySearchResult local_suggestion(MemoryDataType::kAddressFull, u"key 1",
                                        u"value1");
    local_suggestion.sources.push_back(
        MemoryEntrySource(MemoryEntrySourceType::kAutofill));
    metrics.OnQueryResponseReceived(MemorySearchResults(
        MemorySearchStatus::kFinalResponseSuccess, {local_suggestion}));

    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        AutofillSuggestionDelegate::SuggestionUiMetadata{.multi_index = {0}});

    metrics.OnSuggestionAccepted(
        MemoryDataType::kAddressFull, /*sources_bitmask=*/0,
        AutofillSuggestionDelegate::SuggestionMetadata{.multi_index = {0, 1}});
  }

  const auto& uploaded_logs = uploader_service_->uploaded_logs();
  ASSERT_EQ(uploaded_logs.size(), 1u);
  const optimization_guide::proto::AtMemoryQuality& quality =
      uploaded_logs[0]->at_memory().quality();

  // The action should be set to the flyout menu attribute accepted action.
  EXPECT_THAT(
      quality.suggestions(),
      ElementsAre(HasAction(
          optimization_guide::proto::
              AT_MEMORY_SUGGESTION_ACTION_FLYOUT_MENU_ATTRIBUTE_ACCEPTED)));
}

// Tests that the ModelQualityLogEntry is correctly filled with the action
// for a sub-suggestion (flyout menu) being opened for a suggestion but not
// accepted.
TEST_F(AtMemoryMetricsRecorderTest, LogEntryUploaded_PopupShown) {
  {
    AtMemoryMetricsRecorder metrics(
        uploader_service_.get(), &test_ukm_recorder_, kTestSourceId,
        GURL("https://example.com"), u"Example Page", FieldGlobalId(),
        FormSignature(123), FieldSignature(456));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"test query");

    MemorySearchResult local_suggestion(MemoryDataType::kAddressFull, u"key 1",
                                        u"value1");
    local_suggestion.sources.push_back(
        MemoryEntrySource(MemoryEntrySourceType::kAutofill));
    metrics.OnQueryResponseReceived(MemorySearchResults(
        MemorySearchStatus::kFinalResponseSuccess, {local_suggestion}));

    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        AutofillSuggestionDelegate::SuggestionUiMetadata{.multi_index = {0}});
  }

  const auto& uploaded_logs = uploader_service_->uploaded_logs();
  ASSERT_EQ(uploaded_logs.size(), 1u);
  const optimization_guide::proto::AtMemoryQuality& quality =
      uploaded_logs[0]->at_memory().quality();

  // The action should be set to the flyout menu attribute accepted action.
  EXPECT_THAT(quality.suggestions(),
              ElementsAre(HasAction(
                  optimization_guide::proto::
                      AT_MEMORY_SUGGESTION_ACTION_FLYOUT_MENU_OPENED)));
}

// Tests that `OnSuggestionAccepted` correctly logs the accepted suggestion
// indices to UMA.
TEST_F(AtMemoryMetricsRecorderTest, OnSuggestionAccepted_LogsIndices) {
  // Test case 1: Accept root suggestion (index 2).
  {
    base::HistogramTester histogram_tester;
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnSuggestionAccepted(
        MemoryDataType::kAddressFull, /*sources_bitmask=*/0,
        AutofillSuggestionDelegate::SuggestionMetadata{.multi_index = {2}});
    histogram_tester.ExpectUniqueSample(
        "Autofill.AtMemory.AcceptedSuggestionIndex", 2, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.AtMemory.AcceptedSuggestionSecondaryIndex", -1, 1);
  }

  // Test case 2: Accept sub-suggestion (parent index 2, child index 1).
  {
    base::HistogramTester histogram_tester;
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnSuggestionAccepted(
        MemoryDataType::kAddressFull, /*sources_bitmask=*/0,
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
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"query");

    // Simulate empty response.
    metrics.OnQueryResponseReceived(
        MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess, {}));
  }

  histogram_tester_.ExpectTotalCount("Autofill.AtMemory.SuggestionAccepted", 0);
}

TEST_F(AtMemoryMetricsRecorderTest, LogsUiSessionUkm) {
  FieldGlobalId field_id = test::MakeFieldGlobalId();
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), field_id,
                                    FormSignature(1), FieldSignature(2));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"query");
    metrics.MarkFilled();
  }

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::AtMemory_UiSession::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0],
      ukm::builders::AtMemory_UiSession::kFieldSessionIdentifierName,
      autofill_metrics::FieldGlobalIdToHash64Bit(field_id));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], ukm::builders::AtMemory_UiSession::kFormSignatureName,
      HashFormSignature(FormSignature(1)));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], ukm::builders::AtMemory_UiSession::kFieldSignatureName,
      HashFieldSignature(FieldSignature(2)));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], ukm::builders::AtMemory_UiSession::kQuerySubmittedName, 1);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], ukm::builders::AtMemory_UiSession::kSuggestionFilledName, 1);
}

// Tests that AtMemory.SearchQuery UKM is logged correctly when no suggestion is
// accepted.
TEST_F(AtMemoryMetricsRecorderTest, LogsSearchQueryUkm_NoAcceptance) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"query");

    task_environment_.FastForwardBy(base::Milliseconds(100));
    metrics.OnQueryResponseReceived(
        MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                            {MemorySearchResult(MemoryDataType::kAddressFull,
                                                u"Address", u"123 Main St")}));
  }

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::AtMemory_SearchQuery::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], ukm::builders::AtMemory_SearchQuery::kQueryLatencyMsName,
      100);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], ukm::builders::AtMemory_SearchQuery::kQueryCompletedName,
      std::to_underlying(AtMemoryQueryCompletedStatus::kQueryReturnedData));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], ukm::builders::AtMemory_SearchQuery::kSuggestionAcceptedName,
      0);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], ukm::builders::AtMemory_SearchQuery::kUiSessionOrderName, 0);
}

// Tests that AtMemory.SearchQuery UKM is logged with correct acceptance and
// fill metrics.
TEST_F(AtMemoryMetricsRecorderTest, LogsSearchQueryUkm_WithAcceptanceAndFill) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"query");
    metrics.OnQueryResponseReceived(
        MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                            {MemorySearchResult(MemoryDataType::kAddressFull,
                                                u"Address", u"123 Main St")}));
    metrics.OnSuggestionAccepted(
        MemoryDataType::kAddressFull,
        std::to_underlying(MemoryEntrySourceType::kAutofill) |
            std::to_underlying(MemoryEntrySourceType::kGmail),
        AutofillSuggestionDelegate::SuggestionMetadata{.multi_index = {2}});
    metrics.MarkFilled();
  }

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::AtMemory_SearchQuery::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], ukm::builders::AtMemory_SearchQuery::kSuggestionAcceptedName,
      1);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], ukm::builders::AtMemory_SearchQuery::kSuggestionFilledName,
      1);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0],
      ukm::builders::AtMemory_SearchQuery::kAcceptedSuggestionIndexName, 2);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0],
      ukm::builders::AtMemory_SearchQuery::
          kAcceptedSuggestionSecondaryIndexName,
      -1);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0],
      ukm::builders::AtMemory_SearchQuery::kAcceptedSuggestionDataTypeName,
      std::to_underlying(MemoryDataType::kAddressFull));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0],
      ukm::builders::AtMemory_SearchQuery::kAcceptedSuggestionDataSourcesName,
      std::to_underlying(MemoryEntrySourceType::kAutofill) |
          std::to_underlying(MemoryEntrySourceType::kGmail));
}

// Tests that AtMemory.SearchQuery UKM is logged for each query when multiple
// queries are submitted in a session.
TEST_F(AtMemoryMetricsRecorderTest, LogsSearchQueryUkm_MultipleQueries) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});

    // 1st query
    metrics.OnQuerySubmitted(u"query 1");
    task_environment_.FastForwardBy(base::Milliseconds(100));
    metrics.OnQueryResponseReceived(
        MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                            {MemorySearchResult(MemoryDataType::kAddressFull,
                                                u"Address", u"123 Main St")}));

    // 2nd query - should flush 1st query
    metrics.OnQuerySubmitted(u"query 2");

    // Verify 1st query is logged
    auto entries = test_ukm_recorder_.GetEntriesByName(
        ukm::builders::AtMemory_SearchQuery::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    test_ukm_recorder_.ExpectEntryMetric(
        entries[0], ukm::builders::AtMemory_SearchQuery::kUiSessionOrderName,
        0);
    test_ukm_recorder_.ExpectEntryMetric(
        entries[0], ukm::builders::AtMemory_SearchQuery::kQueryLatencyMsName,
        100);

    // Respond to 2nd query
    task_environment_.FastForwardBy(base::Milliseconds(200));
    metrics.OnQueryResponseReceived(
        MemorySearchResults(MemorySearchStatus::kFinalResponseSuccess,
                            {MemorySearchResult(MemoryDataType::kAddressFull,
                                                u"Address", u"123 Main St")}));
    metrics.OnSuggestionAccepted(
        MemoryDataType::kAddressFull, /*sources_bitmask=*/0,
        AutofillSuggestionDelegate::SuggestionMetadata{.multi_index = {0}});
  }

  // Destructor should flush 2nd query
  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::AtMemory_SearchQuery::kEntryName);
  ASSERT_EQ(entries.size(), 2u);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[1], ukm::builders::AtMemory_SearchQuery::kUiSessionOrderName, 1);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[1], ukm::builders::AtMemory_SearchQuery::kQueryLatencyMsName,
      200);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[1], ukm::builders::AtMemory_SearchQuery::kSuggestionAcceptedName,
      1);
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
  AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                  GURL(), std::u16string(), FieldGlobalId(),
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
    Values(
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

// Tests that closing the popup without typing any query emits
// kDismissedBeforeQuery.
TEST_F(AtMemoryMetricsRecorderTest, UiSessionOutcome_DismissedBeforeQuery) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.UiSessionOutcome",
      AtMemoryUiSessionOutcome::kDismissedBeforeQuery, 1);
}

// Tests that closing the popup after submitting a query but before receiving
// results emits kDismissedBeforeResults and logs the latency wait time.
TEST_F(AtMemoryMetricsRecorderTest, UiSessionOutcome_DismissedBeforeResults) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"query");
    task_environment_.FastForwardBy(base::Seconds(2));
    // Destructor called before SendResponse(metrics).
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.UiSessionOutcome",
      AtMemoryUiSessionOutcome::kDismissedBeforeResults, 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Autofill.AtMemory.Latency.DismissedBeforeResults", base::Seconds(2), 1);
}

// Tests that closing the popup after receiving search results without
// accepting a suggestion emits kDismissedResultsBeforeAcceptance.
TEST_F(AtMemoryMetricsRecorderTest,
       UiSessionOutcome_DismissedResultsBeforeAcceptance) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"query");
    SendResponse(metrics);
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.UiSessionOutcome",
      AtMemoryUiSessionOutcome::kDismissedResultsBeforeAcceptance, 1);
}

// Tests that accepting a suggestion without filling emits
// kSuggestionAcceptedNotFilled.
TEST_F(AtMemoryMetricsRecorderTest,
       UiSessionOutcome_SuggestionAcceptedNotFilled) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"query");
    SendResponse(metrics);
    metrics.OnSuggestionAccepted(MemoryDataType::kAddressFull);
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.UiSessionOutcome",
      AtMemoryUiSessionOutcome::kSuggestionAcceptedNotFilled, 1);
}

// Tests that accepting and filling a suggestion emits kSuggestionFilled.
TEST_F(AtMemoryMetricsRecorderTest, UiSessionOutcome_SuggestionFilled) {
  {
    AtMemoryMetricsRecorder metrics(nullptr, &test_ukm_recorder_, kTestSourceId,
                                    GURL(), std::u16string(), FieldGlobalId(),
                                    FormSignature(0), FieldSignature(0));
    metrics.OnPopupShown(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        /*metadata=*/{});
    metrics.OnQuerySubmitted(u"query");
    SendResponse(metrics);
    metrics.OnSuggestionAccepted(MemoryDataType::kAddressFull);
    metrics.MarkFilled();
  }

  histogram_tester_.ExpectUniqueSample(
      "Autofill.AtMemory.UiSessionOutcome",
      AtMemoryUiSessionOutcome::kSuggestionFilled, 1);
}

}  // namespace

}  // namespace autofill
