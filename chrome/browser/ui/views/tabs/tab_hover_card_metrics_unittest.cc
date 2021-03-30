// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_metrics.h"

#include <initializer_list>
#include <memory>

#include "base/logging.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Provides a mock delegate used for testing without having to have a browser,
// tabstrip, tabs, or a real controller.
class MockHoverCardMetricsDelegate : public TabHoverCardMetrics::Delegate {
 public:
  void set_tab_count(size_t tab_count) {
    DCHECK_GT(tab_count, 0U);
    tab_count_ = tab_count;
  }

  void set_previews_enabled(bool previews_enabled) {
    previews_enabled_ = previews_enabled;
    thumbnail_loaded_ &= previews_enabled_;
  }

  void set_thumbnail_loaded(bool thumbnail_loaded) {
    DCHECK(previews_enabled_);
    thumbnail_loaded_ = thumbnail_loaded;
  }

  // TabHoverCardMetrics::Delegate:
  size_t GetTabCount() const override { return tab_count_; }
  bool ArePreviewsEnabled() const override { return previews_enabled_; }
  bool HasPreviewImage() const override { return thumbnail_loaded_; }
  views::Widget* GetHoverCardWidget() override { return nullptr; }

 private:
  size_t tab_count_ = 1U;
  bool previews_enabled_ = false;
  bool thumbnail_loaded_ = false;
};

// Create some sample tab handles that don't correspond to real tabs, but which
// are unique.
const TabHoverCardMetrics::TabHandle kTabHandle1 =
    reinterpret_cast<TabHoverCardMetrics::TabHandle>(1);
const TabHoverCardMetrics::TabHandle kTabHandle2 =
    reinterpret_cast<TabHoverCardMetrics::TabHandle>(2);
const TabHoverCardMetrics::TabHandle kTabHandle3 =
    reinterpret_cast<TabHoverCardMetrics::TabHandle>(3);

// Create some intervals that will fall into different buckets in the histogram.
constexpr int kShortDelayMS = 200;
constexpr int kMediumDelayMS = 500;
constexpr int kLongDelayMS = 1000;
constexpr base::TimeDelta kShortDelay =
    base::TimeDelta::FromMilliseconds(kShortDelayMS);
constexpr base::TimeDelta kMediumDelay =
    base::TimeDelta::FromMilliseconds(kMediumDelayMS);
constexpr base::TimeDelta kLongDelay =
    base::TimeDelta::FromMilliseconds(kLongDelayMS);

std::string GetFullHistogramName(const char* prefix, size_t tab_count) {
  return TabHoverCardMetrics::GetBucketHistogramName(
      prefix, TabHoverCardMetrics::GetBucketForTabCount(tab_count));
}

}  // namespace

class TabHoverCardMetricsTest : public ::testing::Test {
 public:
  void SetUp() override {
    delegate_ = std::make_unique<MockHoverCardMetricsDelegate>();
    metrics_ = std::make_unique<TabHoverCardMetrics>(delegate_.get());
  }

  void TearDown() override {
    // Dump all histograms on failure so we can debug what went wrong.
    if (HasFailure())
      LOG(WARNING) << histograms_.GetAllHistogramsRecorded();
  }

  struct BucketCount {
    int sample;
    size_t count;
  };

  void ExpectResults(const char* prefix,
                     size_t tab_count,
                     std::initializer_list<BucketCount> counts) {
    const std::string name = GetFullHistogramName(prefix, tab_count);
    size_t total_count = 0;
    for (const BucketCount& bucket_count : counts) {
      total_count += bucket_count.count;
      histograms_.ExpectBucketCount(name, bucket_count.sample,
                                    bucket_count.count);
    }
    histograms_.ExpectTotalCount(name, total_count);
  }

 protected:
  const char* kCardsSeenPrefix =
      TabHoverCardMetrics::kHistogramPrefixHoverCardsSeenBeforeSelection;
  const char* kPreviewsSeenPrefix =
      TabHoverCardMetrics::kHistogramPrefixPreviewsSeenBeforeSelection;
  const char* kCardTimePrefix =
      TabHoverCardMetrics::kHistogramPrefixTabHoverCardTime;
  const char* kPreviewTimePrefix =
      TabHoverCardMetrics::kHistogramPrefixTabPreviewTime;
  const char* kLastCardTimePrefix =
      TabHoverCardMetrics::kHistogramPrefixLastTabHoverCardTime;
  const char* kLastPreviewTimePrefix =
      TabHoverCardMetrics::kHistogramPrefixLastTabPreviewTime;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MockHoverCardMetricsDelegate> delegate_;
  std::unique_ptr<TabHoverCardMetrics> metrics_;
  base::HistogramTester histograms_;
};

TEST_F(TabHoverCardMetricsTest, SimpleSequenceWithoutPreviews) {
  metrics_->InitialCardBeingShown();
  metrics_->CardFullyVisibleOnTab(kTabHandle1, true);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle2, false);
  task_environment_.AdvanceClock(kMediumDelay);
  metrics_->CardWillBeHidden();
  metrics_->TabSelectedViaMouse(kTabHandle2);

  ExpectResults(kCardsSeenPrefix, 1, {{2, 1}});
  ExpectResults(kPreviewsSeenPrefix, 1, {{0, 1}});
  ExpectResults(kLastCardTimePrefix, 1, {{kMediumDelayMS, 1}});
  ExpectResults(kCardTimePrefix, 1, {{kShortDelayMS, 1}, {kMediumDelayMS, 1}});

  // These should have no data:
  ExpectResults(kLastPreviewTimePrefix, 1, {});
  ExpectResults(kPreviewTimePrefix, 1, {});
}

TEST_F(TabHoverCardMetricsTest, HideSignalFollowsSelection) {
  metrics_->InitialCardBeingShown();
  metrics_->CardFullyVisibleOnTab(kTabHandle1, true);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle2, false);
  task_environment_.AdvanceClock(kMediumDelay);
  metrics_->TabSelectedViaMouse(kTabHandle2);
  metrics_->CardWillBeHidden();

  ExpectResults(kCardsSeenPrefix, 1, {{2, 1}});
  ExpectResults(kPreviewsSeenPrefix, 1, {{0, 1}});
  ExpectResults(kLastCardTimePrefix, 1, {{kMediumDelayMS, 1}});
  ExpectResults(kCardTimePrefix, 1, {{kShortDelayMS, 1}, {kMediumDelayMS, 1}});

  // These should have no data:
  ExpectResults(kLastPreviewTimePrefix, 1, {});
  ExpectResults(kPreviewTimePrefix, 1, {});
}

TEST_F(TabHoverCardMetricsTest, SimpleSequenceWithPreviews) {
  delegate_->set_previews_enabled(true);

  metrics_->InitialCardBeingShown();
  metrics_->CardFullyVisibleOnTab(kTabHandle1, true);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle2, false);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->ImageLoadedForTab(kTabHandle2);
  task_environment_.AdvanceClock(kMediumDelay);
  metrics_->CardWillBeHidden();
  metrics_->TabSelectedViaMouse(kTabHandle2);

  ExpectResults(kCardsSeenPrefix, 1, {{2, 1}});
  ExpectResults(kPreviewsSeenPrefix, 1, {{1, 1}});
  ExpectResults(kLastCardTimePrefix, 1, {{kShortDelayMS + kMediumDelayMS, 1}});
  ExpectResults(kCardTimePrefix, 1,
                {{kShortDelayMS, 1}, {kShortDelayMS + kMediumDelayMS, 1}});
  ExpectResults(kLastPreviewTimePrefix, 1, {{kMediumDelayMS, 1}});
  ExpectResults(kPreviewTimePrefix, 1, {{kMediumDelayMS, 1}});
}

TEST_F(TabHoverCardMetricsTest, CardDoesNotReachLastTab) {
  delegate_->set_previews_enabled(true);

  metrics_->InitialCardBeingShown();
  metrics_->CardFullyVisibleOnTab(kTabHandle1, false);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->ImageLoadedForTab(kTabHandle1);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->CardWillBeHidden();
  metrics_->TabSelectedViaMouse(kTabHandle2);

  ExpectResults(kCardsSeenPrefix, 1, {{1, 1}});
  ExpectResults(kPreviewsSeenPrefix, 1, {{1, 1}});
  ExpectResults(kLastCardTimePrefix, 1, {{0, 1}});
  ExpectResults(kCardTimePrefix, 1, {{kShortDelayMS * 2, 1}});
  ExpectResults(kLastPreviewTimePrefix, 1, {{0, 1}});
  ExpectResults(kPreviewTimePrefix, 1, {{kShortDelayMS, 1}});
}

TEST_F(TabHoverCardMetricsTest, ImageAlreadyLoadedForTab) {
  delegate_->set_previews_enabled(true);

  metrics_->InitialCardBeingShown();
  delegate_->set_thumbnail_loaded(true);
  metrics_->CardFullyVisibleOnTab(kTabHandle1, false);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->CardWillBeHidden();
  metrics_->TabSelectedViaMouse(kTabHandle1);

  ExpectResults(kCardsSeenPrefix, 1, {{1, 1}});
  ExpectResults(kPreviewsSeenPrefix, 1, {{1, 1}});
  ExpectResults(kLastCardTimePrefix, 1, {{kShortDelayMS, 1}});
  ExpectResults(kCardTimePrefix, 1, {{kShortDelayMS, 1}});
  ExpectResults(kLastPreviewTimePrefix, 1, {{kShortDelayMS, 1}});
  ExpectResults(kPreviewTimePrefix, 1, {{kShortDelayMS, 1}});
}

TEST_F(TabHoverCardMetricsTest, MediumTabCount) {
  delegate_->set_tab_count(7);

  metrics_->InitialCardBeingShown();
  metrics_->CardFullyVisibleOnTab(kTabHandle1, false);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle2, false);
  task_environment_.AdvanceClock(kMediumDelay);
  metrics_->CardWillBeHidden();
  metrics_->TabSelectedViaMouse(kTabHandle2);

  ExpectResults(kCardsSeenPrefix, 7, {{2, 1}});
  ExpectResults(kCardTimePrefix, 7, {{kShortDelayMS, 1}, {kMediumDelayMS, 1}});
  ExpectResults(kLastCardTimePrefix, 7, {{kMediumDelayMS, 1}});
}

TEST_F(TabHoverCardMetricsTest, VeryLargeTabCount) {
  delegate_->set_tab_count(60);

  metrics_->InitialCardBeingShown();
  metrics_->CardFullyVisibleOnTab(kTabHandle1, false);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle2, false);
  task_environment_.AdvanceClock(kMediumDelay);
  metrics_->CardWillBeHidden();
  metrics_->TabSelectedViaMouse(kTabHandle2);

  ExpectResults(kCardsSeenPrefix, 60, {{2, 1}});
  ExpectResults(kCardTimePrefix, 60, {{kShortDelayMS, 1}, {kMediumDelayMS, 1}});
  ExpectResults(kLastCardTimePrefix, 60, {{kMediumDelayMS, 1}});
}

TEST_F(TabHoverCardMetricsTest, RepeatingSequence) {
  metrics_->InitialCardBeingShown();
  metrics_->CardFullyVisibleOnTab(kTabHandle1, true);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle2, false);
  task_environment_.AdvanceClock(kMediumDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle3, false);
  task_environment_.AdvanceClock(kLongDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle2, false);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->CardWillBeHidden();
  metrics_->TabSelectedViaMouse(kTabHandle2);

  ExpectResults(kCardsSeenPrefix, 1, {{4, 1}});
  ExpectResults(kLastCardTimePrefix, 1, {{kShortDelayMS, 1}});
  ExpectResults(kCardTimePrefix, 1,
                {{kShortDelayMS, 2}, {kMediumDelayMS, 1}, {kLongDelayMS, 1}});
}

TEST_F(TabHoverCardMetricsTest, MultipleSequences) {
  metrics_->InitialCardBeingShown();
  metrics_->CardFullyVisibleOnTab(kTabHandle1, true);
  task_environment_.AdvanceClock(kMediumDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle2, false);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->CardWillBeHidden();
  metrics_->TabSelectedViaMouse(kTabHandle2);

  metrics_->InitialCardBeingShown();
  metrics_->CardFullyVisibleOnTab(kTabHandle1, false);
  task_environment_.AdvanceClock(kLongDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle2, true);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle3, false);
  task_environment_.AdvanceClock(kMediumDelay);
  metrics_->CardWillBeHidden();
  metrics_->TabSelectedViaMouse(kTabHandle3);

  ExpectResults(kCardsSeenPrefix, 1, {{2, 1}, {3, 1}});
  ExpectResults(kLastCardTimePrefix, 1,
                {{kShortDelayMS, 1}, {kMediumDelayMS, 1}});
  ExpectResults(kCardTimePrefix, 1,
                {{kShortDelayMS, 2}, {kMediumDelayMS, 2}, {kLongDelayMS, 1}});
}

TEST_F(TabHoverCardMetricsTest, ResumeSequenceFromSameTab) {
  metrics_->InitialCardBeingShown();
  metrics_->CardFullyVisibleOnTab(kTabHandle1, true);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle2, false);
  task_environment_.AdvanceClock(kMediumDelay);
  metrics_->CardWillBeHidden();
  metrics_->CardFullyVisibleOnTab(kTabHandle2, false);
  task_environment_.AdvanceClock(kMediumDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle3, false);
  task_environment_.AdvanceClock(kLongDelay);
  metrics_->CardWillBeHidden();
  metrics_->TabSelectedViaMouse(kTabHandle3);

  ExpectResults(kCardsSeenPrefix, 1, {{4, 1}});
  ExpectResults(kPreviewsSeenPrefix, 1, {{0, 1}});
  ExpectResults(kLastCardTimePrefix, 1, {{kLongDelayMS, 1}});
  ExpectResults(kCardTimePrefix, 1,
                {{kShortDelayMS, 1}, {kMediumDelayMS, 2}, {kLongDelayMS, 1}});

  // These should have no data:
  ExpectResults(kLastPreviewTimePrefix, 1, {});
  ExpectResults(kPreviewTimePrefix, 1, {});
}

TEST_F(TabHoverCardMetricsTest, ResumeSequenceFromDifferentTab) {
  metrics_->InitialCardBeingShown();
  metrics_->CardFullyVisibleOnTab(kTabHandle1, true);
  task_environment_.AdvanceClock(kShortDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle2, false);
  task_environment_.AdvanceClock(kMediumDelay);
  metrics_->CardWillBeHidden();
  metrics_->CardFullyVisibleOnTab(kTabHandle3, false);
  task_environment_.AdvanceClock(kMediumDelay);
  metrics_->CardFullyVisibleOnTab(kTabHandle2, false);
  task_environment_.AdvanceClock(kLongDelay);
  metrics_->CardWillBeHidden();
  metrics_->TabSelectedViaMouse(kTabHandle2);

  ExpectResults(kCardsSeenPrefix, 1, {{4, 1}});
  ExpectResults(kPreviewsSeenPrefix, 1, {{0, 1}});
  ExpectResults(kLastCardTimePrefix, 1, {{kLongDelayMS, 1}});
  ExpectResults(kCardTimePrefix, 1,
                {{kShortDelayMS, 1}, {kMediumDelayMS, 2}, {kLongDelayMS, 1}});

  // These should have no data:
  ExpectResults(kLastPreviewTimePrefix, 1, {});
  ExpectResults(kPreviewTimePrefix, 1, {});
}
