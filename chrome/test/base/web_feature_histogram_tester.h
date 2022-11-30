// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_WEB_FEATURE_HISTOGRAM_TESTER_H_
#define CHROME_TEST_BASE_WEB_FEATURE_HISTOGRAM_TESTER_H_

#include <map>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

// Returns a map containing the given `features` as keys, all mapping to zero.
std::map<blink::mojom::WebFeature, int> AllZeroFeatureCounts(
    const std::vector<blink::mojom::WebFeature>& features);

// Builds a new map by adding the value in `lhs` and `rhs` key-wise.
//
// Useful in conjunction with `AllZeroFeatureCounts()`, when building a map of
// mostly zero values with a few exceptions.
std::map<blink::mojom::WebFeature, int> AddFeatureCounts(
    const std::map<blink::mojom::WebFeature, int>& lhs,
    const std::map<blink::mojom::WebFeature, int>& rhs);

// Helper for testing `WebFeature` use counts recorded as UMA histograms.
//
// Wraps around `base::HistogramTester` and provides a simpler API centered
// around `WebFeature`s. Similarly to `base::HistogramTester`, the counts
// returned by this instance are relative diffs compared to the histogram
// values at construction time.
//
// This class publicly depends solely on content/ code, yet it is defined in
// chrome/ on purpose. Indeed, `WebFeature` use counts are recorded in UMA
// histograms by components/page_load_metrics, which sits above content/. While
// this code may compile in content/, content/ tests written against it would
// observe zero counts that never increase, because no code would be recording
// the feature use counts in histograms.
class WebFeatureHistogramTester final {
 public:
  // Takes a snapshot of counts.
  WebFeatureHistogramTester();

  WebFeatureHistogramTester(const WebFeatureHistogramTester&) = delete;
  WebFeatureHistogramTester& operator=(const WebFeatureHistogramTester&) =
      delete;

  ~WebFeatureHistogramTester();

  // Returns the current count of the given `feature`.
  // This is a diff from the snapshot taken at construction time.
  int GetCount(blink::mojom::WebFeature feature) const;

  // Returns a map of the given `features` to their corresponding count.
  // These are diffs from the snapshot taken at construction time.
  // Duplicate features are ignored.
  std::map<blink::mojom::WebFeature, int> GetCounts(
      const std::vector<blink::mojom::WebFeature>& features) const;

  // Same as `GetCounts()`, except that entries with zero counts are skipped.
  std::map<blink::mojom::WebFeature, int> GetNonZeroCounts(
      const std::vector<blink::mojom::WebFeature>& features) const;

  // Waits for the given features' counts to reach the `expected` values, then
  // returns the last observed feature counts.
  //
  // `expected` must have only non-negative values.
  //
  // As with other methods above, `expected` is compared against diffs from
  // the snapshot taken at constructon time.
  //
  // See also `AllZeroFeatureCounts()` and `AddFeatureCounts()` for help
  // building `expected`.
  std::map<blink::mojom::WebFeature, int> WaitForCountsAtLeast(
      const std::map<blink::mojom::WebFeature, int>& expected) const;

  // Equivalent to `EXPECT_EQ(WaitForCountsAtLeast(expected), expected)`.
  void ExpectCounts(
      const std::map<blink::mojom::WebFeature, int>& expected) const;

 private:
  int GetCountInternal(blink::mojom::WebFeature feature) const;

  std::map<blink::mojom::WebFeature, int> GetCountsInternal(
      const std::vector<blink::mojom::WebFeature>& features,
      bool include_zeroes) const;

  base::HistogramTester histogram_tester_;
};

#endif  // CHROME_TEST_BASE_WEB_FEATURE_HISTOGRAM_TESTER_H_
