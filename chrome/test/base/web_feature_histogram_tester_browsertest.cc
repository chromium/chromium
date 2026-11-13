// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_feature_histogram_tester.h"

#include <map>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace {

using blink::mojom::WebFeature;

constexpr char kFeatureHistogramName[] = "Blink.UseCounter.Features";
constexpr WebFeature kFeatureA =
    WebFeature::kAddressSpacePublicSecureContextNavigatedToLoopbackV2;
constexpr WebFeature kFeatureB =
    WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocalV2;

// Matches the `LinearHistogram` parameters used for `Blink.UseCounter.Features`
// by `UseCounterMetricsRecorder`.
void RecordFeature(WebFeature feature) {
  constexpr int kExclusiveMax = static_cast<int>(WebFeature::kMaxValue) + 1;
  base::LinearHistogram::FactoryGet(
      kFeatureHistogramName, 1, kExclusiveMax,
      static_cast<size_t>(kExclusiveMax + 1),
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(static_cast<int>(feature));
}

using WebFeatureHistogramTesterBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(WebFeatureHistogramTesterBrowserTest,
                       WaitForCountsAtLeastReturnsMatchingCounts) {
  WebFeatureHistogramTester tester;

  base::ThreadPool::PostDelayedTask(FROM_HERE,
                                    base::BindOnce(&RecordFeature, kFeatureA),
                                    base::Milliseconds(10));

  const std::map<WebFeature, int> expected = {
      {kFeatureA, 1},
      {kFeatureB, 0},
  };
  EXPECT_EQ(tester.WaitForCountsAtLeast(expected), expected);
  tester.ExpectCounts(expected);
}

IN_PROC_BROWSER_TEST_F(WebFeatureHistogramTesterBrowserTest,
                       WaitForCountsAtLeastReturnsObservedOverCount) {
  WebFeatureHistogramTester tester;

  RecordFeature(kFeatureA);
  RecordFeature(kFeatureA);

  const std::map<WebFeature, int> expected = {{kFeatureA, 1}};
  const std::map<WebFeature, int> actual = {{kFeatureA, 2}};
  EXPECT_EQ(tester.WaitForCountsAtLeast(expected), actual);
  EXPECT_NONFATAL_FAILURE(tester.ExpectCounts(expected),
                          "WaitForCountsAtLeast(expected)");
}

IN_PROC_BROWSER_TEST_F(WebFeatureHistogramTesterBrowserTest,
                       WaitForCountsAtLeastReturnsObservedUnexpectedFeature) {
  WebFeatureHistogramTester tester;

  // Record `kFeatureB` (expected 0) while leaving `kFeatureA` (expected 1) at
  // 0. `WaitForCountsAtLeast()` stops waiting once any observed count exceeds
  // its expected count and must return the observed counts rather than
  // `expected`.
  RecordFeature(kFeatureB);

  const std::map<WebFeature, int> expected = {
      {kFeatureA, 1},
      {kFeatureB, 0},
  };
  const std::map<WebFeature, int> actual = {
      {kFeatureA, 0},
      {kFeatureB, 1},
  };
  EXPECT_EQ(tester.WaitForCountsAtLeast(expected), actual);
  EXPECT_NONFATAL_FAILURE(tester.ExpectCounts(expected),
                          "WaitForCountsAtLeast(expected)");
}

}  // namespace
