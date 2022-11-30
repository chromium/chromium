// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_capture_driver.h"

#include "base/test/task_environment.h"
#include "chrome/browser/ui/thumbnails/thumbnail_scheduler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockClient : public ThumbnailCaptureDriver::Client {
 public:
  MockClient() = default;

  MOCK_METHOD(void, RequestCapture, (), (override));
  MOCK_METHOD(void, StartCapture, (), (override));
  MOCK_METHOD(void, StopCapture, (), (override));
};

class StubScheduler : public ThumbnailScheduler {
 public:
  StubScheduler() = default;
  ~StubScheduler() override = default;

  TabCapturePriority priority() const { return priority_; }

  // ThumbnailScheduler:
  void AddTab(TabCapturer* tab) override {}
  void RemoveTab(TabCapturer* tab) override {}
  void SetTabCapturePriority(TabCapturer* tab,
                             TabCapturePriority priority) override {
    priority_ = priority;
  }

 private:
  TabCapturePriority priority_ = TabCapturePriority::kNone;
};

class ThumbnailCaptureDriverTest : public ::testing::Test {
 public:
  ThumbnailCaptureDriverTest() = default;
  ~ThumbnailCaptureDriverTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  StubScheduler scheduler_;

  // We strictly specify the interactions between ThumbnailCaptureDriver
  // and its client.
  ::testing::StrictMock<MockClient> mock_client_;
  ThumbnailCaptureDriver capture_driver_{&mock_client_, &scheduler_};
};

}  // namespace

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Expectation;
using ::testing::InSequence;

TEST_F(ThumbnailCaptureDriverTest,
       NoCaptureWhenPageIsVisibleAndThumbnailIsNot) {
  EXPECT_CALL(mock_client_, RequestCapture()).Times(0);
  EXPECT_CALL(mock_client_, StartCapture()).Times(0);
  EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());

  capture_driver_.UpdateThumbnailVisibility(false);
  capture_driver_.UpdatePageVisibility(true);

  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);

  // Simulate a page loading from start to finish
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.SetCanCapture(true);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);
}

TEST_F(ThumbnailCaptureDriverTest,
       CaptureWhenPageIsVisibleAndThumbnailIsRequested) {
  // StopCapture() can be called unnecessarily at first.
  Expectation stop_capture =
      EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());

  // Capture shouldn't start, just requested.
  EXPECT_CALL(mock_client_, StartCapture()).Times(0);

  // Simulate the current page having its thumbnail requested.
  capture_driver_.UpdatePageVisibility(true);
  capture_driver_.UpdateThumbnailVisibility(true);

  // Page becomes sufficiently loaded for capture, but no further, and
  // the client never reports it's ready to capture. This should trigger
  // a RequestCapture() call but nothing more.
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kLow);

  // Capture should only be requested when the scheduler allows it.
  // Additionally ensure the RequestCapture() call is ordered before any
  // StopCapture() calls.
  EXPECT_CALL(mock_client_, RequestCapture()).Times(1).After(stop_capture);
  capture_driver_.SetCapturePermittedByScheduler(true);
}

TEST_F(ThumbnailCaptureDriverTest, NoCaptureWhenPageAndThumbnailAreNotVisible) {
  EXPECT_CALL(mock_client_, RequestCapture()).Times(0);
  EXPECT_CALL(mock_client_, StartCapture()).Times(0);
  EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());

  capture_driver_.UpdateThumbnailVisibility(false);
  capture_driver_.UpdatePageVisibility(false);

  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);

  // Simulate a page loading from start to finish
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.SetCanCapture(true);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);
}

TEST_F(ThumbnailCaptureDriverTest,
       CaptureRequestedWhenPageReadyAndSchedulerAllows) {
  // StopCapture() can be called unnecessarily at first.
  Expectation stop_capture =
      EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());

  // Capture shouldn't start, just requested.
  EXPECT_CALL(mock_client_, StartCapture()).Times(0);

  // The common use case is capturing a thumbnail for a background tab.
  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  // Page becomes sufficiently loaded for capture, but no further, and
  // the client never reports it's ready to capture. This should trigger
  // a RequestCapture() call but nothing more.
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kLow);

  // Capture should only be requested when the scheduler allows it.
  // Additionally ensure the RequestCapture() call is ordered before any
  // StopCapture() calls.
  EXPECT_CALL(mock_client_, RequestCapture()).Times(1).After(stop_capture);
  capture_driver_.SetCapturePermittedByScheduler(true);
}

TEST_F(ThumbnailCaptureDriverTest, CapturesPageWhenPossible) {
  {
    InSequence s;
    EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());
    EXPECT_CALL(mock_client_, RequestCapture());
    EXPECT_CALL(mock_client_, StartCapture());
  }

  // The common use case is capturing a thumbnail for a background tab.
  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kLow);
  capture_driver_.SetCapturePermittedByScheduler(true);
  capture_driver_.SetCanCapture(true);
}

TEST_F(ThumbnailCaptureDriverTest,
       AlwaysWaitsForSchedulerAndCallsRequestCapture) {
  Expectation stop_capture =
      EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());

  // The common use case is capturing a thumbnail for a background tab.
  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.SetCanCapture(true);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kLow);

  Expectation request_capture =
      EXPECT_CALL(mock_client_, RequestCapture()).After(stop_capture);
  EXPECT_CALL(mock_client_, StartCapture()).After(request_capture);
  capture_driver_.SetCapturePermittedByScheduler(true);
}

TEST_F(ThumbnailCaptureDriverTest, FinalCaptureWaitsForScheduler) {
  Expectation stop_capture =
      EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());

  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kLow);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kHigh);

  Expectation request_capture =
      EXPECT_CALL(mock_client_, RequestCapture()).After(stop_capture);
  capture_driver_.SetCapturePermittedByScheduler(true);

  EXPECT_CALL(mock_client_, StartCapture()).After(request_capture);
  capture_driver_.SetCanCapture(true);
}

TEST_F(ThumbnailCaptureDriverTest, StopsCaptureThenResumesFromScheduler) {
  {
    InSequence s;
    EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());
    EXPECT_CALL(mock_client_, RequestCapture());
    EXPECT_CALL(mock_client_, StartCapture());
    EXPECT_CALL(mock_client_, StopCapture()).Times(AtLeast(1));
    EXPECT_CALL(mock_client_, RequestCapture());
    EXPECT_CALL(mock_client_, StartCapture());
  }

  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kLow);
  capture_driver_.SetCapturePermittedByScheduler(true);
  capture_driver_.SetCanCapture(true);

  // Simulate getting descheduled then scheduled again. This should
  // cause capture to stop and restart.
  capture_driver_.SetCapturePermittedByScheduler(false);
  capture_driver_.SetCapturePermittedByScheduler(true);
}

TEST_F(ThumbnailCaptureDriverTest, RestartsCaptureWhenPossible) {
  Expectation stop_capture =
      EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());

  // The common use case is capturing a thumbnail for a background tab.
  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  // Start capture, but then temporarily report we can't capture. When
  // we're able again, we should get another StartCapture() call.
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kLow);
  Expectation request_capture =
      EXPECT_CALL(mock_client_, RequestCapture()).Times(1).After(stop_capture);
  capture_driver_.SetCapturePermittedByScheduler(true);

  Expectation start_capture =
      EXPECT_CALL(mock_client_, StartCapture()).Times(1).After(request_capture);
  capture_driver_.SetCanCapture(true);
  capture_driver_.SetCanCapture(false);

  EXPECT_CALL(mock_client_, StartCapture()).Times(1).After(start_capture);
  capture_driver_.SetCanCapture(true);
}

TEST_F(ThumbnailCaptureDriverTest, StopsOngoingCaptureWhenPageNoLongerReady) {
  {
    InSequence s;
    EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());
    EXPECT_CALL(mock_client_, RequestCapture());
    EXPECT_CALL(mock_client_, StartCapture());
    EXPECT_CALL(mock_client_, StopCapture());
  }

  // The common use case is capturing a thumbnail for a background tab.
  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  capture_driver_.SetCapturePermittedByScheduler(true);
  capture_driver_.SetCanCapture(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);
  capture_driver_.SetCapturePermittedByScheduler(false);
}

TEST_F(ThumbnailCaptureDriverTest, CanContinueCaptureIfPageBecomesVisible) {
  {
    InSequence s;
    EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());
    EXPECT_CALL(mock_client_, RequestCapture());
    EXPECT_CALL(mock_client_, StartCapture());
    EXPECT_CALL(mock_client_, StopCapture()).Times(0);
  }

  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  capture_driver_.SetCapturePermittedByScheduler(true);
  capture_driver_.SetCanCapture(true);

  capture_driver_.UpdatePageVisibility(true);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kLow);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kHigh);
}

TEST_F(ThumbnailCaptureDriverTest, ContinuesCaptureWhenPageBecomesFinal) {
  {
    InSequence s;
    EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());
    EXPECT_CALL(mock_client_, RequestCapture());
    EXPECT_CALL(mock_client_, StartCapture());
  }

  // The common use case is capturing a thumbnail for a background tab.
  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kLow);
  capture_driver_.SetCapturePermittedByScheduler(true);
  capture_driver_.SetCanCapture(true);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kHigh);
}

TEST_F(ThumbnailCaptureDriverTest, StopsCaptureOnFinalFrame) {
  {
    InSequence s;
    EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());
    EXPECT_CALL(mock_client_, RequestCapture());
    EXPECT_CALL(mock_client_, StartCapture());
    EXPECT_CALL(mock_client_, StopCapture()).Times(AtLeast(1));
  }

  // The common use case is capturing a thumbnail for a background tab.
  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  capture_driver_.SetCapturePermittedByScheduler(true);
  capture_driver_.SetCanCapture(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
  capture_driver_.GotFrame();

  task_environment_.FastForwardBy(ThumbnailCaptureDriver::kCooldownDelay);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);
  capture_driver_.SetCapturePermittedByScheduler(false);
}

TEST_F(ThumbnailCaptureDriverTest, RetriesWithinLimits) {
  {
    InSequence s;
    EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());
    EXPECT_CALL(mock_client_, RequestCapture());
    EXPECT_CALL(mock_client_, StartCapture());
  }

  // The common use case is capturing a thumbnail for a background tab.
  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  capture_driver_.SetCapturePermittedByScheduler(true);
  capture_driver_.SetCanCapture(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);

  task_environment_.FastForwardBy(ThumbnailCaptureDriver::kMaxCooldownRetries *
                                  ThumbnailCaptureDriver::kCooldownDelay);
}

TEST_F(ThumbnailCaptureDriverTest, StopsCaptureAtRetryLimit) {
  {
    InSequence s;
    EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());
    EXPECT_CALL(mock_client_, RequestCapture());
    EXPECT_CALL(mock_client_, StartCapture());
    EXPECT_CALL(mock_client_, StopCapture());
  }

  // The common use case is capturing a thumbnail for a background tab.
  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  capture_driver_.SetCapturePermittedByScheduler(true);
  capture_driver_.SetCanCapture(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);

  task_environment_.FastForwardBy(
      (1 + ThumbnailCaptureDriver::kMaxCooldownRetries) *
      ThumbnailCaptureDriver::kCooldownDelay);

  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);
}

TEST_F(ThumbnailCaptureDriverTest, DoesNotReCaptureAfterFinalThumbnail) {
  {
    InSequence s;
    EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());
    EXPECT_CALL(mock_client_, RequestCapture());
    EXPECT_CALL(mock_client_, StartCapture());
    EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());
  }

  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  capture_driver_.SetCapturePermittedByScheduler(true);
  capture_driver_.SetCanCapture(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kHigh);

  capture_driver_.SetCapturePermittedByScheduler(true);
  capture_driver_.GotFrame();
  task_environment_.FastForwardBy(ThumbnailCaptureDriver::kCooldownDelay);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);

  capture_driver_.SetCapturePermittedByScheduler(false);

  capture_driver_.UpdateThumbnailVisibility(false);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);

  capture_driver_.UpdateThumbnailVisibility(true);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);
}

// Going from kReadyForFinalCapture to a lower readiness should always
// invalidate the current thumbnail. Capture should restart from
// scratch. Regression test for https://crbug.com/1137330
TEST_F(ThumbnailCaptureDriverTest, InvalidatesThumbnailOnReadinessDecrease) {
  {
    InSequence s;
    EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());
    EXPECT_CALL(mock_client_, RequestCapture());
    EXPECT_CALL(mock_client_, StartCapture());
    EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());
    EXPECT_CALL(mock_client_, RequestCapture());
    EXPECT_CALL(mock_client_, StartCapture());
  }

  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  capture_driver_.SetCapturePermittedByScheduler(true);
  capture_driver_.SetCanCapture(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kHigh);

  capture_driver_.SetCapturePermittedByScheduler(true);
  capture_driver_.GotFrame();
  task_environment_.FastForwardBy(ThumbnailCaptureDriver::kCooldownDelay);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);

  // This should result in the thumbnail being invalidated. Subsequent
  // loading should trigger capture again.
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kNone);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kLow);

  capture_driver_.SetCapturePermittedByScheduler(true);
  capture_driver_.SetCanCapture(true);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
  EXPECT_EQ(scheduler_.priority(),
            ThumbnailScheduler::TabCapturePriority::kHigh);
}
