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

  GURL search_engine_url = GURL("https://google.com");
  GURL non_search_engine_url = GURL("https://example.com");

  LogTextFragmentLinkOpenSource(search_engine_url);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.LinkOpenSource", 1, 1);

  LogTextFragmentLinkOpenSource(non_search_engine_url);
  histogram_tester.ExpectBucketCount("TextFragmentAnchor.LinkOpenSource", 0, 1);
  histogram_tester.ExpectTotalCount("TextFragmentAnchor.LinkOpenSource", 2);
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

}  // namespace

}  // namespace shared_highlighting
