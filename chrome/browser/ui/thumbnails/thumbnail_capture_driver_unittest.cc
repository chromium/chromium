// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_capture_driver.h"

#include "base/test/task_environment.h"
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

class ThumbnailCaptureDriverTest : public ::testing::Test {
 public:
  ThumbnailCaptureDriverTest() = default;
  ~ThumbnailCaptureDriverTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // We strictly specify the interactions between ThumbnailCaptureDriver
  // and its client.
  ::testing::StrictMock<MockClient> mock_client_;
  ThumbnailCaptureDriver capture_driver_{&mock_client_};
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

  // Simulate a page loading from start to finish
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.SetCanCapture(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
}

TEST_F(ThumbnailCaptureDriverTest,
       NoCaptureWhenPageIsVisibleAndThumbnailIsRequested) {
  EXPECT_CALL(mock_client_, RequestCapture()).Times(0);
  EXPECT_CALL(mock_client_, StartCapture()).Times(0);
  EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());

  capture_driver_.UpdatePageVisibility(true);
  capture_driver_.UpdateThumbnailVisibility(true);

  // Simulate a page loading from start to finish
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.SetCanCapture(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
}

TEST_F(ThumbnailCaptureDriverTest, NoCaptureWhenPageAndThumbnailAreNotVisible) {
  EXPECT_CALL(mock_client_, RequestCapture()).Times(0);
  EXPECT_CALL(mock_client_, StartCapture()).Times(0);
  EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());

  capture_driver_.UpdateThumbnailVisibility(false);
  capture_driver_.UpdatePageVisibility(false);

  // Simulate a page loading from start to finish
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.SetCanCapture(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
}

TEST_F(ThumbnailCaptureDriverTest, CaptureRequestedWhenPageReady) {
  // StopCapture() can be called unnecessarily at first.
  Expectation stop_capture =
      EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());

  // This test should not trigger capture.
  EXPECT_CALL(mock_client_, StartCapture()).Times(0);

  // The common use case is capturing a thumbnail for a background tab.
  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  // Page becomes sufficiently loaded for capture, but no further, and
  // the client never reports it's ready to capture. This should trigger
  // a RequestCapture() call but nothing more.
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);

  // Ensure the RequestCapture() call is ordered before any StopCapture() calls.
  EXPECT_CALL(mock_client_, RequestCapture()).Times(1).After(stop_capture);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
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
  capture_driver_.SetCanCapture(true);
}

TEST_F(ThumbnailCaptureDriverTest, RequestCaptureEvenIfAbleEarlier) {
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
  capture_driver_.SetCanCapture(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
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

  Expectation request_capture =
      EXPECT_CALL(mock_client_, RequestCapture()).Times(1).After(stop_capture);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);

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
  capture_driver_.SetCanCapture(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
}

TEST_F(ThumbnailCaptureDriverTest, StopsCaptureIfPageBecomesVisible) {
  {
    InSequence s;
    EXPECT_CALL(mock_client_, StopCapture()).Times(AnyNumber());
    EXPECT_CALL(mock_client_, RequestCapture());
    EXPECT_CALL(mock_client_, StartCapture());
    EXPECT_CALL(mock_client_, StopCapture()).Times(AtLeast(1));
  }

  capture_driver_.UpdateThumbnailVisibility(true);
  capture_driver_.UpdatePageVisibility(false);

  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kNotReady);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForInitialCapture);
  capture_driver_.SetCanCapture(true);

  capture_driver_.UpdatePageVisibility(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
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
  capture_driver_.SetCanCapture(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
}

TEST_F(ThumbnailCaptureDriverTest, StopsCaptureOnFinalFrame) {
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
  capture_driver_.SetCanCapture(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);
  capture_driver_.GotFrame();

  task_environment_.FastForwardBy(ThumbnailCaptureDriver::kCooldownDelay);
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
  capture_driver_.SetCanCapture(true);
  capture_driver_.UpdatePageReadiness(
      ThumbnailReadinessTracker::Readiness::kReadyForFinalCapture);

  task_environment_.FastForwardBy(
      (1 + ThumbnailCaptureDriver::kMaxCooldownRetries) *
      ThumbnailCaptureDriver::kCooldownDelay);
}
