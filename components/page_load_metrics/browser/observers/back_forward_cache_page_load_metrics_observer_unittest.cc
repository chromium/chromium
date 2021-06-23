// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/back_forward_cache_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/test/mock_navigation_handle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kTestUrl1[] = "https://www.google.com";

class BackForwardCachePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness {
 public:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    auto observer = std::make_unique<BackForwardCachePageLoadMetricsObserver>();
    observer_ = observer.get();
    tracker->AddObserver(std::move(observer));
  }

  void SetUp() override {
    PageLoadMetricsObserverContentTestHarness::SetUp();

    page_load_metrics::InitPageLoadTimingForTest(&timing_);

    // Navigating here so |RegisterObservers| will get called before each test.
    NavigateAndCommit(GURL(kTestUrl1));
  }

  void AssertHistoryNavigationRecordedAmpNavigation(bool was_amp) {
    auto entry_map = tester()->test_ukm_recorder().GetMergedEntriesByName(
        ukm::builders::HistoryNavigation::kEntryName);
    ASSERT_EQ(entry_map.size(), 1ull);
    ukm::mojom::UkmEntry* entry = entry_map.begin()->second.get();

    tester()->test_ukm_recorder().ExpectEntryMetric(
        entry,
        ukm::builders::HistoryNavigation::kBackForwardCache_IsAmpPageName,
        was_amp);
  }

  static const page_load_metrics::mojom::FrameMetadata& AmpMetadata() {
    static page_load_metrics::mojom::FrameMetadata metadata;
    metadata.behavior_flags |=
        blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded;
    return metadata;
  }

  static const page_load_metrics::mojom::FrameMetadata& NonAmpMetadata() {
    static page_load_metrics::mojom::FrameMetadata metadata;
    metadata.behavior_flags &=
        ~blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded;
    return metadata;
  }

  page_load_metrics::mojom::PageLoadTiming timing_;
  BackForwardCachePageLoadMetricsObserver* observer_;
};

TEST_F(BackForwardCachePageLoadMetricsObserverTest,
       OnRestoreFromBackForwardCache_NonAmpPageHasFalse) {
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_is_served_from_bfcache(true);

  tester()->SimulateMetadataUpdate(NonAmpMetadata(),
                                   web_contents()->GetMainFrame());
  observer_->OnRestoreFromBackForwardCache(timing_, &navigation_handle);

  AssertHistoryNavigationRecordedAmpNavigation(false);
}

TEST_F(BackForwardCachePageLoadMetricsObserverTest,
       OnRestoreFromBackForwardCache_AmpPageHasTrue) {
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_is_served_from_bfcache(true);

  tester()->SimulateMetadataUpdate(AmpMetadata(),
                                   web_contents()->GetMainFrame());
  observer_->OnRestoreFromBackForwardCache(timing_, &navigation_handle);

  AssertHistoryNavigationRecordedAmpNavigation(true);
}

TEST_F(BackForwardCachePageLoadMetricsObserverTest,
       OnNonBackForwardCacheNavigation_AmpPageIsUndefined) {
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_is_served_from_bfcache(false);

  tester()->SimulateMetadataUpdate(NonAmpMetadata(),
                                   web_contents()->GetMainFrame());

  // Since there was no call to observer_->OnRestoreFromBackForwardCache, there
  // should be no HistoryNavigation UKM entry.
  auto entry_map = tester()->test_ukm_recorder().GetMergedEntriesByName(
      ukm::builders::HistoryNavigation::kEntryName);
  EXPECT_EQ(entry_map.size(), 0ull);
}
