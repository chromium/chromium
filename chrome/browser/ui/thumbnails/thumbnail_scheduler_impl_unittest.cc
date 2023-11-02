// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_scheduler_impl.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeTabCapturer : public ThumbnailScheduler::TabCapturer {
 public:
  FakeTabCapturer() = default;
  ~FakeTabCapturer() override = default;

  bool capture_permitted() const { return capture_permitted_; }

  // ThumbnailScheduler::TabCapturer:
  void SetCapturePermittedByScheduler(bool capture_permitted) override {
    capture_permitted_ = capture_permitted;
  }

 private:
  bool capture_permitted_ = false;
};

}  // namespace

using ::testing::_;
using ::testing::ElementsAre;

class ThumbnailSchedulerImplTest : public ::testing::Test {
 public:
  ThumbnailSchedulerImplTest() : scheduler_(2, 1), tabs_(4) {}
  ~ThumbnailSchedulerImplTest() override = default;

  void SetUp() override { AddTabsToScheduler(); }

  void TearDown() override { RemoveTabsFromScheduler(); }

 protected:
  void AddTabsToScheduler() {
    for (FakeTabCapturer& tab : tabs_)
      scheduler_.AddTab(&tab);
  }

  void RemoveTabsFromScheduler() {
    for (FakeTabCapturer& tab : tabs_)
      scheduler_.RemoveTab(&tab);
  }

  std::vector<bool> TabScheduledStates() {
    std::vector<bool> states;
    for (const auto& tab : tabs_)
      states.push_back(tab.capture_permitted());
    return states;
  }

  ThumbnailSchedulerImpl scheduler_;
  std::vector<FakeTabCapturer> tabs_;
};

TEST_F(ThumbnailSchedulerImplTest, TabsNotScheduledIfCaptureNotNeeded) {
  // By default, tabs' priorities are kNone. No captures should be
  // scheduled.
  EXPECT_THAT(TabScheduledStates(), ElementsAre(false, false, false, false));
}

TEST_F(ThumbnailSchedulerImplTest, HighPriorityCapturesScheduledUpToMax) {
  scheduler_.SetTabCapturePriority(
      &tabs_[0], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, false, false, false));

  scheduler_.SetTabCapturePriority(
      &tabs_[1], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, true, false, false));

  // A new capture of the same priority shouldn't preempt an existing
  // one. Otherwise there may be too much churn: a tab almost done
  // capturing might be descheduled, only to restart capture later.
  scheduler_.SetTabCapturePriority(
      &tabs_[2], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, true, false, false));
}

TEST_F(ThumbnailSchedulerImplTest, LowPriorityCapturesScheduledUpToMax) {
  scheduler_.SetTabCapturePriority(
      &tabs_[0], ThumbnailSchedulerImpl::TabCapturePriority::kLow);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, false, false, false));

  // Similar to the above test, don't preempt.
  scheduler_.SetTabCapturePriority(
      &tabs_[1], ThumbnailSchedulerImpl::TabCapturePriority::kLow);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, false, false, false));
}

TEST_F(ThumbnailSchedulerImplTest, HighPriorityCapturesPreemptLowPriority) {
  scheduler_.SetTabCapturePriority(
      &tabs_[0], ThumbnailSchedulerImpl::TabCapturePriority::kLow);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, false, false, false));

  scheduler_.SetTabCapturePriority(
      &tabs_[1], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, true, false, false));

  // There should never be a low priority capture running while a high
  // priority capture is descheduled. When the high priority tabs
  // exceeds the max total captures (2) minus the max low priority
  // captures (1), the high priority tabs take precedence.
  scheduler_.SetTabCapturePriority(
      &tabs_[2], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(false, true, true, false));
}

TEST_F(ThumbnailSchedulerImplTest, NewTabScheduledWhenCaptureFinished) {
  scheduler_.SetTabCapturePriority(
      &tabs_[0], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  scheduler_.SetTabCapturePriority(
      &tabs_[1], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  scheduler_.SetTabCapturePriority(
      &tabs_[2], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  scheduler_.SetTabCapturePriority(
      &tabs_[3], ThumbnailSchedulerImpl::TabCapturePriority::kLow);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, true, false, false));

  scheduler_.SetTabCapturePriority(
      &tabs_[1], ThumbnailSchedulerImpl::TabCapturePriority::kNone);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, false, true, false));

  scheduler_.SetTabCapturePriority(
      &tabs_[0], ThumbnailSchedulerImpl::TabCapturePriority::kNone);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(false, false, true, true));
}

TEST_F(ThumbnailSchedulerImplTest, PreemptsOnPriorityDowngrade) {
  scheduler_.SetTabCapturePriority(
      &tabs_[0], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  scheduler_.SetTabCapturePriority(
      &tabs_[1], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  scheduler_.SetTabCapturePriority(
      &tabs_[2], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, true, false, false));

  scheduler_.SetTabCapturePriority(
      &tabs_[1], ThumbnailSchedulerImpl::TabCapturePriority::kLow);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, false, true, false));
}

TEST_F(ThumbnailSchedulerImplTest, PreemptsOnPriorityUpgrade) {
  scheduler_.SetTabCapturePriority(
      &tabs_[0], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  scheduler_.SetTabCapturePriority(
      &tabs_[1], ThumbnailSchedulerImpl::TabCapturePriority::kLow);
  scheduler_.SetTabCapturePriority(
      &tabs_[2], ThumbnailSchedulerImpl::TabCapturePriority::kLow);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, true, false, false));

  scheduler_.SetTabCapturePriority(
      &tabs_[2], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, false, true, false));
}

TEST_F(ThumbnailSchedulerImplTest,
       LowPriorityCaptureContinuesOnPriorityUpgrade) {
  scheduler_.SetTabCapturePriority(
      &tabs_[0], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  scheduler_.SetTabCapturePriority(
      &tabs_[1], ThumbnailSchedulerImpl::TabCapturePriority::kLow);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, true, false, false));

  scheduler_.SetTabCapturePriority(
      &tabs_[1], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, true, false, false));
}

TEST_F(ThumbnailSchedulerImplTest, CaptureStopsOnPriorityNone) {
  scheduler_.SetTabCapturePriority(
      &tabs_[0], ThumbnailSchedulerImpl::TabCapturePriority::kHigh);
  scheduler_.SetTabCapturePriority(
      &tabs_[1], ThumbnailSchedulerImpl::TabCapturePriority::kLow);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(true, true, false, false));

  scheduler_.SetTabCapturePriority(
      &tabs_[0], ThumbnailSchedulerImpl::TabCapturePriority::kNone);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(false, true, false, false));

  scheduler_.SetTabCapturePriority(
      &tabs_[1], ThumbnailSchedulerImpl::TabCapturePriority::kNone);
  EXPECT_THAT(TabScheduledStates(), ElementsAre(false, false, false, false));
}
