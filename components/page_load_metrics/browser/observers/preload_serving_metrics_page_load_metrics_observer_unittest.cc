// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/preload_serving_metrics_page_load_metrics_observer.h"

#include <optional>
#include <string>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/public/browser/preload_serving_metrics_capsule.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void ExpectFCP(const base::HistogramTester& histogram_tester,
               const std::string& suffix,
               std::optional<int> fcp) {
  const std::string name =
      base::StrCat({"PreloadServingMetrics.PageLoad.Clients.PaintTiming."
                    "NavigationToFirstContentfulPaint.",
                    suffix});
  if (fcp) {
    histogram_tester.ExpectUniqueTimeSample(name, base::Milliseconds(*fcp), 1);
  } else {
    histogram_tester.ExpectTotalCount(name, 0);
  }
}

// Verifies metrics recording for a standard navigation without preloading.
TEST(PreloadServingMetricsPageLoadMetricsObserverTest,
     NavigationWithoutPreload) {
  base::HistogramTester histogram_tester;

  page_load_metrics_internal::RecordPreloadServingMetricsByNavigationInitiator(
      content::UsedInstantLoad::kNoInstantLoad, "Other", /*is_url_srp=*/false);
  page_load_metrics_internal::RecordFirstContentfulPaint(
      base::Milliseconds(334), /*is_in_foreground=*/true,
      content::UsedInstantLoad::kNoInstantLoad, "Other", /*is_url_srp=*/false);

  ExpectFCP(histogram_tester, "WithoutPreload", {334});
  ExpectFCP(histogram_tester, "WithPrefetch", {});
  ExpectFCP(histogram_tester, "WithPrerender", {});

  ExpectFCP(histogram_tester, "All.All.All", {334});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "All.All.Prerender", {});

  ExpectFCP(histogram_tester, "Other.All.All", {334});
  ExpectFCP(histogram_tester, "Other.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "Other.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.NoInstantLoad",
            {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prerender", {});

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      0 /* kNoInstantLoad */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.Other.SRP", 0);
}

// Verifies metrics recording when navigation occurs in background.
TEST(PreloadServingMetricsPageLoadMetricsObserverTest,
     NavigationWithoutPreloadInBackground) {
  base::HistogramTester histogram_tester;

  page_load_metrics_internal::RecordPreloadServingMetricsByNavigationInitiator(
      content::UsedInstantLoad::kNoInstantLoad, "Other", /*is_url_srp=*/false);
  page_load_metrics_internal::RecordFirstContentfulPaint(
      base::Milliseconds(334), /*is_in_foreground=*/false,
      content::UsedInstantLoad::kNoInstantLoad, "Other", /*is_url_srp=*/false);

  ExpectFCP(histogram_tester, "WithoutPreload", {334});
  ExpectFCP(histogram_tester, "WithPrefetch", {});
  ExpectFCP(histogram_tester, "WithPrerender", {});

  ExpectFCP(histogram_tester, "All.All.All", {});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "All.All.Prerender", {});

  ExpectFCP(histogram_tester, "Other.All.All", {});
  ExpectFCP(histogram_tester, "Other.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "Other.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.NoInstantLoad",
            {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prerender", {});

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      0 /* kNoInstantLoad */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.Other.SRP", 0);
}

// Verifies metrics recording for BackForwardCache restore navigation.
TEST(PreloadServingMetricsPageLoadMetricsObserverTest,
     NavigationWithBFCacheRestore) {
  base::HistogramTester histogram_tester;

  page_load_metrics_internal::RecordPreloadServingMetricsByNavigationInitiator(
      content::UsedInstantLoad::kBFCache, "Backward", /*is_url_srp=*/false);
  page_load_metrics_internal::RecordFirstContentfulPaint(
      base::Milliseconds(334), /*is_in_foreground=*/true,
      content::UsedInstantLoad::kBFCache, "Backward", /*is_url_srp=*/false);

  histogram_tester.ExpectBucketCount("PreloadServingMetrics.Backward.All",
                                     3 /* kBFCache */, 1);

  ExpectFCP(histogram_tester, "WithoutPreload", {});
  ExpectFCP(histogram_tester, "WithPrefetch", {});
  ExpectFCP(histogram_tester, "WithPrerender", {});

  ExpectFCP(histogram_tester, "All.All.All", {});
}

// Verifies metrics recording for SRP navigation.
TEST(PreloadServingMetricsPageLoadMetricsObserverTest, NavigationWithSRP) {
  base::HistogramTester histogram_tester;

  page_load_metrics_internal::RecordPreloadServingMetricsByNavigationInitiator(
      content::UsedInstantLoad::kNoInstantLoad, "Other", /*is_url_srp=*/true);
  page_load_metrics_internal::RecordFirstContentfulPaint(
      base::Milliseconds(334), /*is_in_foreground=*/true,
      content::UsedInstantLoad::kNoInstantLoad, "Other", /*is_url_srp=*/true);

  ExpectFCP(histogram_tester, "All.All.All", {334});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "All.SRP.All", {334});
  ExpectFCP(histogram_tester, "All.SRP.NoInstantLoad", {334});

  ExpectFCP(histogram_tester, "Other.All.All", {334});
  ExpectFCP(histogram_tester, "Other.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "Other.SRP.All", {334});
  ExpectFCP(histogram_tester, "Other.SRP.NoInstantLoad", {334});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.SRP.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.SRP.NoInstantLoad", {334});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.NoInstantLoad",
            {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.SRP.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.SRP.NoInstantLoad",
            {334});

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      0 /* kNoInstantLoad */, 1);
  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.SRP",
                                      0 /* kNoInstantLoad */, 1);
}

// Verifies metrics recording for navigation with legacy search prefetch.
TEST(PreloadServingMetricsPageLoadMetricsObserverTest,
     NavigationWithLegacySearchPrefetch) {
  base::HistogramTester histogram_tester;

  page_load_metrics_internal::RecordPreloadServingMetricsByNavigationInitiator(
      content::UsedInstantLoad::kPrefetchWithoutPrePrefetch, "Other",
      /*is_url_srp=*/true);
  page_load_metrics_internal::RecordFirstContentfulPaint(
      base::Milliseconds(334), /*is_in_foreground=*/true,
      content::UsedInstantLoad::kPrefetchWithoutPrePrefetch, "Other",
      /*is_url_srp=*/true);

  ExpectFCP(histogram_tester, "WithPrefetch", {334});
  ExpectFCP(histogram_tester, "WithoutPreload", {});
  ExpectFCP(histogram_tester, "WithPrerender", {});

  ExpectFCP(histogram_tester, "All.All.All", {334});
  ExpectFCP(histogram_tester, "All.All.Prefetch", {334});
  ExpectFCP(histogram_tester, "All.SRP.All", {334});
  ExpectFCP(histogram_tester, "All.SRP.Prefetch", {334});

  ExpectFCP(histogram_tester, "Other.All.All", {334});
  ExpectFCP(histogram_tester, "Other.All.Prefetch", {334});
  ExpectFCP(histogram_tester, "Other.SRP.All", {334});
  ExpectFCP(histogram_tester, "Other.SRP.Prefetch", {334});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prefetch", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.SRP.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.SRP.Prefetch", {334});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prefetch", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.SRP.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.SRP.Prefetch", {334});

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      1 /* kPrefetch */, 1);
  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.SRP",
                                      1 /* kPrefetch */, 1);
}

// Verifies metrics recording for navigation with prefetch.
TEST(PreloadServingMetricsPageLoadMetricsObserverTest, NavigationWithPrefetch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPrefetchOffTheMainThread);
  base::HistogramTester histogram_tester;

  page_load_metrics_internal::RecordPreloadServingMetricsByNavigationInitiator(
      content::UsedInstantLoad::kPrefetchWithoutPrePrefetch, "Other",
      /*is_url_srp=*/false);
  page_load_metrics_internal::RecordFirstContentfulPaint(
      base::Milliseconds(334), /*is_in_foreground=*/true,
      content::UsedInstantLoad::kPrefetchWithoutPrePrefetch, "Other",
      /*is_url_srp=*/false);

  ExpectFCP(histogram_tester, "WithoutPreload", {});
  ExpectFCP(histogram_tester, "WithPrefetch", {334});
  ExpectFCP(histogram_tester, "WithPrefetch.WithPrePrefetch", {});
  ExpectFCP(histogram_tester, "WithPrefetch.WithoutPrePrefetch", {334});
  ExpectFCP(histogram_tester, "WithPrerender", {});

  ExpectFCP(histogram_tester, "All.All.All", {334});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "All.All.Prefetch", {334});
  ExpectFCP(histogram_tester, "All.All.Prefetch.WithPrePrefetch", {});
  ExpectFCP(histogram_tester, "All.All.Prefetch.WithoutPrePrefetch", {334});
  ExpectFCP(histogram_tester, "All.All.Prerender", {});

  ExpectFCP(histogram_tester, "Other.All.All", {334});
  ExpectFCP(histogram_tester, "Other.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "Other.All.Prefetch", {334});
  ExpectFCP(histogram_tester, "Other.All.Prefetch.WithPrePrefetch", {});
  ExpectFCP(histogram_tester, "Other.All.Prefetch.WithoutPrePrefetch", {334});
  ExpectFCP(histogram_tester, "Other.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prefetch", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.All.All.Prefetch.WithPrePrefetch", {});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.All.All.Prefetch.WithoutPrePrefetch", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prefetch", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.Other.All.Prefetch.WithPrePrefetch", {});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.Other.All.Prefetch.WithoutPrePrefetch", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prerender", {});

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      1 /* kPrefetch */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.Other.SRP", 0);
}

// Verifies metrics recording for navigation with prefetch using pre-prefetch.
TEST(PreloadServingMetricsPageLoadMetricsObserverTest,
     NavigationWithPrefetchWithPrePrefetch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPrefetchOffTheMainThread);
  base::HistogramTester histogram_tester;

  page_load_metrics_internal::RecordPreloadServingMetricsByNavigationInitiator(
      content::UsedInstantLoad::kPrefetchWithPrePrefetch, "Other",
      /*is_url_srp=*/false);
  page_load_metrics_internal::RecordFirstContentfulPaint(
      base::Milliseconds(334), /*is_in_foreground=*/true,
      content::UsedInstantLoad::kPrefetchWithPrePrefetch, "Other",
      /*is_url_srp=*/false);

  ExpectFCP(histogram_tester, "WithoutPreload", {});
  ExpectFCP(histogram_tester, "WithPrefetch", {334});
  ExpectFCP(histogram_tester, "WithPrefetch.WithPrePrefetch", {334});
  ExpectFCP(histogram_tester, "WithPrefetch.WithoutPrePrefetch", {});
  ExpectFCP(histogram_tester, "WithPrerender", {});

  ExpectFCP(histogram_tester, "All.All.All", {334});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "All.All.Prefetch", {334});
  ExpectFCP(histogram_tester, "All.All.Prefetch.WithPrePrefetch", {334});
  ExpectFCP(histogram_tester, "All.All.Prefetch.WithoutPrePrefetch", {});
  ExpectFCP(histogram_tester, "All.All.Prerender", {});

  ExpectFCP(histogram_tester, "Other.All.All", {334});
  ExpectFCP(histogram_tester, "Other.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "Other.All.Prefetch", {334});
  ExpectFCP(histogram_tester, "Other.All.Prefetch.WithPrePrefetch", {334});
  ExpectFCP(histogram_tester, "Other.All.Prefetch.WithoutPrePrefetch", {});
  ExpectFCP(histogram_tester, "Other.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prefetch", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.All.All.Prefetch.WithPrePrefetch", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.All.All.Prefetch.WithoutPrePrefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prerender", {});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prefetch", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.Other.All.Prefetch.WithPrePrefetch", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.Other.All.Prefetch.WithoutPrePrefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prerender", {});

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      1 /* kPrefetch */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.Other.SRP", 0);
}

// Verifies metrics recording for various navigation initiators and SRP
// combinations.
TEST(PreloadServingMetricsPageLoadMetricsObserverTest,
     RecordByNavigationInitiator) {
  base::HistogramTester histogram_tester;

  page_load_metrics_internal::RecordPreloadServingMetricsByNavigationInitiator(
      content::UsedInstantLoad::kNoInstantLoad, "TestInitiator",
      /*is_url_srp=*/true);
  page_load_metrics_internal::RecordFirstContentfulPaint(
      base::Milliseconds(334), /*is_in_foreground=*/true,
      content::UsedInstantLoad::kNoInstantLoad, "TestInitiator",
      /*is_url_srp=*/true);

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.TestInitiator.All",
                                      0 /* kNoInstantLoad */, 1);
  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.TestInitiator.SRP",
                                      0 /* kNoInstantLoad */, 1);

  ExpectFCP(histogram_tester, "All.All.All", {334});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "All.SRP.All", {334});
  ExpectFCP(histogram_tester, "All.SRP.NoInstantLoad", {334});

  ExpectFCP(histogram_tester, "TestInitiator.All.All", {334});
  ExpectFCP(histogram_tester, "TestInitiator.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "TestInitiator.SRP.All", {334});
  ExpectFCP(histogram_tester, "TestInitiator.SRP.NoInstantLoad", {334});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.SRP.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.SRP.NoInstantLoad", {334});

  ExpectFCP(histogram_tester, "WithoutFiltering.TestInitiator.All.All", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.TestInitiator.All.NoInstantLoad", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.TestInitiator.SRP.All", {334});
  ExpectFCP(histogram_tester,
            "WithoutFiltering.TestInitiator.SRP.NoInstantLoad", {334});
}

// Verifies metrics recording for navigation with prerender.
TEST(PreloadServingMetricsPageLoadMetricsObserverTest,
     NavigationWithPrerenderWithPrefetchAheadOfPrerender) {
  base::HistogramTester histogram_tester;

  page_load_metrics_internal::RecordPreloadServingMetricsByNavigationInitiator(
      content::UsedInstantLoad::kPrerender, "Other", /*is_url_srp=*/false);
  page_load_metrics_internal::RecordFirstContentfulPaint(
      base::Milliseconds(334), /*is_in_foreground=*/true,
      content::UsedInstantLoad::kPrerender, "Other", /*is_url_srp=*/false);

  ExpectFCP(histogram_tester, "WithoutPreload", {});
  ExpectFCP(histogram_tester, "WithPrefetch", {});
  ExpectFCP(histogram_tester, "WithPrerender", {334});

  ExpectFCP(histogram_tester, "All.All.All", {334});
  ExpectFCP(histogram_tester, "All.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "All.All.Prerender", {334});

  ExpectFCP(histogram_tester, "Other.All.All", {334});
  ExpectFCP(histogram_tester, "Other.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "Other.All.Prerender", {334});

  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.All.All.Prerender", {334});

  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.All", {334});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.NoInstantLoad", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prefetch", {});
  ExpectFCP(histogram_tester, "WithoutFiltering.Other.All.Prerender", {334});

  histogram_tester.ExpectUniqueSample("PreloadServingMetrics.Other.All",
                                      2 /* kPrerender */, 1);
  histogram_tester.ExpectTotalCount("PreloadServingMetrics.Other.SRP", 0);
}

}  // namespace
