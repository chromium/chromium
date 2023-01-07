// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace shared_highlighting {

namespace {

const char kSearchEngineUrl[] = "https://google.com";

const char kSourceUkmMetric[] = "Source";
const char kSuccessUkmMetric[] = "Success";
const char kErrorUkmMetric[] = "Error";

class SharedHighlightingMetricsTest : public testing::Test {
 protected:
  SharedHighlightingMetricsTest() = default;
  ~SharedHighlightingMetricsTest() override = default;

  void ValidateLinkOpenedUkm(const ukm::TestAutoSetUkmRecorder& recorder,
                             ukm::SourceId source_id,
                             bool success,
                             TextFragmentLinkOpenSource source) {
    auto entries = recorder.GetEntriesByName(
        ukm::builders::SharedHighlights_LinkOpened::kEntryName);
    ASSERT_EQ(1u, entries.size());
    const ukm::mojom::UkmEntry* entry = entries[0];
    EXPECT_EQ(source_id, entry->source_id);
    recorder.ExpectEntryMetric(entry, kSuccessUkmMetric, success);
    recorder.ExpectEntryMetric(entry, kSourceUkmMetric,
                               static_cast<int64_t>(source));
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(SharedHighlightingMetricsTest, LogTextFragmentAmbiguousMatch) {
  LogTextFragmentAmbiguousMatch(true);
  histogram_tester_.ExpectBucketCount("TextFragmentAnchor.AmbiguousMatch", 1,
                                      1);

  LogTextFragmentAmbiguousMatch(false);
  histogram_tester_.ExpectBucketCount("TextFragmentAnchor.AmbiguousMatch", 0,
                                      1);
  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 2);
}

TEST_F(SharedHighlightingMetricsTest, LogTextFragmentLinkOpenSource) {
  GURL search_engine_url(kSearchEngineUrl);
  LogTextFragmentLinkOpenSource(search_engine_url);
  histogram_tester_.ExpectBucketCount("TextFragmentAnchor.LinkOpenSource",
                                      TextFragmentLinkOpenSource::kSearchEngine,
                                      1);

  GURL non_search_engine_url("https://example.com");
  LogTextFragmentLinkOpenSource(non_search_engine_url);
  histogram_tester_.ExpectBucketCount("TextFragmentAnchor.LinkOpenSource",
                                      TextFragmentLinkOpenSource::kUnknown, 1);
  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 2);

  GURL empty_gurl("");
  LogTextFragmentLinkOpenSource(empty_gurl);
  histogram_tester_.ExpectBucketCount("TextFragmentAnchor.LinkOpenSource",
                                      TextFragmentLinkOpenSource::kUnknown, 2);
  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 3);

  GURL google_non_search_domain("https://mail.google.com");
  LogTextFragmentLinkOpenSource(google_non_search_domain);
  histogram_tester_.ExpectBucketCount("TextFragmentAnchor.LinkOpenSource",
                                      TextFragmentLinkOpenSource::kUnknown, 3);
  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 4);
}

TEST_F(SharedHighlightingMetricsTest, LogTextFragmentMatchRate) {
  LogTextFragmentMatchRate(/*matches=*/2, /*text_fragments=*/2);
  histogram_tester_.ExpectBucketCount("TextFragmentAnchor.MatchRate", 100, 1);

  LogTextFragmentMatchRate(/*matches=*/1, /*text_fragments=*/2);
  histogram_tester_.ExpectBucketCount("TextFragmentAnchor.MatchRate", 50, 1);
  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.MatchRate", 2);
}

TEST_F(SharedHighlightingMetricsTest, LogTextFragmentSelectorCount) {
  LogTextFragmentSelectorCount(1);
  histogram_tester_.ExpectBucketCount("TextFragmentAnchor.SelectorCount", 1, 1);

  LogTextFragmentSelectorCount(20);
  histogram_tester_.ExpectBucketCount("TextFragmentAnchor.SelectorCount", 20,
                                      1);
  histogram_tester_.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 2);
}

TEST_F(SharedHighlightingMetricsTest, LogLinkGenerationStatus) {
  LogLinkGenerationStatus(LinkGenerationStatus::kSuccess);
  histogram_tester_.ExpectUniqueSample("SharedHighlights.LinkGenerated", true,
                                       1);

  LogLinkGenerationStatus(LinkGenerationStatus::kFailure);
  histogram_tester_.ExpectBucketCount("SharedHighlights.LinkGenerated", false,
                                      1);
  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated", 2);
}

TEST_F(SharedHighlightingMetricsTest, LogLinkGenerationErrorReason) {
  LogLinkGenerationErrorReason(LinkGenerationError::kIncorrectSelector);
  histogram_tester_.ExpectBucketCount("SharedHighlights.LinkGenerated.Error",
                                      LinkGenerationError::kIncorrectSelector,
                                      1);

  LogLinkGenerationErrorReason(LinkGenerationError::kEmptySelection);
  histogram_tester_.ExpectBucketCount("SharedHighlights.LinkGenerated.Error",
                                      LinkGenerationError::kEmptySelection, 1);
  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated.Error", 2);
}

TEST_F(SharedHighlightingMetricsTest, LinkOpenedUkmSuccessSearchEngine) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = 1;
  bool success = true;

  LogLinkOpenedUkmEvent(source_id, GURL(kSearchEngineUrl), success);

  ValidateLinkOpenedUkm(ukm_recorder, source_id, success,
                        TextFragmentLinkOpenSource::kSearchEngine);
}

TEST_F(SharedHighlightingMetricsTest, LinkOpenedUkmFailSearchEngine) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = 1;
  bool success = false;

  LogLinkOpenedUkmEvent(source_id, GURL(kSearchEngineUrl), success);

  ValidateLinkOpenedUkm(ukm_recorder, source_id, success,
                        TextFragmentLinkOpenSource::kSearchEngine);
}

TEST_F(SharedHighlightingMetricsTest, LinkOpenedUkmSuccessUnknownSource) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = 1;
  bool success = true;

  LogLinkOpenedUkmEvent(source_id, GURL(), success);

  ValidateLinkOpenedUkm(ukm_recorder, source_id, success,
                        TextFragmentLinkOpenSource::kUnknown);
}

TEST_F(SharedHighlightingMetricsTest, LinkOpenedUkmFailUnknownSource) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = 1;
  bool success = false;

  LogLinkOpenedUkmEvent(source_id, GURL(), success);

  ValidateLinkOpenedUkm(ukm_recorder, source_id, success,
                        TextFragmentLinkOpenSource::kUnknown);
}

TEST_F(SharedHighlightingMetricsTest, LinkOpenedUkmInvalidSourceId) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  LogLinkOpenedUkmEvent(ukm::kInvalidSourceId, GURL(kSearchEngineUrl),
                        /*success=*/true);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::SharedHighlights_LinkOpened::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

// Tests that using the endpoints with a custom recorder won't use the static
// UKM recorder.
TEST_F(SharedHighlightingMetricsTest, LinkOpenedUkmCustomRecorder) {
  ukm::TestAutoSetUkmRecorder static_ukm_recorder;
  ukm::TestUkmRecorder custom_ukm_recorder;
  ukm::SourceId source_id = 1;

  LogLinkOpenedUkmEvent(&custom_ukm_recorder, source_id, GURL(),
                        /*success=*/true);

  auto static_entries = static_ukm_recorder.GetEntriesByName(
      ukm::builders::SharedHighlights_LinkOpened::kEntryName);
  EXPECT_EQ(0U, static_entries.size());

  auto custom_entries = custom_ukm_recorder.GetEntriesByName(
      ukm::builders::SharedHighlights_LinkOpened::kEntryName);
  EXPECT_EQ(1U, custom_entries.size());
}

TEST_F(SharedHighlightingMetricsTest, LinkGeneratedUkmSuccess) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = 1;

  LogLinkGeneratedSuccessUkmEvent(source_id);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::SharedHighlights_LinkGenerated::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  EXPECT_EQ(source_id, entry->source_id);
  ukm_recorder.ExpectEntryMetric(entry, kSuccessUkmMetric, true);
  EXPECT_FALSE(ukm_recorder.GetEntryMetric(entry, kErrorUkmMetric));
}

TEST_F(SharedHighlightingMetricsTest, LinkGeneratedUkmError) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = 1;
  LinkGenerationError error = LinkGenerationError::kEmptySelection;

  LogLinkGeneratedErrorUkmEvent(source_id, error);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::SharedHighlights_LinkGenerated::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  EXPECT_EQ(source_id, entry->source_id);
  ukm_recorder.ExpectEntryMetric(entry, kSuccessUkmMetric, false);
  ukm_recorder.ExpectEntryMetric(entry, kErrorUkmMetric,
                                 static_cast<int64_t>(error));
}

TEST_F(SharedHighlightingMetricsTest, LinkGeneratedUkmSuccessInvalidSourceId) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  LogLinkGeneratedSuccessUkmEvent(ukm::kInvalidSourceId);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::SharedHighlights_LinkGenerated::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

// Tests that using the endpoints with a custom recorder won't use the static
// UKM recorder.
TEST_F(SharedHighlightingMetricsTest, LinkGeneratedUkmCustomRecorder) {
  ukm::TestAutoSetUkmRecorder static_ukm_recorder;
  ukm::TestUkmRecorder custom_ukm_recorder;
  ukm::SourceId source_id = 1;

  LogLinkGeneratedSuccessUkmEvent(&custom_ukm_recorder, source_id);
  LogLinkGeneratedErrorUkmEvent(&custom_ukm_recorder, source_id,
                                LinkGenerationError::kEmptySelection);

  auto static_entries = static_ukm_recorder.GetEntriesByName(
      ukm::builders::SharedHighlights_LinkGenerated::kEntryName);
  EXPECT_EQ(0U, static_entries.size());

  auto custom_entries = custom_ukm_recorder.GetEntriesByName(
      ukm::builders::SharedHighlights_LinkGenerated::kEntryName);
  EXPECT_EQ(2U, custom_entries.size());
}

// Tests that link generation success latency logs to the right histogram.
TEST_F(SharedHighlightingMetricsTest, LinkGeneratedSuccessLatency) {
  base::TimeDelta test_delta = base::Milliseconds(2000);

  LogGenerateSuccessLatency(test_delta);

  histogram_tester_.ExpectTimeBucketCount(
      "SharedHighlights.LinkGenerated.TimeToGenerate", test_delta, 1);
}

// Tests that link generation failure latency logs to the right histogram.
TEST_F(SharedHighlightingMetricsTest, LinkGeneratedErrorLatency) {
  base::TimeDelta test_delta = base::Milliseconds(2000);

  LogGenerateErrorLatency(test_delta);

  histogram_tester_.ExpectTimeBucketCount(
      "SharedHighlights.LinkGenerated.Error.TimeToGenerate", test_delta, 1);
}

// Tests all the metrics that need to be recorded in case of a failure.
TEST_F(SharedHighlightingMetricsTest, LogRequestedFailureMetrics) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = 1;
  LogRequestedFailureMetrics(source_id, LinkGenerationError::kEmptySelection);

  histogram_tester_.ExpectBucketCount(
      "SharedHighlights.LinkGenerated.Requested", false, 1);
  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated.Requested",
                                     1);
  histogram_tester_.ExpectBucketCount(
      "SharedHighlights.LinkGenerated.Error.Requested",
      LinkGenerationError::kEmptySelection, 1);
  histogram_tester_.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.Error.Requested", 1);

  // Check UKM
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::SharedHighlights_LinkGenerated_Requested::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  EXPECT_EQ(source_id, entry->source_id);
  ukm_recorder.ExpectEntryMetric(entry, kSuccessUkmMetric, false);
  ukm_recorder.ExpectEntryMetric(
      entry, kErrorUkmMetric,
      static_cast<int64_t>(LinkGenerationError::kEmptySelection));
}

// Tests all the metrics that need to be recorded in case of a success.
TEST_F(SharedHighlightingMetricsTest, LogRequestedSuccessMetrics) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = 1;
  LogRequestedSuccessMetrics(source_id);

  histogram_tester_.ExpectBucketCount(
      "SharedHighlights.LinkGenerated.Requested", true, 1);
  histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated.Requested",
                                     1);

  // Check UKM
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::SharedHighlights_LinkGenerated_Requested::kEntryName);
  ASSERT_EQ(1u, entries.size());
  const ukm::mojom::UkmEntry* entry = entries[0];
  EXPECT_EQ(source_id, entry->source_id);
  ukm_recorder.ExpectEntryMetric(entry, kSuccessUkmMetric, true);
  EXPECT_FALSE(ukm_recorder.GetEntryMetric(entry, kErrorUkmMetric));
}

// Tests that requested before or after histogram is correctly recorded.
TEST_F(SharedHighlightingMetricsTest, LogLinkRequestedBeforeStatus) {
  LogLinkRequestedBeforeStatus(
      LinkGenerationStatus::kSuccess,
      LinkGenerationReadyStatus::kRequestedBeforeReady);

  histogram_tester_.ExpectBucketCount(
      "SharedHighlights.LinkGenerated.RequestedBeforeReady", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedBeforeReady", 1);
  histogram_tester_.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedAfterReady", 0);

  LogLinkRequestedBeforeStatus(
      LinkGenerationStatus::kFailure,
      LinkGenerationReadyStatus::kRequestedBeforeReady);
  histogram_tester_.ExpectBucketCount(
      "SharedHighlights.LinkGenerated.RequestedBeforeReady", false, 1);
  histogram_tester_.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedBeforeReady", 2);
  histogram_tester_.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedAfterReady", 0);

  LogLinkRequestedBeforeStatus(LinkGenerationStatus::kSuccess,
                               LinkGenerationReadyStatus::kRequestedAfterReady);
  histogram_tester_.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedBeforeReady", 2);
  histogram_tester_.ExpectBucketCount(
      "SharedHighlights.LinkGenerated.RequestedAfterReady", true, 1);
  histogram_tester_.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedAfterReady", 1);

  LogLinkRequestedBeforeStatus(LinkGenerationStatus::kFailure,
                               LinkGenerationReadyStatus::kRequestedAfterReady);
  histogram_tester_.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedBeforeReady", 2);
  histogram_tester_.ExpectBucketCount(
      "SharedHighlights.LinkGenerated.RequestedAfterReady", false, 1);
  histogram_tester_.ExpectTotalCount(
      "SharedHighlights.LinkGenerated.RequestedAfterReady", 2);
}

// Tests that link generation failure latency logs to the right histogram.
TEST_F(SharedHighlightingMetricsTest, LogLinkToTextReshareStatus) {
  LogLinkToTextReshareStatus(LinkToTextReshareStatus::kSuccess);
  histogram_tester_.ExpectBucketCount(
      "SharedHighlights.ObtainReshareLink.Status",
      LinkToTextReshareStatus::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(
      "SharedHighlights.ObtainReshareLink.Status", 1);

  LogLinkToTextReshareStatus(LinkToTextReshareStatus::kTimeout);
  histogram_tester_.ExpectBucketCount(
      "SharedHighlights.ObtainReshareLink.Status",
      LinkToTextReshareStatus::kTimeout, 1);
  histogram_tester_.ExpectTotalCount(
      "SharedHighlights.ObtainReshareLink.Status", 2);
}

}  // namespace

}  // namespace shared_highlighting
