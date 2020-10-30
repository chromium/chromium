// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace shared_highlighting {

namespace {

TEST(SharedHighlightingMetricsTest, LogTextFragmentAmbiguousMatch) {
  base::HistogramTester histogram_tester;

  LogTextFragmentAmbiguousMatch(true);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.AmbiguousMatch", 1, 1);

  LogTextFragmentAmbiguousMatch(false);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.AmbiguousMatch", 0, 1);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.AmbiguousMatch", 2);
}

TEST(SharedHighlightingMetricsTest, LogTextFragmentLinkOpenSource) {
  base::HistogramTester histogram_tester;

  GURL search_engine_url("https://google.com");
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

TEST(SharedHighlightingMetricsTest, LogTextFragmentMatchRate) {
  base::HistogramTester histogram_tester;

  LogTextFragmentMatchRate(/*matches=*/2, /*nb_selectors=*/2);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.MatchRate", 100, 1);

  LogTextFragmentMatchRate(/*matches=*/1, /*nb_selectors=*/2);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.MatchRate", 50, 1);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.MatchRate", 2);
}

TEST(SharedHighlightingMetricsTest, LogTextFragmentSelectorCount) {
  base::HistogramTester histogram_tester;

  LogTextFragmentSelectorCount(1);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.SelectorCount", 1, 1);

  LogTextFragmentSelectorCount(20);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.SelectorCount", 20, 1);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.SelectorCount", 2);
}

TEST(SharedHighlightingMetricsTest, LogLinkGenerationStatus) {
  base::HistogramTester histogram_tester;

  LogLinkGenerationStatus(true);
  histogram_tester.ExpectUniqueSample("SharedHighlights.LinkGenerated", true,
                                      1);

  LogLinkGenerationStatus(false);
  histogram_tester.ExpectBucketCount("SharedHighlights.LinkGenerated", false,
                                     1);
  histogram_tester.ExpectTotalCount("SharedHighlights.LinkGenerated", 2);
}

TEST(SharedHighlightingMetricsTest, LogLinkGenerationErrorReason) {
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

TEST(SharedHighlightingMetricsTest, LogAndroidLinkGenerationErrorReason) {
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

}  // namespace

}  // namespace shared_highlighting
