// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/back_forward_cache_page_load_metrics_observer.h"

#include "base/memory/raw_ptr.h"
#include "components/page_load_metrics/browser/fake_page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/test/mock_navigation_handle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kTestUrl1[] = "https://www.google.com";

using page_load_metrics::FakePageLoadMetricsObserverDelegate;
using page_load_metrics::PageLoadMetricsObserverDelegate;
using ukm::builders::HistoryNavigation;

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

    observer_with_fake_delegate_ =
        std::make_unique<BackForwardCachePageLoadMetricsObserver>();
    fake_delegate_ = std::make_unique<FakePageLoadMetricsObserverDelegate>();
    fake_delegate_->web_contents_ = web_contents();
    observer_with_fake_delegate_->SetDelegate(fake_delegate_.get());
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

  void AddBFCacheRestoreState(
      BackForwardCachePageLoadMetricsObserver* observer,
      FakePageLoadMetricsObserverDelegate* fake_delegate,
      bool was_in_foreground,
      int64_t navigation_id,
      absl::optional<base::TimeDelta> background_time) {
    observer->back_forward_cache_navigation_ids_.push_back(navigation_id);
    auto bf_state = PageLoadMetricsObserverDelegate::BackForwardCacheRestore(
        was_in_foreground, base::TimeTicks::Now());
    if (background_time.has_value())
      bf_state.first_background_time = background_time.value();
    fake_delegate->AddBackForwardCacheRestore(bf_state);
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
  raw_ptr<BackForwardCachePageLoadMetricsObserver> observer_;

  // |observer_with_fake_delegate_| is an observer set up with |fake_delegate_|
  // as its PageLoadMetricsObserverDelegate. This is for unit tests where it's
  // useful to easily directly configure the responses the delegate will return.
  // By default this observer assumes that the page has been in the back forward
  // cache, but is not currently in there.
  std::unique_ptr<BackForwardCachePageLoadMetricsObserver>
      observer_with_fake_delegate_;
  std::unique_ptr<FakePageLoadMetricsObserverDelegate> fake_delegate_;

  content::MockNavigationHandle navigation_handle_;
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

// Tests that NavigationToFirstPaint are correctly logged after a BFCache
// restore, based on the BackForwardCacheRestore structs the page load
// tracker has available.
// * No logging if the restore was not in the foreground
// * No logging if the FirstPaint timestamp is after the restored page
//   backgrounded.
// * Log if the page is either still active (no background time) or the
//   timestamp for the paint is before the background time for that back forward
//   cache restore.
TEST_F(BackForwardCachePageLoadMetricsObserverTest,
       TestOnFirstPaintAfterBackForwardCacheRestoreInPage) {
  absl::optional<base::TimeDelta> first_bg_time(
      base::TimeDelta::FromMilliseconds(50));
  absl::optional<base::TimeDelta> second_bg_time(
      base::TimeDelta::FromMilliseconds(200));

  AddBFCacheRestoreState(observer_with_fake_delegate_.get(),
                         fake_delegate_.get(), /*was_in_foreground=*/true,
                         /*navigation_id=*/123,
                         /*background_time=*/first_bg_time);
  AddBFCacheRestoreState(observer_with_fake_delegate_.get(),
                         fake_delegate_.get(), /*was_in_foreground=*/true,
                         /*navigation_id=*/456,
                         /*background_time=*/second_bg_time);
  AddBFCacheRestoreState(observer_with_fake_delegate_.get(),
                         fake_delegate_.get(), /*was_in_foreground=*/true,
                         /*navigation_id=*/789,
                         /*background_time=*/absl::optional<base::TimeDelta>());

  page_load_metrics::mojom::BackForwardCacheTiming bf_cache_timing;
  bf_cache_timing.first_paint_after_back_forward_cache_restore =
      base::TimeDelta::FromMilliseconds(100);

  size_t expected_metrics_count = 0;
  auto& test_ukm_recorder = tester()->test_ukm_recorder();

  // No FCP should be logged for this call - the index passed is for a bfcache
  // restore that backgrounded before the FCP timestamp.
  observer_with_fake_delegate_->OnFirstPaintAfterBackForwardCacheRestoreInPage(
      bf_cache_timing, /*index=*/0);
  auto result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::
          kNavigationToFirstPaintAfterBackForwardCacheRestoreName);
  EXPECT_EQ(expected_metrics_count, result_metrics.size());

  // An FCP should be logged here, because the BFCacheRestore struct at index 1
  // has a background time after the FCP time.
  expected_metrics_count++;
  observer_with_fake_delegate_->OnFirstPaintAfterBackForwardCacheRestoreInPage(
      bf_cache_timing, /*index=*/1);
  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::
          kNavigationToFirstPaintAfterBackForwardCacheRestoreName);
  EXPECT_EQ(expected_metrics_count, result_metrics.size());
  EXPECT_EQ(HistoryNavigation::
                kNavigationToFirstPaintAfterBackForwardCacheRestoreName,
            result_metrics.begin()->begin()->first);
  EXPECT_EQ(100, result_metrics.begin()->begin()->second);

  // An FCP should also be logged here, because the BFCacheRestore struct at
  // index 2 has no background time (i.e. is still active).
  expected_metrics_count++;
  bf_cache_timing.first_paint_after_back_forward_cache_restore =
      base::TimeDelta::FromMilliseconds(500);
  observer_with_fake_delegate_->OnFirstPaintAfterBackForwardCacheRestoreInPage(
      bf_cache_timing, /*index=*/2);
  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::
          kNavigationToFirstPaintAfterBackForwardCacheRestoreName);
  EXPECT_EQ(expected_metrics_count, result_metrics.size());
  EXPECT_EQ(HistoryNavigation::
                kNavigationToFirstPaintAfterBackForwardCacheRestoreName,
            result_metrics.begin()->begin()->first);
  EXPECT_EQ(500, result_metrics[1].begin()->second);

  // None of these should cause logs, as all of these restores are started in
  // the background.
  std::vector<absl::optional<base::TimeDelta>> test_times(
      {first_bg_time, second_bg_time, absl::optional<base::TimeDelta>()});
  size_t index = 3;
  for (auto bg_time : test_times) {
    AddBFCacheRestoreState(observer_with_fake_delegate_.get(),
                           fake_delegate_.get(), /*was_in_foreground=*/false,
                           /*navigation_id=*/123 * index,
                           /*background_time=*/bg_time);
    observer_with_fake_delegate_
        ->OnFirstPaintAfterBackForwardCacheRestoreInPage(bf_cache_timing,
                                                         index++);
    result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
        HistoryNavigation::kEntryName,
        HistoryNavigation::
            kNavigationToFirstPaintAfterBackForwardCacheRestoreName);
    EXPECT_EQ(expected_metrics_count, result_metrics.size());
  }
}
