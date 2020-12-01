// Copyright 2020 The Chromium Authors. All rights reserved.
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

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(SharedHighlightingMetricsTest, LogTextFragmentAmbiguousMatch) {
  base::HistogramTester histogram_tester;

  LogTextFragmentAmbiguousMatch(true);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.AmbiguousMatch", 1, 1);

  LogTextFragmentAmbiguousMatch(false);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.AmbiguousMatch", 0, 1);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 2);
}

TEST_F(SharedHighlightingMetricsTest, LogTextFragmentLinkOpenSource) {
  base::HistogramTester histogram_tester;

  GURL search_engine_url(kSearchEngineUrl);
  LogTextFragmentLinkOpenSource(search_engine_url);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.LinkOpenSource",
                                     TextFragmentLinkOpenSource::kSearchEngine,
                                     1);

  GURL non_search_engine_url("https://example.com");
  LogTextFragmentLinkOpenSource(non_search_engine_url);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.LinkOpenSource",
                                     TextFragmentLinkOpenSource::kUnknown, 1);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 2);

  GURL empty_gurl("");
  LogTextFragmentLinkOpenSource(empty_gurl);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.LinkOpenSource",
                                     TextFragmentLinkOpenSource::kUnknown, 2);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 3);
}

TEST_F(SharedHighlightingMetricsTest, LogTextFragmentMatchRate) {
  base::HistogramTester histogram_tester;

  LogTextFragmentMatchRate(/*matches=*/2, /*nb_selectors=*/2);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.MatchRate", 100, 1);

  LogTextFragmentMatchRate(/*matches=*/1, /*nb_selectors=*/2);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.MatchRate", 50, 1);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.MatchRate", 2);
}

TEST_F(SharedHighlightingMetricsTest, LogTextFragmentSelectorCount) {
  base::HistogramTester histogram_tester;

  LogTextFragmentSelectorCount(1);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.SelectorCount", 1, 1);

  LogTextFragmentSelectorCount(20);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.SelectorCount", 20, 1);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 2);
}

TEST_F(SharedHighlightingMetricsTest, LogLinkGenerationStatus) {
  base::HistogramTester histogram_tester;

  LogLinkGenerationStatus(true);
  histogram_tester.ExpectUniqueSample("SharedHighlights.LinkGenerated", true,
                                      1);

  LogLinkGenerationStatus(false);
  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated", false,
                                     1);
  histogram_tester.ExpectTotalCount("SharedHighlights.LinkGenerated", 2);
}

TEST_F(SharedHighlightingMetricsTest, LogLinkGenerationErrorReason) {
  base::HistogramTester histogram_tester;

  LogLinkGenerationErrorReason(LinkGenerationError::kIncorrectSelector);
  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Error",
                                     LinkGenerationError::kIncorrectSelector,
                                     1);

  LogLinkGenerationErrorReason(LinkGenerationError::kEmptySelection);
  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Error",
                                     LinkGenerationError::kEmptySelection, 1);
  histogram_tester.ExpectTotalCount("SharedHighlights.LinkGenerated.Error", 2);
}

TEST_F(SharedHighlightingMetricsTest, LogAndroidLinkGenerationErrorReason) {
  base::HistogramTester histogram_tester;

  LogGenerateErrorTabHidden();
  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Error",
                                     LinkGenerationError::kTabHidden, 1);

  LogGenerateErrorOmniboxNavigation();
  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Error",
                                     LinkGenerationError::kOmniboxNavigation,
                                     1);

  LogGenerateErrorTabCrash();
  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated.Error",
                                     LinkGenerationError::kTabCrash, 1);
  histogram_tester.ExpectTotalCount("SharedHighlights.LinkGenerated.Error", 3);
}

TEST_F(SharedHighlightingMetricsTest, LinkOpenedUkm_Success_SearchEngine) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = 1;
  bool success = true;

  LogLinkOpenedUkmEvent(source_id, GURL(kSearchEngineUrl), success);

  ValidateLinkOpenedUkm(ukm_recorder, source_id, success,
                        TextFragmentLinkOpenSource::kSearchEngine);
}

TEST_F(SharedHighlightingMetricsTest, LinkOpenedUkm_Fail_SearchEngine) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = 1;
  bool success = false;

  LogLinkOpenedUkmEvent(source_id, GURL(kSearchEngineUrl), success);

  ValidateLinkOpenedUkm(ukm_recorder, source_id, success,
                        TextFragmentLinkOpenSource::kSearchEngine);
}

TEST_F(SharedHighlightingMetricsTest, LinkOpenedUkm_Success_UnknownSource) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = 1;
  bool success = true;

  LogLinkOpenedUkmEvent(source_id, GURL(), success);

  ValidateLinkOpenedUkm(ukm_recorder, source_id, success,
                        TextFragmentLinkOpenSource::kUnknown);
}

TEST_F(SharedHighlightingMetricsTest, LinkOpenedUkm_Fail_UnknownSource) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = 1;
  bool success = false;

  LogLinkOpenedUkmEvent(source_id, GURL(), success);

  ValidateLinkOpenedUkm(ukm_recorder, source_id, success,
                        TextFragmentLinkOpenSource::kUnknown);
}

TEST_F(SharedHighlightingMetricsTest, LinkOpenedUkm_InvalidSourceId) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  LogLinkOpenedUkmEvent(ukm::kInvalidSourceId, GURL(kSearchEngineUrl),
                        /*success=*/true);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::SharedHighlights_LinkOpened::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

// Tests that using the endpoints with a custom recorder won't use the static
// UKM recorder.
TEST_F(SharedHighlightingMetricsTest, LinkOpenedUkm_CustomRecorder) {
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

TEST_F(SharedHighlightingMetricsTest, LinkGeneratedUkm_Success) {
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

TEST_F(SharedHighlightingMetricsTest, LinkGeneratedUkm_Error) {
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

TEST_F(SharedHighlightingMetricsTest,
       LinkGeneratedUkm_Success_InvalidSourceId) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  LogLinkGeneratedSuccessUkmEvent(ukm::kInvalidSourceId);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::SharedHighlights_LinkGenerated::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

// Tests that using the endpoints with a custom recorder won't use the static
// UKM recorder.
TEST_F(SharedHighlightingMetricsTest, LinkGeneratedUkm_CustomRecorder) {
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

}  // namespace

}  // namespace shared_highlighting
