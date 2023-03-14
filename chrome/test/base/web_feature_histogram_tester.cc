// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_feature_histogram_tester.h"

#include <utility>

#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using blink::mojom::WebFeature;
using testing::_;
using testing::Each;
using testing::Ge;
using testing::Pair;

constexpr char kFeatureHistogramName[] = "Blink.UseCounter.Features";

void FetchCounts() {
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
}

// Returns the list of keys in the given map.
std::vector<WebFeature> Keys(const std::map<WebFeature, int>& counts) {
  std::vector<WebFeature> keys;
  for (const auto& entry : counts) {
    keys.push_back(entry.first);
  }
  return keys;
}

bool AllCountsAreLessThanOrEqual(const std::map<WebFeature, int>& lhs,
                                 const std::map<WebFeature, int>& rhs) {
  // Prints both sets of keys in case of mismatch, for debugging.
  EXPECT_EQ(Keys(lhs), Keys(rhs));

  for (const auto& entry : lhs) {
    WebFeature feature = entry.first;
    int lhs_count = entry.second;

    const auto it = rhs.find(feature);
    if (it == rhs.end()) {
      return false;
    }

    int rhs_count = it->second;
    if (lhs_count > rhs_count) {
      return false;
    }
  }

  return true;
}

}  // namespace

std::map<WebFeature, int> AllZeroFeatureCounts(
    const std::vector<WebFeature>& features) {
  std::map<WebFeature, int> result;
  for (WebFeature feature : features) {
    result.emplace(feature, 0);
  }
  return result;
}

std::map<WebFeature, int> AddFeatureCounts(
    const std::map<WebFeature, int>& lhs,
    const std::map<WebFeature, int>& rhs) {
  std::map<WebFeature, int> result = lhs;
  for (const auto& entry : rhs) {
    result[entry.first] += entry.second;
  }
  return result;
}

WebFeatureHistogramTester::WebFeatureHistogramTester() = default;

WebFeatureHistogramTester::~WebFeatureHistogramTester() = default;

int WebFeatureHistogramTester::GetCountInternal(WebFeature feature) const {
  return histogram_tester_.GetBucketCount(kFeatureHistogramName, feature);
}

int WebFeatureHistogramTester::GetCount(WebFeature feature) const {
  FetchCounts();
  return GetCountInternal(feature);
}

std::map<WebFeature, int> WebFeatureHistogramTester::GetCounts(
    const std::vector<WebFeature>& features) const {
  FetchCounts();
  return GetCountsInternal(features, true);
}

std::map<WebFeature, int> WebFeatureHistogramTester::GetNonZeroCounts(
    const std::vector<WebFeature>& features) const {
  FetchCounts();
  return GetCountsInternal(features, false);
}

std::map<WebFeature, int> WebFeatureHistogramTester::GetCountsInternal(
    const std::vector<WebFeature>& features,
    bool include_zeroes) const {
  std::map<WebFeature, int> counts;
  for (WebFeature feature : features) {
    int count = GetCountInternal(feature);
    if (count == 0 && !include_zeroes) {
      continue;
    }

    counts.emplace(feature, count);
  }

  return counts;
}

std::map<WebFeature, int> WebFeatureHistogramTester::WaitForCountsAtLeast(
    const std::map<WebFeature, int>& expected) const {
  EXPECT_THAT(expected, Each(Pair(_, Ge(0))))
      << "All counts must be non-negative.";

  std::vector<WebFeature> features = Keys(expected);
  std::map<WebFeature, int> counts;

  while (true) {
    counts = GetCounts(features);
    if (!AllCountsAreLessThanOrEqual(counts, expected) || counts == expected) {
      break;
    }

    base::PlatformThread::Sleep(base::Milliseconds(5));
  }

  return expected;
}

void WebFeatureHistogramTester::ExpectCounts(
    const std::map<WebFeature, int>& expected) const {
  EXPECT_EQ(WaitForCountsAtLeast(expected), expected);
}
