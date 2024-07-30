// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/back_forward_cache_page_load_metrics_observer.h"

#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/page_load_metrics/browser/fake_page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/test/mock_navigation_handle.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kTestUrl1[] = "https://www.google.com";

using content::Visibility;
using page_load_metrics::FakePageLoadMetricsObserverDelegate;
using page_load_metrics::PageLoadMetricsObserverDelegate;
using ukm::builders::HistoryNavigation;
using ukm::builders::UserPerceivedPageVisit;

class BackForwardCachePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness {
 public:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    auto observer = std::make_unique<BackForwardCachePageLoadMetricsObserver>();
    observer_ = observer.get();
    // TODO(crbug.com/40203717): Remove this when removing the DCHECK for lack
    // of page end metric logging from the back forward page load metrics
    // observer.
    observer_->logged_page_end_metrics_ = true;
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
    observer_with_fake_delegate_->has_ever_entered_back_forward_cache_ = true;
    observer_with_fake_delegate_->back_forward_cache_navigation_ids_.push_back(
        123456);
    // TODO(crbug.com/40203717): Remove this when removing the DCHECK for lack
    // of page end metric logging from the back forward page load metrics
    // observer.
    observer_with_fake_delegate_->logged_page_end_metrics_ = true;
    test_clock_ = std::make_unique<base::SimpleTestTickClock>();
    test_clock_->SetNowTicks(base::TimeTicks() + base::Milliseconds(25000));
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
      std::optional<base::TimeDelta> background_time) {
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

  void InvokeMeasureForegroundDuration(
      BackForwardCachePageLoadMetricsObserver* observer,
      bool simulate_app_backgrounding) {
    observer->MaybeRecordForegroundDurationAfterBackForwardCacheRestore(
        test_clock_.get(), simulate_app_backgrounding);
  }

  void SetObserverHidden() { observer_with_fake_delegate_->was_hidden_ = true; }

  // TODO(crbug.com/40203717): Remove this when removing the DCHECK for lack of
  // page end metric logging from the back forward page load metrics observer.
  void SetPageEndReasonLogged() { observer_->logged_page_end_metrics_ = true; }

  // Should declare first to avoid dangling pointer detection in the following
  // observers.
  std::unique_ptr<FakePageLoadMetricsObserverDelegate> fake_delegate_;

  page_load_metrics::mojom::PageLoadTiming timing_;
  raw_ptr<BackForwardCachePageLoadMetricsObserver, DanglingUntriaged> observer_;

  // |observer_with_fake_delegate_| is an observer set up with |fake_delegate_|
  // as its PageLoadMetricsObserverDelegate. This is for unit tests where it's
  // useful to easily directly configure the responses the delegate will return.
  // By default this observer assumes that the page has been in the back forward
  // cache, but is not currently in there.
  std::unique_ptr<BackForwardCachePageLoadMetricsObserver>
      observer_with_fake_delegate_;

  content::MockNavigationHandle navigation_handle_;
  std::unique_ptr<base::SimpleTestTickClock> test_clock_;
};

TEST_F(BackForwardCachePageLoadMetricsObserverTest,
       OnRestoreFromBackForwardCache_NonAmpPageHasFalse) {
  navigation_handle_.set_is_served_from_bfcache(true);

  tester()->SimulateMetadataUpdate(NonAmpMetadata(),
                                   web_contents()->GetPrimaryMainFrame());
  observer_->OnRestoreFromBackForwardCache(timing_, &navigation_handle_);

  AssertHistoryNavigationRecordedAmpNavigation(false);
  SetPageEndReasonLogged();
}

TEST_F(BackForwardCachePageLoadMetricsObserverTest,
       OnRestoreFromBackForwardCache_AmpPageHasTrue) {
  navigation_handle_.set_is_served_from_bfcache(true);

  tester()->SimulateMetadataUpdate(AmpMetadata(),
                                   web_contents()->GetPrimaryMainFrame());
  observer_->OnRestoreFromBackForwardCache(timing_, &navigation_handle_);

  AssertHistoryNavigationRecordedAmpNavigation(true);
  SetPageEndReasonLogged();
}

TEST_F(BackForwardCachePageLoadMetricsObserverTest,
       OnNonBackForwardCacheNavigation_AmpPageIsUndefined) {
  navigation_handle_.set_is_served_from_bfcache(false);

  tester()->SimulateMetadataUpdate(NonAmpMetadata(),
                                   web_contents()->GetPrimaryMainFrame());

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
  std::optional<base::TimeDelta> first_bg_time(base::Milliseconds(50));
  std::optional<base::TimeDelta> second_bg_time(base::Milliseconds(200));

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
                         /*background_time=*/std::optional<base::TimeDelta>());

  page_load_metrics::mojom::BackForwardCacheTiming bf_cache_timing;
  bf_cache_timing.first_paint_after_back_forward_cache_restore =
      base::Milliseconds(100);

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
      base::Milliseconds(500);
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
  std::vector<std::optional<base::TimeDelta>> test_times({
      first_bg_time,
      second_bg_time,
      std::nullopt,
  });
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

// Tests basic logging of foreground duration.
// Note that for this, and other foreground duration tests, that the logged
// foreground duration is bucketed, so even though the test sets up specific
// timings, the expected result from the logs is the bucketed result.
TEST_F(BackForwardCachePageLoadMetricsObserverTest,
       TestBasicForegroundLogging) {
  base::TimeTicks navigation_start =
      base::TimeTicks() + base::Milliseconds(100);

  auto in_foreground_bf_state =
      PageLoadMetricsObserverDelegate::BackForwardCacheRestore(
          /*was_in_foreground=*/true, navigation_start);
  fake_delegate_->AddBackForwardCacheRestore(in_foreground_bf_state);
  // Set up timing so that foreground duration will be 200ms (but will then be
  // bucketed)
  fake_delegate_->page_end_reason_ = page_load_metrics::END_OTHER;
  fake_delegate_->page_end_time_ = base::TimeTicks() + base::Milliseconds(300);

  InvokeMeasureForegroundDuration(observer_with_fake_delegate_.get(),
                                  /*simulate_app_backgrounding=*/false);

  auto& test_ukm_recorder = tester()->test_ukm_recorder();

  auto result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName);
  EXPECT_EQ(1U, result_metrics.size());
  EXPECT_EQ(200, result_metrics.begin()->begin()->second);

  // Check that another metric can be logged with different times.
  fake_delegate_->page_end_time_ = base::TimeTicks() + base::Milliseconds(1000);
  InvokeMeasureForegroundDuration(observer_with_fake_delegate_.get(),
                                  /*simulate_app_backgrounding=*/false);

  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName);
  EXPECT_EQ(2U, result_metrics.size());
  EXPECT_EQ(900, result_metrics[1].begin()->second);
}

TEST_F(BackForwardCachePageLoadMetricsObserverTest,
       TestLoggingWithNoPageEndWithFirstBackgroundTime) {
  // This start time is passed to the BackForwardCacheRestore struct, but isn't
  // actually supposed to be used in the calculations. It's just set here to
  // make sure that it isn't used.
  base::TimeTicks navigation_start =
      base::TimeTicks() + base::Milliseconds(100);

  auto in_foreground_bf_state =
      PageLoadMetricsObserverDelegate::BackForwardCacheRestore(
          /*was_in_foreground=*/true, navigation_start);
  in_foreground_bf_state.first_background_time = base::Milliseconds(400);
  fake_delegate_->AddBackForwardCacheRestore(in_foreground_bf_state);

  fake_delegate_->page_end_reason_ = page_load_metrics::END_NONE;

  InvokeMeasureForegroundDuration(observer_with_fake_delegate_.get(),
                                  /*simulate_app_backgrounding=*/false);
  auto& test_ukm_recorder = tester()->test_ukm_recorder();
  auto result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName);
  EXPECT_EQ(1U, result_metrics.size());
  EXPECT_EQ(400, result_metrics.begin()->begin()->second);

  // The app backgrounding argument should be irrelevant, so re-run and check
  // that two metrics are present (and the same).
  InvokeMeasureForegroundDuration(observer_with_fake_delegate_.get(),
                                  /*simulate_app_backgrounding=*/true);
  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName);
  EXPECT_EQ(2U, result_metrics.size());
  EXPECT_EQ(400, result_metrics[1].begin()->second);
}

// TODO(crbug.com/40200061): Flaky under TSan.
TEST_F(BackForwardCachePageLoadMetricsObserverTest,
       TestLoggingWithNoPageEndWithNoFirstBackgroundTime) {
  // In the case that there is no page end time and the page has never
  // backgrounded, the observer falls back to using Now as the end point of
  // the time in foreground. So override what 'Now' means.
  base::TimeTicks navigation_start =
      test_clock_->NowTicks() - base::Milliseconds(1000);
  base::TimeTicks() + base::Milliseconds(100);
  auto in_foreground_bf_state =
      PageLoadMetricsObserverDelegate::BackForwardCacheRestore(
          /*was_in_foreground=*/true, navigation_start);
  fake_delegate_->AddBackForwardCacheRestore(in_foreground_bf_state);

  fake_delegate_->page_end_reason_ = page_load_metrics::END_NONE;
  InvokeMeasureForegroundDuration(observer_with_fake_delegate_.get(),
                                  /*simulate_app_backgrounding=*/false);

  // There's no page end time, and no first background time, and the app isn't
  // backgrounding, so there should be no results.
  auto& test_ukm_recorder = tester()->test_ukm_recorder();
  auto result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName);
  EXPECT_EQ(0U, result_metrics.size());
  test_clock_->Advance(base::Milliseconds(250));
  // This time the app is entering the background, so a metric should be
  // logged.
  InvokeMeasureForegroundDuration(observer_with_fake_delegate_.get(),
                                  /*simulate_app_backgrounding=*/true);
  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName);
  EXPECT_EQ(1U, result_metrics.size());

  // The recorded time has been bucketed, so the test needs to figure out what
  // the bucketed version of the over-ridden Now is.
  base::TimeDelta expected_foreground_time =
      test_clock_->NowTicks() - navigation_start;
  int64_t bucketed_expected_time = ukm::GetSemanticBucketMinForDurationTiming(
      expected_foreground_time.InMilliseconds());
  EXPECT_EQ(bucketed_expected_time, result_metrics.begin()->begin()->second);
}

// Verifies that no foreground duration is logged if the page is restored from
// BFCache while not in the foreground.
TEST_F(BackForwardCachePageLoadMetricsObserverTest,
       DoesNotLogForegroundDurationIfRestoredInBackground) {
  // Tell the fake delegate that the BFCache restore happened in the background.
  // This means there should be no foreground duration recorded.
  auto no_foreground_bf_state =
      PageLoadMetricsObserverDelegate::BackForwardCacheRestore(
          /*was_in_foreground=*/false, base::TimeTicks::Now());
  fake_delegate_->AddBackForwardCacheRestore(no_foreground_bf_state);
  InvokeMeasureForegroundDuration(observer_with_fake_delegate_.get(),
                                  /*simulate_app_backgrounding=*/true);
  auto& test_ukm_recorder = tester()->test_ukm_recorder();
  auto result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName);
  EXPECT_EQ(0U, result_metrics.size());

  // This argument shouldn't make any difference.
  InvokeMeasureForegroundDuration(observer_with_fake_delegate_.get(),
                                  /*simulate_app_backgrounding=*/false);
  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName);
  EXPECT_EQ(0U, result_metrics.size());
}

// Verifies that no foreground duration is logged if the page is restored from
// BFCache while not in the foreground.
TEST_F(BackForwardCachePageLoadMetricsObserverTest,
       DoesNotLogForegroundDurationIfWasHidden) {
  auto in_foreground_bf_state =
      PageLoadMetricsObserverDelegate::BackForwardCacheRestore(
          /*was_in_foreground=*/true, base::TimeTicks::Now());
  fake_delegate_->AddBackForwardCacheRestore(in_foreground_bf_state);
  SetObserverHidden();
  InvokeMeasureForegroundDuration(observer_with_fake_delegate_.get(),
                                  /*simulate_app_backgrounding=*/true);
  auto& test_ukm_recorder = tester()->test_ukm_recorder();
  auto result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName);
  EXPECT_EQ(0U, result_metrics.size());

  // This argument shouldn't make any difference.
  InvokeMeasureForegroundDuration(observer_with_fake_delegate_.get(),
                                  /*simulate_app_backgrounding=*/false);
  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName);
  EXPECT_EQ(0U, result_metrics.size());
}

TEST_F(BackForwardCachePageLoadMetricsObserverTest,
       DoesNotLogForegroundDurationIfNeverEnteredBFCache) {
  auto never_in_bfcache_observer =
      std::make_unique<BackForwardCachePageLoadMetricsObserver>();
  never_in_bfcache_observer->SetDelegate(fake_delegate_.get());
  InvokeMeasureForegroundDuration(never_in_bfcache_observer.get(),
                                  /*simulate_app_backgrounding=*/false);
  auto& test_ukm_recorder = tester()->test_ukm_recorder();
  auto result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName);
  EXPECT_EQ(0U, result_metrics.size());

  InvokeMeasureForegroundDuration(never_in_bfcache_observer.get(),
                                  /*simulate_app_backgrounding=*/true);
  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kForegroundDurationAfterBackForwardCacheRestoreName);
  EXPECT_EQ(0U, result_metrics.size());
}
TEST_F(BackForwardCachePageLoadMetricsObserverTest,
       TestPageEndWhenBackgrounding) {
  // Give the stored BackForwardCacheRestoreState a starting navigation time of
  // twenty seconds ago, because later in the test we'll need a page_end_time.
  auto bf_state = PageLoadMetricsObserverDelegate::BackForwardCacheRestore(
      /*was_in_foreground=*/true, test_clock_->NowTicks() - base::Seconds(20));

  fake_delegate_->AddBackForwardCacheRestore(bf_state);
  // 'END_NONE' is the default, but set it explicitly because this test needs to
  // be sure the backgrounding only overrides the END_NONE end reason.
  fake_delegate_->page_end_reason_ = page_load_metrics::PageEndReason::END_NONE;
  observer_with_fake_delegate_->FlushMetricsOnAppEnterBackground(timing_);
  auto& test_ukm_recorder = tester()->test_ukm_recorder();
  auto result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kPageEndReasonAfterBackForwardCacheRestoreName);
  EXPECT_EQ(1U, result_metrics.size());
  EXPECT_EQ(HistoryNavigation::kPageEndReasonAfterBackForwardCacheRestoreName,
            result_metrics.begin()->begin()->first);
  EXPECT_EQ(page_load_metrics::PageEndReason::END_APP_ENTER_BACKGROUND,
            result_metrics.begin()->begin()->second);

  // Observers stop logging after FlushMetricsOnAppEnterBackground is called,
  // until they're restored.
  observer_with_fake_delegate_->OnRestoreFromBackForwardCache(
      timing_, &navigation_handle_);
  fake_delegate_->AddBackForwardCacheRestore(bf_state);

  // Verify that backgrounding does not take precedence over other page end
  // reasons. Note that if there's a page_end_reason, there needs to be a
  // page_end_time as well.
  fake_delegate_->page_end_reason_ =
      page_load_metrics::PageEndReason::END_NEW_NAVIGATION;
  fake_delegate_->page_end_time_ = base::TimeTicks::Now();
  observer_with_fake_delegate_->FlushMetricsOnAppEnterBackground(timing_);
  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      HistoryNavigation::kEntryName,
      HistoryNavigation::kPageEndReasonAfterBackForwardCacheRestoreName);
  EXPECT_EQ(2U, result_metrics.size());
  EXPECT_EQ(HistoryNavigation::kPageEndReasonAfterBackForwardCacheRestoreName,
            result_metrics[1].begin()->first);
  EXPECT_EQ(page_load_metrics::PageEndReason::END_NEW_NAVIGATION,
            result_metrics[1].begin()->second);
}

TEST_F(BackForwardCachePageLoadMetricsObserverTest, TestLogsNonCWVPageVisit) {
  auto fake_bfcache_restore =
      PageLoadMetricsObserverDelegate::BackForwardCacheRestore(
          /*was_in_foreground=*/true, base::TimeTicks());
  fake_delegate_->AddBackForwardCacheRestore(fake_bfcache_restore);
  observer_with_fake_delegate_->ShouldObserveMimeType("fake-mime-type");
  auto& test_ukm_recorder = tester()->test_ukm_recorder();
  auto result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      UserPerceivedPageVisit::kEntryName,
      UserPerceivedPageVisit::kNotCountedForCoreWebVitalsName);
  EXPECT_EQ(1U, result_metrics.size());
  EXPECT_EQ(UserPerceivedPageVisit::kNotCountedForCoreWebVitalsName,
            result_metrics[0].begin()->first);
  EXPECT_TRUE(result_metrics[0].begin()->second);

  observer_with_fake_delegate_->ShouldObserveMimeType("text/html");
  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      UserPerceivedPageVisit::kEntryName,
      UserPerceivedPageVisit::kNotCountedForCoreWebVitalsName);
  // The metric being tested here indicates whether or not logs should be
  // ignored when counting logs towards Core Web Vitals. Either the metric being
  // present and false, or the metric being absent completely, means the logs
  // shouldn't be counted.
  // We've just indicated that these logs *should* be counted.
  // So if the result metrics size is still 1, this test has passed, and if the
  // result metrics are of size 2, the new value should be false.
  if (result_metrics.size() > 1) {
    EXPECT_EQ(2U, result_metrics.size());
    EXPECT_EQ(UserPerceivedPageVisit::kNotCountedForCoreWebVitalsName,
              result_metrics[1].begin()->first);
    EXPECT_FALSE(result_metrics[1].begin()->second);
  }
}

TEST_F(BackForwardCachePageLoadMetricsObserverTest, TestLogsUserInitiated) {
  auto& test_ukm_recorder = tester()->test_ukm_recorder();
  auto fake_bfcache_restore =
      PageLoadMetricsObserverDelegate::BackForwardCacheRestore(
          /*was_in_foreground=*/true, base::TimeTicks());
  fake_delegate_->AddBackForwardCacheRestore(fake_bfcache_restore);

  fake_delegate_->user_initiated_info_ =
      page_load_metrics::UserInitiatedInfo::NotUserInitiated();
  observer_with_fake_delegate_->OnComplete(timing_);

  auto result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      UserPerceivedPageVisit::kEntryName,
      UserPerceivedPageVisit::kUserInitiatedName);
  EXPECT_EQ(1U, result_metrics.size());
  EXPECT_EQ(UserPerceivedPageVisit::kUserInitiatedName,
            result_metrics[0].begin()->first);
  EXPECT_FALSE(result_metrics[0].begin()->second);

  // Browser initiated; this is always considered user initiated.
  fake_delegate_->user_initiated_info_ =
      page_load_metrics::UserInitiatedInfo::BrowserInitiated();
  observer_with_fake_delegate_->OnComplete(timing_);

  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      UserPerceivedPageVisit::kEntryName,
      UserPerceivedPageVisit::kUserInitiatedName);
  EXPECT_EQ(2U, result_metrics.size());
  EXPECT_EQ(UserPerceivedPageVisit::kUserInitiatedName,
            result_metrics[1].begin()->first);
  EXPECT_TRUE(result_metrics[1].begin()->second);

  // Renderer initiated, with user input, considered user initiated.
  fake_delegate_->user_initiated_info_ =
      page_load_metrics::UserInitiatedInfo::RenderInitiated(true, true);
  observer_with_fake_delegate_->OnComplete(timing_);

  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      UserPerceivedPageVisit::kEntryName,
      UserPerceivedPageVisit::kUserInitiatedName);
  EXPECT_EQ(3U, result_metrics.size());
  EXPECT_EQ(UserPerceivedPageVisit::kUserInitiatedName,
            result_metrics[2].begin()->first);
  EXPECT_TRUE(result_metrics[2].begin()->second);

  // Renderer initiated, without user input, not considered user initiated.
  fake_delegate_->user_initiated_info_ =
      page_load_metrics::UserInitiatedInfo::RenderInitiated(false, false);
  observer_with_fake_delegate_->OnComplete(timing_);

  result_metrics = test_ukm_recorder.FilteredHumanReadableMetricForEntry(
      UserPerceivedPageVisit::kEntryName,
      UserPerceivedPageVisit::kUserInitiatedName);
  EXPECT_EQ(4U, result_metrics.size());
  EXPECT_EQ(UserPerceivedPageVisit::kUserInitiatedName,
            result_metrics[3].begin()->first);
  EXPECT_FALSE(result_metrics[3].begin()->second);
}
